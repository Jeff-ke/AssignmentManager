#ifndef GOOGLEAUTHMANAGER_H
#define GOOGLEAUTHMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTcpServer>
#include <QTimer>
#include <QNetworkReply>

class GoogleAuthManager : public QObject
{
    Q_OBJECT

public:
    explicit GoogleAuthManager(QObject *parent = nullptr);
    ~GoogleAuthManager();

    // Public methods
    void startOAuthProcess();
    void requestAccessToken(const QString &authCode);
    void refreshAccessToken();
    void logout();

    // Getters
    bool isLoggedIn() const { return m_isLoggedIn; }
    QString accessToken() const { return m_accessToken; }
    QString refreshToken() const { return m_refreshToken; }
    QDateTime tokenExpiryTime() const { return m_tokenExpiryTime; }

signals:
    // Signals to notify the main application about auth events
    void loginStatusChanged(bool isLoggedIn);
    void accessTokenReceived();
    void authenticationError(const QString &errorMessage);
    void userLoggedOut();
    void tokenRefreshed();

private slots:
    void onReadyRead();
    void onAccessTokenReceived();
    void scheduleTokenRefresh(int secondsUntilRefresh);

private:
    // Constants
    const QString CLIENT_ID = "GOOGLE_CLIENT_ID";
    const QString CLIENT_SECRET = "GOOGLE_CLIENT_SECRET";
    const QString REDIRECT_URI = "http://localhost:8080";
    const QString TOKEN_URL = "https://oauth2.googleapis.com/token";
    const QString AUTH_URL = "https://accounts.google.com/o/oauth2/v2/auth";

    // Member variables
    QTcpServer *m_server;
    QTimer *m_tokenRefreshTimer;
    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_tokenExpiryTime;
    bool m_isLoggedIn;
};

#endif // GOOGLEAUTHMANAGER_H
