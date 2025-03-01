#include "GoogleAuthManager.h"
#include <QDesktopServices>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QDebug>
#include <QTcpSocket>
#include <QMetaObject>

GoogleAuthManager::GoogleAuthManager(QObject *parent)
    : QObject(parent),
    m_server(new QTcpServer(this)),
    m_tokenRefreshTimer(nullptr),
    m_isLoggedIn(false)
{
    // Set up the local server for OAuth callback
    connect(m_server, &QTcpServer::newConnection, this, &GoogleAuthManager::onReadyRead);
    if (!m_server->listen(QHostAddress::Any, 8080)) {
        qWarning() << "Failed to start server on port 8080:" << m_server->errorString();
    }
}

GoogleAuthManager::~GoogleAuthManager()
{
    if (m_server) {
        m_server->close();
    }

    if (m_tokenRefreshTimer) {
        m_tokenRefreshTimer->stop();
        delete m_tokenRefreshTimer;
    }
}

void GoogleAuthManager::startOAuthProcess()
{
    qDebug() << "Starting OAuth process";

    QString authUrl = QString("%1"
                              "?scope=https://www.googleapis.com/auth/drive%%20openid%%20https://www.googleapis.com/auth/userinfo.email%%20https://www.googleapis.com/auth/userinfo.profile"
                              "&access_type=offline"
                              "&include_granted_scopes=true"
                              "&response_type=code"
                              "&prompt=consent"
                              "&redirect_uri=%2"
                              "&client_id=%3")
                          .arg(AUTH_URL)
                          .arg(REDIRECT_URI)
                          .arg(CLIENT_ID);

    qDebug() << "Generated Authorization URL: " << authUrl;
    QDesktopServices::openUrl(QUrl(authUrl));
}

void GoogleAuthManager::requestAccessToken(const QString &authCode)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl url(TOKEN_URL);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray postData;
    postData.append("code=" + authCode.toUtf8() + "&");
    postData.append("client_id=" + CLIENT_ID.toUtf8() + "&");
    postData.append("client_secret=" + CLIENT_SECRET.toUtf8() + "&");
    postData.append("redirect_uri=" + REDIRECT_URI.toUtf8() + "&");
    postData.append("grant_type=authorization_code");

    QNetworkReply *reply = manager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, &GoogleAuthManager::onAccessTokenReceived);
}

void GoogleAuthManager::refreshAccessToken()
{
    if (m_refreshToken.isEmpty()) {
        qWarning() << "No refresh token available";
        return;
    }

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl url(TOKEN_URL);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray postData;
    postData.append("client_id=" + CLIENT_ID.toUtf8() + "&");
    postData.append("client_secret=" + CLIENT_SECRET.toUtf8() + "&");
    postData.append("refresh_token=" + m_refreshToken.toUtf8() + "&");
    postData.append("grant_type=refresh_token");

    QNetworkReply *reply = manager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            QJsonObject jsonObject = jsonResponse.object();

            m_accessToken = jsonObject["access_token"].toString();
            int expiresIn = jsonObject["expires_in"].toInt();
            m_tokenExpiryTime = QDateTime::currentDateTime().addSecs(expiresIn);

            qDebug() << "Access token refreshed. New token:" << m_accessToken;
            qDebug() << "Token expires at:" << m_tokenExpiryTime.toString();

            scheduleTokenRefresh(expiresIn - 120); // Refresh 2 minutes before expiry

            // Emit signal to notify that token was refreshed
            emit tokenRefreshed();
        } else {
            qWarning() << "Error refreshing token:" << reply->errorString();
            emit authenticationError("Failed to refresh access token: " + reply->errorString());
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

void GoogleAuthManager::scheduleTokenRefresh(int secondsUntilRefresh)
{
    if (m_tokenRefreshTimer) {
        m_tokenRefreshTimer->stop();
    } else {
        m_tokenRefreshTimer = new QTimer(this);
        connect(m_tokenRefreshTimer, &QTimer::timeout, this, &GoogleAuthManager::refreshAccessToken);
    }

    m_tokenRefreshTimer->start(secondsUntilRefresh * 1000);
    qDebug() << "Token refresh scheduled in " << secondsUntilRefresh << " seconds.";
}

void GoogleAuthManager::onAccessTokenReceived()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QNetworkAccessManager *manager = reply->manager();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        QJsonObject jsonObject = jsonResponse.object();

        m_accessToken = jsonObject["access_token"].toString();
        m_refreshToken = jsonObject["refresh_token"].toString();
        int expiresIn = jsonObject["expires_in"].toInt();

        m_tokenExpiryTime = QDateTime::currentDateTime().addSecs(expiresIn);

        // Update login state
        m_isLoggedIn = true;

        qDebug() << "Access Token: " << m_accessToken;
        qDebug() << "Token Expires At: " << m_tokenExpiryTime.toString();
        qDebug() << "Login successful.";

        scheduleTokenRefresh(expiresIn - 120);

        // Emit signals
        emit loginStatusChanged(true);
        emit accessTokenReceived();
    } else {
        qWarning() << "Error fetching access token: " << reply->errorString();
        m_isLoggedIn = false;

        // Emit error signal
        emit authenticationError("Authentication failed: " + reply->errorString());
        emit loginStatusChanged(false);
    }

    reply->deleteLater();
    manager->deleteLater();
}

void GoogleAuthManager::onReadyRead()
{
    QTcpSocket *socket = m_server->nextPendingConnection();

    connect(socket, &QTcpSocket::readyRead, this, [socket, this]() {
        QByteArray request = socket->readAll();
        qDebug() << "Full HTTP Request: " << request;

        QString requestString = QString::fromUtf8(request);

        static const QRegularExpression regExp("GET\\s(\\/.*)\\sHTTP");
        QRegularExpressionMatch match = regExp.match(requestString);

        if (match.hasMatch()) {
            QString queryString = match.captured(1);
            QUrl url("http://localhost" + queryString);
            QUrlQuery query(url);

            if (query.hasQueryItem("code")) {
                QString authCode = query.queryItemValue("code");
                qDebug() << "Extracted Authorization Code: " << authCode;

                QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                      "<html><body><h1>Authentication Successful!</h1>"
                                      "<h3>You may close this window and proceed to the desktop application "
                                      "to view your files in your selected drive and analyze the files therein.</h3>"
                                      "</body></html>";
                socket->write(response);
                socket->disconnectFromHost();

                // Request access token with the auth code
                requestAccessToken(authCode);
            } else {
                QByteArray response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
                                      "<html><body><h1>Authorization failed.</h1><h3>Try again.</h3></body></html>";
                socket->write(response);
                socket->disconnectFromHost();
            }
        }
    });

    // Handle socket disconnection
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void GoogleAuthManager::logout()
{
    // Clear session variables
    m_accessToken.clear();
    m_refreshToken.clear();

    if (m_tokenRefreshTimer) {
        m_tokenRefreshTimer->stop();
    }

    // Reset login state
    m_isLoggedIn = false;

    // Emit signals
    emit userLoggedOut();
    emit loginStatusChanged(false);

    qDebug() << "User logged out successfully.";
}
