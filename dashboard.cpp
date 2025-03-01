#include "./ui_Dashboard.h"
#include "Dashboard.h"
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDebug>
#include <QProgressDialog>
#include <QMessageBox>
#include <QDir>
#include <QTextDocument>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <poppler-qt6.h>
#include <memory>

Dashboard::Dashboard(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::Dashboard),
    tokenRefreshTimer(nullptr),
    selectedFolderId(""),
    isLoggedIn(false),
    progressDialog(nullptr),
    showingHighSimilarityOnly(false)

{
    ui->setupUi(this);

    ui->search_lineEdit->setPlaceholderText("Search files...");
    ui->search_lineEdit->setStyleSheet("QLineEdit::placeholder { color: grey; }");

    ui->deletedAssignments_pushButton->setText("Show High Similarity Only");
    ui->deletedAssignments_pushButton->setToolTip("Click to show only files with similarity > 50%");

    // Make sure combobox is visible and enabled
    ui->folderComboBox->setVisible(true);
    ui->folderComboBox->setEnabled(true);
    ui->folderComboBox->setEditable(false);
    ui->assignments_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->assignments_tableWidget->setSortingEnabled(true);

    // Initialize qlabel for class score summaries
    ui->class_average_label->setText("0.00%");
    ui->class_average_label->setStyleSheet("color: white; font-weight: bold;");

    // Clearin combo box, just incase. and add initial item
    ui->folderComboBox->clear();
    ui->folderComboBox->addItem("Select a folder", "");

    // Settin up the server
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(onReadyRead()));
    server->listen(QHostAddress::Any, 8080);

    // Settin up UI elements
    updateDateLabel();
    startDateTimer();
    setAppLogo();

    QTimer* searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300); // 300ms delay

    // Connecting the search box to the timer
    connect(ui->search_lineEdit, &QLineEdit::textChanged, this, [this, searchTimer]() {
        searchTimer->start(); // Restart timer on each text change
    });

    // Connecting timer to the search function
    connect(searchTimer, &QTimer::timeout, this, [this]() {
        filterAssignmentsBySearch(ui->search_lineEdit->text());
    });

    // Connecting enter/return key press - using the enter keyboard button for validation.
    connect(ui->search_lineEdit, &QLineEdit::returnPressed, this, [this]() {
        filterAssignmentsBySearch(ui->search_lineEdit->text());
    });

    connect(ui->downloadAll_pushButton, &QPushButton::clicked,
            this, &Dashboard::downloadAllFiles);

    connect(ui->folderComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_folderComboBox_currentIndexChanged(int)));

    connect(ui->checkPlagiarism_pushButton, &QPushButton::clicked,
            this, &Dashboard::compareFiles);

    connect(ui->refresh_pushButton, SIGNAL(clicked()),
            this, SLOT(on_refresh_pushButton_clicked()));

    connect(ui->deletedAssignments_pushButton, SIGNAL(clicked()),
            this, SLOT(on_deletedAssignments_pushButton_clicked()));

    connect(ui->deletedAssignments_pushButton, &QPushButton::clicked,
            this, &Dashboard::on_deletedAssignments_pushButton_clicked);

    //debugging setup - just to make sure all connections are made.
    qDebug() << "Dashboard constructed. Button connected.";

    // Setting initial button text
    ui->login_pushButton->setText("Login");
}

Dashboard::~Dashboard()
{
    delete ui;
    delete server;
    delete progressDialog;

    if (tokenRefreshTimer) {
        tokenRefreshTimer->stop();
        delete tokenRefreshTimer;
    }
}
// extra code just for debuggin - purpos
void Dashboard::startOAuthProcess()
{
    qDebug() << "startOAuthProcess called from:" << QObject::sender();
    QString authUrl = QString("https://accounts.google.com/o/oauth2/v2/auth"
                              "?scope=https://www.googleapis.com/auth/drive%20openid%20https://www.googleapis.com/auth/userinfo.email%20https://www.googleapis.com/auth/userinfo.profile"
                              "&access_type=offline"
                              "&include_granted_scopes=true"
                              "&response_type=code"
                              "&prompt=consent"
                              "&redirect_uri=http://localhost:8080"
                              "&client_id=%1")
                          .arg("GOOGLE_CLIENT_ID");
    qDebug() << "Generated Authorization URL: " << authUrl;
    QDesktopServices::openUrl(QUrl(authUrl));
}

void Dashboard::requestAccessToken(const QString &authCode, const QString &folderId)
{
    QString clientId = "GOOGLE_CLIENT_ID";
    QString clientSecret = "GOOGLE_CLIENT_SECRET";
    QString redirectUri = "http://localhost:8080";
    QString tokenUrl = "https://oauth2.googleapis.com/token";

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl url(tokenUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray postData;
    postData.append("code=" + authCode.toUtf8() + "&");
    postData.append("client_id=" + clientId.toUtf8() + "&");
    postData.append("client_secret=" + clientSecret.toUtf8() + "&");
    postData.append("redirect_uri=" + redirectUri.toUtf8() + "&");
    postData.append("grant_type=authorization_code");

    QNetworkReply *reply = manager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, &Dashboard::onAccessTokenReceived);
}

void Dashboard::refreshAccessToken()
{
    if (refreshToken.isEmpty()) {
        qWarning() << "No refresh token available";
        return;
    }

    const QString clientId = qgetenv("GOOGLE_CLIENT_ID");
    const QString clientSecret = qgetenv("GOOGLE_CLIENT_SECRET");


    QString tokenUrl = "https://oauth2.googleapis.com/token";

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl url(tokenUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray postData;
    postData.append("client_id=" + clientId.toUtf8() + "&");
    postData.append("client_secret=" + clientSecret.toUtf8() + "&");
    postData.append("refresh_token=" + refreshToken.toUtf8() + "&");
    postData.append("grant_type=refresh_token");

    QNetworkReply *reply = manager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            QJsonObject jsonObject = jsonResponse.object();

            accessToken = jsonObject["access_token"].toString();
            int expiresIn = jsonObject["expires_in"].toInt();
            tokenExpiryTime = QDateTime::currentDateTime().addSecs(expiresIn);

            qDebug() << "Access token refreshed. New token:" << accessToken;
            qDebug() << "Token expires at:" << tokenExpiryTime.toString();

            scheduleTokenRefresh(expiresIn - 120); // Refresh 2 minutes before expiry
        } else {
            qWarning() << "Error refreshing token:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void Dashboard::scheduleTokenRefresh(int secondsUntilRefresh)
{
    if (tokenRefreshTimer)
    {
        tokenRefreshTimer->stop();
    }
    else
    {
        tokenRefreshTimer = new QTimer(this);
        connect(tokenRefreshTimer, &QTimer::timeout, this, &Dashboard::refreshAccessToken);
    }

    tokenRefreshTimer->start(secondsUntilRefresh * 1000);
    qDebug() << "Token refresh scheduled in " << secondsUntilRefresh << " seconds.";
}

void Dashboard::onAccessTokenReceived()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray response = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        QJsonObject jsonObject = jsonResponse.object();

        accessToken = jsonObject["access_token"].toString();
        refreshToken = jsonObject["refresh_token"].toString();
        int expiresIn = jsonObject["expires_in"].toInt();

        tokenExpiryTime = QDateTime::currentDateTime().addSecs(expiresIn);

        // Update login state first
        isLoggedIn = true;

        // Update UI on the main thread
        QMetaObject::invokeMethod(this, [this]() {
            ui->login_pushButton->setText("Logout");
            ui->login_pushButton->setEnabled(true);
        }, Qt::QueuedConnection);

        qDebug() << "Access Token: " << accessToken;
        qDebug() << "Token Expires At: " << tokenExpiryTime.toString();
        qDebug() << "Login successful. Button updated to 'Logout'.";

        scheduleTokenRefresh(expiresIn - 120);
        fetchDriveFolders();
        fetchUserProfile();
    }
    else
    {
        qWarning() << "Error fetching access token: " << reply->errorString();
        // Update UI to show error state
        QMetaObject::invokeMethod(this, [this]() {
            ui->login_pushButton->setEnabled(true);
            ui->login_pushButton->setText("Login");
            isLoggedIn = false;
        }, Qt::QueuedConnection);
    }
    reply->deleteLater();
}

void Dashboard::onReadyRead()
{
    QTcpSocket *socket = server->nextPendingConnection();
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
                                      "<html><body><h1>Authentication Successful!</h1><h3>Youu may close this window and proceed to the desktop application to view your files in your selected drive and Analyze the files therein.</h3></body></html>";
                socket->write(response);
                socket->disconnectFromHost();

                // Call with default empty folder ID
                requestAccessToken(authCode, QString());
            } else {
                QByteArray response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
                                      "<html><body><h1>Authorization failed.</h1><h3>Try again.</h3></body></html>";
                socket->write(response);
                socket->disconnectFromHost();
            }
        }
    });
}

void Dashboard::logout()
{
    // Clear session variables
    accessToken.clear();
    refreshToken.clear();
    downloadedFiles.clear();
    updatePlagiarismButtonState();


    resetClassAverage();
    showingHighSimilarityOnly = false;
    originalAssignments.clear();



    if (tokenRefreshTimer) {
        tokenRefreshTimer->stop();
    }

    // Reset login state
    isLoggedIn = false;

    // Update UI elements
    ui->login_pushButton->setText("Login");
    ui->login_pushButton->setEnabled(true);
    ui->rejected_assignments_label->setText("Flagged Files: 0");

    ui->class_average_label->setText("0.00%");
    ui->class_average_label->setStyleSheet("color: white; font-weight: bold;");

    // Clear other UI elements
    ui->folderComboBox->clear();
    ui->folderComboBox->addItem("Select a folder", "");
    ui->assignments_tableWidget->setRowCount(0);
    ui->total_submitted_assignments_label->setText("Total Files: 0");
    ui->logged_in_as_Label->setText("Not logged in");
    qDebug() << "User logged out. Button updated to 'Login'.";

    if (ui->assignments_tableWidget->columnCount() >= 5) {
        for (int row = 0; row < ui->assignments_tableWidget->rowCount(); ++row) {
            if (ui->assignments_tableWidget->item(row, 4)) {
                ui->assignments_tableWidget->item(row, 4)->setText("0.00%");
                ui->assignments_tableWidget->item(row, 4)->setBackground(Qt::white);
            }
        }
    }
}

void Dashboard::verifyState() {
    qDebug() << "\n=== CURRENT STATE ===";
    qDebug() << "showingHighSimilarityOnly:" << showingHighSimilarityOnly;
    qDebug() << "originalAssignments size:" << originalAssignments.size();
    qDebug() << "Current table rows:" << ui->assignments_tableWidget->rowCount();
    qDebug() << "Button text:" << ui->deletedAssignments_pushButton->text();
}

void Dashboard::setAppLogo()
{
    QPixmap logo("://Logo.png");
    if (!logo.isNull()) {
        logo = logo.scaled(75, 75, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->App_logo->setPixmap(logo);
        //ui->App_logo->setScaledContents(true);
    } else {
        qWarning() << "Failed to load the logo image!";
    }
}

void Dashboard::updateDateLabel()
{
    QDate currentDate = QDate::currentDate();
    QString formattedDate = currentDate.toString("MMM d, yyyy");
    ui->date_label->setText(formattedDate);
}

void Dashboard::startDateTimer()
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Dashboard::updateDateLabel);
    timer->start(60000);
}

void Dashboard::fetchDriveFolders()
{
    QString query = "mimeType='application/vnd.google-apps.folder' and trashed=false";

    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("q", query);
    urlQuery.addQueryItem("fields", "files(id,name)");
    url.setQuery(urlQuery);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, &Dashboard::onDriveFoldersFetched);
}

void Dashboard::fetchUserProfile() {
    qDebug() << "Fetching user profile...";
    QUrl url("https://www.googleapis.com/oauth2/v1/userinfo?alt=json");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, &Dashboard::onUserProfileFetched);
}

void Dashboard::onUserProfileFetched() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        QJsonObject jsonObject = jsonResponse.object();

        QString fullName = jsonObject["name"].toString();
        QString firstName = fullName.split(" ").first();

        // Update UI on the main thread
        QMetaObject::invokeMethod(this, [this, firstName]() {
            ui->logged_in_as_Label->setText("Logged in as: " + firstName);
        }, Qt::QueuedConnection);
    }
    reply->deleteLater();
}

void Dashboard::fetchDriveFiles(const QString &folderId)
{
    QString query;
    if (folderId.isEmpty()) {
        query = "trashed=false and mimeType!='application/vnd.google-apps.folder'";
    } else {
        query = QString("'%1' in parents and trashed=false and mimeType!='application/vnd.google-apps.folder'").arg(folderId);
    }

    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("q", query);
    urlQuery.addQueryItem("fields", "files(id,name,mimeType,modifiedTime,size)");
    urlQuery.addQueryItem("pageSize", "100");
    url.setQuery(urlQuery);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, &Dashboard::onDriveFilesFetched);
}

void Dashboard::onDriveFilesFetched() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        QJsonObject jsonObject = jsonResponse.object();

        // Clear existing table contents and stored assignments
        ui->assignments_tableWidget->setRowCount(0);
        originalAssignments.clear();

        QJsonArray files = jsonObject["files"].toArray();

        if (files.isEmpty()) {
            qDebug() << "No files found in the folder.";
            ui->total_submitted_assignments_label->setText("Total Files: 0");
            reply->deleteLater();
            return;
        }

        int totalFiles = files.size();
        ui->total_submitted_assignments_label->setText(QString("Total Files: %1").arg(totalFiles));

        for (const QJsonValue &value : files) {
            QJsonObject file = value.toObject();
            QString fileId = file["id"].toString();
            QString fileName = file["name"].toString();
            QString mimeType = file["mimeType"].toString();

            QDateTime modifiedTime = QDateTime::fromString(
                file["modifiedTime"].toString(), Qt::ISODate);
            QString formattedTime = modifiedTime.toString("yyyy-MM-dd hh:mm:ss");

            int rowCount = ui->assignments_tableWidget->rowCount();
            ui->assignments_tableWidget->insertRow(rowCount);

            auto dateItem = new QTableWidgetItem(formattedTime);
            auto nameItem = new QTableWidgetItem(fileName);
            nameItem->setData(Qt::UserRole, fileId); // Store fileId for later use
            auto typeItem = new QTableWidgetItem(mimeType);

            ui->assignments_tableWidget->setItem(rowCount, 0, dateItem);
            ui->assignments_tableWidget->setItem(rowCount, 1, nameItem);
            ui->assignments_tableWidget->setItem(rowCount, 2, typeItem);

            QPushButton *downloadButton = new QPushButton("Download", ui->assignments_tableWidget);
            ui->assignments_tableWidget->setCellWidget(rowCount, 3, downloadButton);

            connect(downloadButton, &QPushButton::clicked, this, [this, fileId, fileName]() {
                downloadFile(fileId, fileName);
            });

            // Add empty similarity column
            auto similarityItem = new QTableWidgetItem("0.00%");
            similarityItem->setBackground(Qt::darkGray);
            ui->assignments_tableWidget->setItem(rowCount, 4, similarityItem);
        }
    }
    reply->deleteLater();
}

void Dashboard::downloadFile(const QString &fileId, const QString &fileName)
{
    QString apiUrl = QString("https://www.googleapis.com/drive/v3/files/%1?alt=media")
    .arg(fileId);
    QUrl url(apiUrl);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId, fileName]() {
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray response = reply->readAll();
            QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileName;
            QFile file(downloadPath);
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(response);
                file.close();
                qDebug() << "File downloaded: " << fileName;

                // Store file content for plagiarism detection
                FileContent fileContent;
                fileContent.id = fileId;
                fileContent.name = fileName;
                fileContent.content = extractTextFromFile(downloadPath);
                downloadedFiles.append(fileContent);
                updatePlagiarismButtonState();
            }
            else
            {
                qWarning() << "Failed to save file!";
            }
        }
        else
        {
            qWarning() << "Failed to download file: " << reply->errorString();
        }
        reply->deleteLater();
    });
}

void Dashboard::on_login_pushButton_clicked()
{
    if (isLoggedIn) {
        logout();
    } else {
          ui->login_pushButton->setEnabled(false);
        startOAuthProcess();
    }
}

void Dashboard::on_refresh_pushButton_clicked()
{
    QString selectedId = ui->folderComboBox->currentData().toString();
    if (selectedId.isEmpty()) {
        QMessageBox::warning(this, "Selection Required", "Please select a folder first.");
        return;
    }

    // Clear existing data
    ui->assignments_tableWidget->setRowCount(0);
    ui->total_submitted_assignments_label->setText("Total Files: 0");
    ui->rejected_assignments_label->setText("Flagged Files: 0");
    downloadedFiles.clear();
    updatePlagiarismButtonState();
    resetClassAverage();
    showingHighSimilarityOnly = false;
    originalAssignments.clear();

    // Fetch files for selected folder
    fetchDriveFiles(selectedId);
}

void Dashboard::on_folderComboBox_currentIndexChanged(int index)
{
    QString newFolderId = ui->folderComboBox->currentData().toString();
    qDebug() << "ComboBox selection changed to index:" << index
             << "with folder ID:" << newFolderId;

    if (newFolderId != selectedFolderId) {
        selectedFolderId = newFolderId;
        // Remove the automatic file fetching from here
        if (selectedFolderId.isEmpty()) {
            ui->assignments_tableWidget->setRowCount(0);
            ui->total_submitted_assignments_label->setText("Total Files: 0");
        }
    }
}

void Dashboard::onDriveFoldersFetched()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray response = reply->readAll();
        qDebug() << "Received folder response:" << response; // Add this debug line

        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        QJsonObject jsonObject = jsonResponse.object();
        QJsonArray folders = jsonObject["files"].toArray();

        qDebug() << "Number of folders found:" << folders.size(); // Add this debug line

        // Store current selection
        QString currentSelection = ui->folderComboBox->currentData().toString();

        // Clear and populate the combo box
        ui->folderComboBox->clear();
        ui->folderComboBox->addItem("Select a folder", "");

        for (const QJsonValue &value : folders)
        {
            QJsonObject folder = value.toObject();
            QString folderId = folder["id"].toString();
            QString folderName = folder["name"].toString();
            qDebug() << "Adding folder:" << folderName << "with ID:" << folderId; // Add this debug line
            ui->folderComboBox->addItem(folderName, folderId);

            // Restore previous selection if it exists
            if (folderId == currentSelection) {
                ui->folderComboBox->setCurrentIndex(ui->folderComboBox->count() - 1);
            }
        }
    }
    else
    {
        qWarning() << "Error fetching folders: " << reply->errorString();
        qDebug() << "Error details:" << reply->readAll(); // Add this debug line
    }
    reply->deleteLater();
}

QString Dashboard::extractTextFromFile(const QString& filePath)
{
    QFile file(filePath);
    QString fileExtension = QFileInfo(filePath).suffix().toLower();

    if (fileExtension == "pdf") {
        std::unique_ptr<Poppler::Document> document(Poppler::Document::load(filePath));
        if (!document || document->isLocked()) {
            qWarning() << "Failed to load or decrypt PDF:" << filePath;
            return QString();
        }

        QString content;
        for (int i = 0; i < document->numPages(); ++i) {
            std::unique_ptr<Poppler::Page> page(document->page(i));
            if (page) {
                content += page->text(QRectF());
                content += "\n"; // Add separator between pages
            }
        }

        return content;
    }
    else {
        // Original text file handling
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();

        QString content;
        QTextStream in(&file);
        content = in.readAll();
        file.close();

        return content;
    }
}

QStringList Dashboard::preprocessText(const QString& text)
{
    // Convert to lowercase and split into words
    QString processed = text.toLower();
    // Remove punctuation
    processed.replace(QRegularExpression("[.,!?;:'\"()\\[\\]{}]"), " ");
    // Split into words and remove empty strings
    QStringList words = processed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    return words;
}

double Dashboard::calculateSimilarity(const QString& text1, const QString& text2)
{
    if (text1.isEmpty() || text2.isEmpty()) {
        return 0.0;
    }

    QStringList words1 = preprocessText(text1);
    QStringList words2 = preprocessText(text2);

    if (words1.isEmpty() || words2.isEmpty()) {
        return 0.0;
    }

    // Create sets of words
    QSet<QString> set1 = QSet<QString>(words1.begin(), words1.end());
    QSet<QString> set2 = QSet<QString>(words2.begin(), words2.end());

    // Calculate Jaccard similarity coefficient
    QSet<QString> intersection = set1.intersect(set2);
    QSet<QString> union_set = set1.unite(set2);

    double similarity = (double)intersection.size() / union_set.size() * 100.0;

    qDebug() << "Similarity between files:" << similarity << "%";
    return similarity;
}

void Dashboard::compareFiles()
{
    qDebug() << "compareFiles() called with" << downloadedFiles.size() << "files";


    if (downloadedFiles.size() < 2) {
        QMessageBox::warning(this, "Not Enough Files", "Please download at least two files to compare.");
        return;
    }

    // Delete existing progress dialog if any
    if (progressDialog) {
        delete progressDialog;
        progressDialog = nullptr;
    }

    // Create new progress dialog
    progressDialog = new QProgressDialog("Analyzing files for plagiarism...", "Cancel", 0,
                                         (downloadedFiles.size() * (downloadedFiles.size() - 1)) / 2, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();




    if (downloadedFiles.size() < 2) {
        QMessageBox::warning(this, "Not Enough Files",
                             "Please download at least two files to compare.");
        qDebug() << "Not enough files for comparison";
        return;
    }

    // Store current assignments before comparison
    QVector<AssignmentData> tempAssignments = originalAssignments;

    // Calculate total comparisons needed
    int totalComparisons = (downloadedFiles.size() * (downloadedFiles.size() - 1)) / 2;
    qDebug() << "Total comparisons to be made:" << totalComparisons;

    // Initialize progress dialog
    if (progressDialog) {
        delete progressDialog;
    }
    progressDialog = new QProgressDialog("Analyzing files for plagiarism...",
                                         "Cancel", 0, totalComparisons, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    // Map to store highest similarities
    QMap<QString, double> highestSimilarities;
    similarityResults.clear();

    // Initialize similarities
    for (const auto& file : downloadedFiles) {
        highestSimilarities[file.name] = 0.0;
        qDebug() << "Initialized similarity for file:" << file.name;
    }

    int progress = 0;
    double totalSimilarity = 0.0;
    int significantComparisons = 0;

    // Compare files
    for (int i = 0; i < downloadedFiles.size() - 1; ++i) {
        for (int j = i + 1; j < downloadedFiles.size(); ++j) {
            if (progressDialog->wasCanceled()) {
                qDebug() << "Comparison cancelled by user";
                delete progressDialog;
                progressDialog = nullptr;
                return;
            }

            qDebug() << "Comparing files:" << downloadedFiles[i].name
                     << "and" << downloadedFiles[j].name;

            double similarity = calculateSimilarity(
                downloadedFiles[i].content,
                downloadedFiles[j].content
                );

            qDebug() << "Similarity score:" << similarity;

            if (similarity > 0.0) {
                totalSimilarity += similarity;
                significantComparisons++;
            }

            // Update highest similarities
            highestSimilarities[downloadedFiles[i].name] =
                qMax(highestSimilarities[downloadedFiles[i].name], similarity);
            highestSimilarities[downloadedFiles[j].name] =
                qMax(highestSimilarities[downloadedFiles[j].name], similarity);

            if (similarity > 20.0) {
                similarityResults.append({
                    {downloadedFiles[i].name, downloadedFiles[j].name},
                    similarity
                });
            }

            progressDialog->setValue(++progress);
            QApplication::processEvents();
        }
    }

    if (progressDialog) {
        delete progressDialog;
        progressDialog = nullptr;
    }

    // Calculate and update results
    double averageSimilarity = significantComparisons > 0 ?
                                   totalSimilarity / significantComparisons : 0.0;

    qDebug() << "Final average similarity:" << averageSimilarity;
    qDebug() << "Total significant comparisons:" << significantComparisons;

    // Restore assignments and update display
    originalAssignments = tempAssignments;
    updateSimilarityDisplay(highestSimilarities, averageSimilarity);

    // Apply current filter state after updating similarities
    filterAssignmentsBySimilarity(showingHighSimilarityOnly);
    showPlagiarismResults(similarityResults);

}

void Dashboard::updateSimilarityDisplay(const QMap<QString, double>& highestSimilarities,
                                        double averageSimilarity)
{
    // Clear previous assignments
    originalAssignments.clear();

    // Update UI elements
    ui->class_average_label->setText(QString("%1%").arg(QString::number(averageSimilarity, 'f', 2)));
    ui->class_average_label->setStyleSheet(
        averageSimilarity > 50.0 ?
            "color: red; font-weight: bold;" :
            "color: white; font-weight: bold;"
        );

    int highSimilarityCount = 0;

    // Store all assignments with their similarity scores
    for (int row = 0; row < ui->assignments_tableWidget->rowCount(); ++row) {
        QTableWidgetItem* dateItem = ui->assignments_tableWidget->item(row, 0);
        QTableWidgetItem* nameItem = ui->assignments_tableWidget->item(row, 1);
        QTableWidgetItem* typeItem = ui->assignments_tableWidget->item(row, 2);

        if (!dateItem || !nameItem || !typeItem) {
            continue;
        }

        QString fileName = nameItem->text();
        double similarity = highestSimilarities.value(fileName, 0.0);

        if (similarity > 50.0) {
            highSimilarityCount++;
        }

        // Store complete assignment data
        AssignmentData data;
        data.dateModified = dateItem->text();
        data.fileName = fileName;
        data.fileType = typeItem->text();
        data.similarity = similarity;
        data.fileId = nameItem->data(Qt::UserRole).toString();
        originalAssignments.append(data);
    }

    // Apply current filter state
    filterAssignmentsBySimilarity(showingHighSimilarityOnly);
}

void Dashboard::showPlagiarismResults(const QVector<QPair<QPair<QString, QString>, double>>& results)
{
    qDebug() << "Showing results for" << results.size() << "comparisons";
    QDialog* resultsDialog = new QDialog(this);
    resultsDialog->setWindowTitle("Plagiarism Analysis Results");
    resultsDialog->setMinimumSize(800, 600);
    resultsDialog->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout* layout = new QVBoxLayout(resultsDialog);

    // Calculate average similarity
    double totalSimilarity = 0.0;
    for (const auto& result : results) {
        totalSimilarity += result.second;
    }
    double averageSimilarity = results.isEmpty() ? 0.0 : totalSimilarity / results.size();

    // Add average similarity label at the top
    QString averageText = QString("Average Similarity Score: %1%%")
                              .arg(QString::number(averageSimilarity, 'f', 2));
    QLabel* averageLabel = new QLabel(averageText, resultsDialog);
    averageLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: " +
                                (averageSimilarity > 35.0 ? QString("red") : QString("white")));
    layout->addWidget(averageLabel);

    // Create set of unique file names
    QSet<QString> uniqueFiles;
    for (const auto& result : results) {
        uniqueFiles.insert(result.first.first);
        uniqueFiles.insert(result.first.second);
    }
    QList<QString> filesList = uniqueFiles.values();
    std::sort(filesList.begin(), filesList.end());

    // Create matrix-style table
    QTableWidget* table = new QTableWidget(resultsDialog);
    int numFiles = filesList.size();
    table->setRowCount(numFiles);
    table->setColumnCount(numFiles);

    // Set headers
    table->setHorizontalHeaderLabels(filesList);
    table->setVerticalHeaderLabels(filesList);

    // Configure table properties for better visibility
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Set resize mode for better content display
    for (int i = 0; i < table->columnCount(); ++i) {
        table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        table->verticalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    // Set minimum section size for both headers
    table->horizontalHeader()->setMinimumSectionSize(150);
    table->verticalHeader()->setMinimumSectionSize(30);

    // Initialize all non-diagonal cells with "0.00%"
    for (int i = 0; i < numFiles; ++i) {
        for (int j = 0; j < numFiles; ++j) {
            QTableWidgetItem* item = new QTableWidgetItem(i == j ? "" : "0.00%");
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table->setItem(i, j, item);
        }
    }

    // Fill in similarity scores
    for (const auto& result : results) {
        if (result.first.first != result.first.second) {
            int row = filesList.indexOf(result.first.first);
            int col = filesList.indexOf(result.first.second);

            if (row != -1 && col != -1) { // Ensure valid indices
                QString similarityText = QString::number(result.second, 'f', 2) + "%";

                // Create and set item for the primary cell
                QTableWidgetItem* item1 = new QTableWidgetItem(similarityText);
                if (result.second > 50.0) {
                    item1->setBackground(QColor(255, 150, 150));  // Light Red
                } else if (result.second > 35.0) {
                    item1->setBackground(QColor(173, 216, 230));  // Light Blue
                }
                item1->setFlags(item1->flags() & ~Qt::ItemIsEditable);
                item1->setTextAlignment(Qt::AlignCenter);
                table->setItem(row, col, item1);

                // Create and set item for the symmetric cell
                QTableWidgetItem* item2 = new QTableWidgetItem(similarityText);
                if (result.second > 50.0) {
                    item2->setBackground(QColor(255, 150, 150));  // Light Red
                } else if (result.second > 35.0) {
                    item2->setBackground(QColor(173, 216, 230));  // Light Blue
                }
                item2->setFlags(item2->flags() & ~Qt::ItemIsEditable);
                item2->setTextAlignment(Qt::AlignCenter);
                table->setItem(col, row, item2);
            }
        }
    }

    // Style the headers
    QString headerStyle = "QHeaderView::section { padding: 8px; font-weight: bold; }";
    table->horizontalHeader()->setStyleSheet(headerStyle);
    table->verticalHeader()->setStyleSheet(headerStyle);

    layout->addWidget(table);

    // Add summary statistics
    QString summaryText = QString("Total Comparisons: %1\n"
                                  "Files with Similarity > 50%%: %2\n"
                                  "Files with Similarity > 35%%: %3")
                              .arg(results.size())
                              .arg(std::count_if(results.begin(), results.end(),
                                                 [](const auto& r) { return r.second > 50.0; }))
                              .arg(std::count_if(results.begin(), results.end(),
                                                 [](const auto& r) { return r.second > 35.0; }));
    QLabel* summaryLabel = new QLabel(summaryText, resultsDialog);
    layout->addWidget(summaryLabel);

    // Export button
    QPushButton* exportButton = new QPushButton("Export to PDF", resultsDialog);
    connect(exportButton, &QPushButton::clicked, resultsDialog,
            [this, table, averageText, summaryText]() {
                exportToPDF(table, averageText, summaryText);
            });
    layout->addWidget(exportButton);

    // Close button
    QPushButton* closeButton = new QPushButton("Close", resultsDialog);
    connect(closeButton, &QPushButton::clicked, resultsDialog, &QDialog::accept);
    layout->addWidget(closeButton);

    // Adjust table size after all content is added
    table->resizeColumnsToContents();
    table->resizeRowsToContents();

    resultsDialog->exec();
}

void Dashboard::clearDownloadedFiles() {
    downloadedFiles.clear();
    updatePlagiarismButtonState();
}

void Dashboard::refreshTableDisplay(const QVector<AssignmentData>& assignments) {
    qDebug() << "\n=== REFRESHING TABLE DISPLAY ===";
    qDebug() << "Assignments to display:" << assignments.size();

    // Block signals temporarily to prevent unwanted updates
    ui->assignments_tableWidget->blockSignals(true);

    // Store the current horizontal header state
    QHeaderView* header = ui->assignments_tableWidget->horizontalHeader();
    QByteArray headerState = header->saveState();

    // Clear the table while preserving columns
    int columnCount = ui->assignments_tableWidget->columnCount();
    ui->assignments_tableWidget->setRowCount(0);
    ui->assignments_tableWidget->setColumnCount(columnCount);

    // Set new row count
    ui->assignments_tableWidget->setRowCount(assignments.size());

    for (int row = 0; row < assignments.size(); ++row) {
        const auto& assignment = assignments[row];

        qDebug() << "Processing row" << row << ":" << assignment.fileName
                 << "Similarity:" << assignment.similarity;

        // Date column
        QTableWidgetItem* dateItem = new QTableWidgetItem(assignment.dateModified);
        dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);
        ui->assignments_tableWidget->setItem(row, 0, dateItem);

        // Name column
        QTableWidgetItem* nameItem = new QTableWidgetItem(assignment.fileName);
        nameItem->setData(Qt::UserRole, assignment.fileId);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->assignments_tableWidget->setItem(row, 1, nameItem);

        // Type column
        QTableWidgetItem* typeItem = new QTableWidgetItem(assignment.fileType);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        ui->assignments_tableWidget->setItem(row, 2, typeItem);

        // Download button
        QPushButton* downloadButton = new QPushButton("Download");
        connect(downloadButton, &QPushButton::clicked, this, [this, assignment]() {
            downloadFile(assignment.fileId, assignment.fileName);
        });
        ui->assignments_tableWidget->setCellWidget(row, 3, downloadButton);

        // Similarity column with color coding
        QString similarityText = QString::number(assignment.similarity, 'f', 2) + "%";
        QTableWidgetItem* similarityItem = new QTableWidgetItem(similarityText);
        similarityItem->setFlags(similarityItem->flags() & ~Qt::ItemIsEditable);

        // Color coding based on similarity
        if (assignment.similarity > 70.0) {
            similarityItem->setBackground(QColor(255, 150, 150));  // Red
        } else if (assignment.similarity > 50.0) {
            similarityItem->setBackground(QColor(255, 255, 150));  // Yellow
        } else {
            similarityItem->setBackground(Qt::darkGray);  // Default
        }

        ui->assignments_tableWidget->setItem(row, 4, similarityItem);
    }

    // Restore the header state
    header->restoreState(headerState);

    // Unblock signals
    ui->assignments_tableWidget->blockSignals(false);

    // Ensure columns are properly sized
    ui->assignments_tableWidget->resizeColumnsToContents();

    // Force complete UI refresh
    ui->assignments_tableWidget->viewport()->update();
    ui->assignments_tableWidget->update();
    QApplication::processEvents();

    qDebug() << "Table refresh completed. Final row count:"
             << ui->assignments_tableWidget->rowCount();
}

double Dashboard::getAverageSimilarity() const
{
    if (similarityResults.isEmpty()) {
        return 0.0;
    }

    double totalSimilarity = 0.0;
    for (const auto& result : similarityResults) {
        totalSimilarity += result.second;
    }
    return totalSimilarity / similarityResults.size();
}

void Dashboard::resetClassAverage()
{
    ui->class_average_label->setText("0.00%");
    ui->class_average_label->setStyleSheet("color: white; font-weight: bold;");
    ui->rejected_assignments_label->setText("Flagged Files: 0");
    similarityResults.clear();
    originalAssignments.clear();
}

void Dashboard::filterAssignmentsBySimilarity(bool showHighSimilarityOnly) {
    qDebug() << "\n=== FILTERING ASSIGNMENTS ===";
    qDebug() << "ShowHighSimilarityOnly:" << showHighSimilarityOnly;
    qDebug() << "Original assignments size:" << originalAssignments.size();

    if (originalAssignments.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No assignments to filter!");
        qDebug() << "WARNING: No original assignments to filter!";

        return;
    }

    QVector<AssignmentData> filteredAssignments;
    int highSimilarityCount = 0;

    // Populate filtered assignments based on condition
    for (const auto& assignment : originalAssignments) {
        if (assignment.similarity > 30.0) {
            highSimilarityCount++;
        }

        if (!showHighSimilarityOnly || assignment.similarity > 30.0) {
            filteredAssignments.append(assignment);
        }
    }

    qDebug() << "Filtered assignments size:" << filteredAssignments.size();
    qDebug() << "High similarity count:" << highSimilarityCount;

    // Update UI elements
    ui->total_submitted_assignments_label->setText(QString("Total Files: %1").arg(originalAssignments.size()));
    ui->rejected_assignments_label->setText(QString("Flagged Files: %1").arg(highSimilarityCount));

    // Update the table with filtered data
    refreshTableDisplay(filteredAssignments);
}

void Dashboard::updateTableRow(int row, const AssignmentData& assignment) {
    // Date column
    auto dateItem = new QTableWidgetItem(assignment.dateModified);
    dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);
    ui->assignments_tableWidget->setItem(row, 0, dateItem);

    // Name column with file ID stored
    auto nameItem = new QTableWidgetItem(assignment.fileName);
    nameItem->setData(Qt::UserRole, assignment.fileId);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    ui->assignments_tableWidget->setItem(row, 1, nameItem);

    // Type column
    auto typeItem = new QTableWidgetItem(assignment.fileType);
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    ui->assignments_tableWidget->setItem(row, 2, typeItem);

    // Download button
    auto downloadButton = new QPushButton("Download");
    connect(downloadButton, &QPushButton::clicked, this, [this, assignment]() {
        downloadFile(assignment.fileId, assignment.fileName);
    });
    ui->assignments_tableWidget->setCellWidget(row, 3, downloadButton);

    // Similarity column with color coding
    auto similarityItem = new QTableWidgetItem(QString::number(assignment.similarity, 'f', 2) + "%");
    similarityItem->setFlags(similarityItem->flags() & ~Qt::ItemIsEditable);

    if (assignment.similarity > 70.0) {
        similarityItem->setBackground(QColor(255, 150, 150));  // Red for high similarity
    } else if (assignment.similarity > 50.0) {
        similarityItem->setBackground(QColor(255, 255, 150));  // Yellow for moderate
    } else {
        similarityItem->setBackground(Qt::darkGray);  // Default
    }

    ui->assignments_tableWidget->setItem(row, 4, similarityItem);
}

void Dashboard::on_deletedAssignments_pushButton_clicked() {
    qDebug() << "\n=== FILTER BUTTON CLICKED ===";
    qDebug() << "Before toggle - showingHighSimilarityOnly:" << showingHighSimilarityOnly;
    qDebug() << "Current button text:" << ui->deletedAssignments_pushButton->text();
    qDebug() << "Original assignments count:" << originalAssignments.size();

    showingHighSimilarityOnly = !showingHighSimilarityOnly;

    qDebug() << "After toggle - showingHighSimilarityOnly:" << showingHighSimilarityOnly;

    // Force UI update before filtering
    ui->deletedAssignments_pushButton->setText(showingHighSimilarityOnly ? "Show All Files" : "Show High Similarity Only");
    ui->deletedAssignments_pushButton->repaint();

    filterAssignmentsBySimilarity(showingHighSimilarityOnly);
}



void Dashboard::exportToPDF(QTableWidget* table, const QString& averageText, const QString& summaryText)
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save PDF",
                                                    QDir::homePath() + "/plagiarism_report.pdf", "PDF Files (*.pdf)");

    if (fileName.isEmpty()) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageOrientation(QPageLayout::Landscape); // Changed to landscape for more width
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPainter painter(&printer);
    if (!painter.isActive()) {
        QMessageBox::warning(this, "Export Failed", "Could not create the PDF file.");
        return;
    }

    // // Get page rect as QRectF and convert to integer values
    // QRectF pageRectF = printer.pageRect(QPrinter::Point);
    // int pageWidth = static_cast<int>(pageRectF.width()) - 50; // 2 × 20pt margins
    // int pageHeight = static_cast<int>(pageRectF.height()) - 40; // 2 × 20pt margins

    int pageWidth = 13000; // 2 × 20pt margins
    int pageHeight = 7500;


    // Set positions and sizes
    int margin = 200;
    int headerHeight = 300;
    int totalColumns = table->columnCount();
    int totalRows = table->rowCount();

    // Calculate table dimensions to occupy most of the page
    int tableWidth = pageWidth * 0.95;
    int tableHeight = pageHeight * 0.8;

    // Calculate cell dimensions with increased horizontal spacing
    int horizontalSpacing = 5; // Added horizontal spacing between cells
    int cellWidth = (tableWidth / (totalColumns + 1)) - horizontalSpacing;
    int cellHeight = tableHeight / (totalRows + 1);

    // Start drawing at margin position
    painter.translate(margin, margin);

    // Draw title and header
    QFont titleFont("Arial", 14, QFont::Bold);
    QFont headerFont("Arial", 11, QFont::Bold);
    QFont contentFont("Arial", 10);

    painter.setFont(titleFont);
    // painter.drawText(QRect(0, 0, tableWidth, headerHeight), Qt::AlignCenter, "Plagiarism Analysis Report");
    painter.drawText(QRect(0, 0, tableWidth, headerHeight), Qt::AlignCenter, "Results of Compared files");//to delete this and uncomment the above line
    painter.setFont(headerFont);
    painter.drawText(QRect(0, headerHeight, tableWidth, headerHeight), Qt::AlignCenter, averageText);

    // Start position for table
    int yPos = 2 * headerHeight + 50;

    // Draw column headers
    painter.setFont(headerFont);
    for (int col = -1; col < totalColumns; ++col) {
        // Include horizontal spacing in x-position calculation
        int xPos = (col + 1) * (cellWidth + horizontalSpacing);
        QRect headerRect(xPos, yPos, cellWidth, cellHeight);
        painter.fillRect(headerRect, QColor(240, 240, 240));
        painter.drawRect(headerRect);
        if (col >= 0) {
            // Draw column header text with word wrapping
            QString headerText = table->horizontalHeaderItem(col)->text();
            painter.drawText(headerRect.adjusted(2, 2, -2, -2),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             headerText);
        }
    }

    // Draw rows and cells
    yPos += cellHeight;
    painter.setFont(contentFont);

    for (int row = 0; row < totalRows; ++row) {
        // Draw row header
        QRect rowHeaderRect(0, yPos, cellWidth, cellHeight);
        painter.fillRect(rowHeaderRect, QColor(240, 240, 240));
        painter.drawRect(rowHeaderRect);
        painter.drawText(rowHeaderRect.adjusted(2, 2, -2, -2),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         table->verticalHeaderItem(row)->text());

        // Draw cells in this row
        for (int col = 0; col < totalColumns; ++col) {
            // Include horizontal spacing in x-position calculation
            int xPos = (col + 1) * (cellWidth + horizontalSpacing);
            QTableWidgetItem* item = table->item(row, col);
            QRect cellRect(xPos, yPos, cellWidth, cellHeight);

            if (item) {
                // First fill all cells with white background
                painter.fillRect(cellRect, Qt::white);

                // Then apply special background color only for highlighted cells
                if (item->background().color() != Qt::transparent &&
                    item->background().color() != Qt::black) {
                    painter.fillRect(cellRect, item->background().color());
                }
                painter.drawRect(cellRect);
                painter.drawText(cellRect.adjusted(10, 10, -10, -10), Qt::AlignCenter, item->text());
            } else {
                // Fill empty cells with white background
                painter.fillRect(cellRect, Qt::white);
                painter.drawRect(cellRect);
            }
        }
        yPos += cellHeight;
    }

    // Draw summary at the bottom
    yPos += 20;
    painter.setFont(headerFont);
    QStringList summaryLines = summaryText.split('\n');
    for (const QString& line : summaryLines) {
        painter.drawText(QRect(0, yPos, tableWidth, headerHeight), Qt::AlignLeft, line);
        yPos += headerHeight;
    }

    // Add timestamp
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timestamp = "Generated on: " + currentTime.toString("yyyy-MM-dd hh:mm:ss");
    painter.drawText(QRect(0, yPos, tableWidth, headerHeight), Qt::AlignRight, timestamp);

    painter.end();

    QMessageBox::information(this, "Export Complete", "The report has been successfully exported to PDF.");
}

void Dashboard::filterAssignmentsBySearch(const QString& query) {
    QString lowercaseQuery = query.toLower();
    int visibleCount = 0;

    for (int row = 0; row < ui->assignments_tableWidget->rowCount(); ++row) {
        bool shouldShow = false;

        if (query.isEmpty()) {
            shouldShow = true;
        } else {
            // Check filename (column 1)
            QTableWidgetItem* nameItem = ui->assignments_tableWidget->item(row, 1);
            // Check file type (column 2)
            QTableWidgetItem* typeItem = ui->assignments_tableWidget->item(row, 2);

            if (nameItem && typeItem) {
                QString itemText = nameItem->text().toLower() + " " + typeItem->text().toLower();
                shouldShow = itemText.contains(lowercaseQuery);
            }
        }

        ui->assignments_tableWidget->setRowHidden(row, !shouldShow);
        if (shouldShow) {
            visibleCount++;
        }
    }
//to uncomment this whole block
    // // Update the total files label to show filtered count
    // int totalFiles = ui->assignments_tableWidget->rowCount();
    // if (visibleCount < totalFiles) {
    //     ui->total_submitted_assignments_label->setText(
    //         QString("Total Files: %1 (Showing: %2)").arg(totalFiles).arg(visibleCount)
    //         );
    // } else {
    //     ui->total_submitted_assignments_label->setText(
    //         QString("Total Files: %1").arg(totalFiles)
    //         );
    // }
}

void Dashboard::downloadAllFiles() {
    // Check if any files are available to download
    if (ui->assignments_tableWidget->rowCount() == 0) {
        QMessageBox::warning(this, "No Files", "There are no files available to download.");
        return;
    }

    // Create progress dialog
    QProgressDialog progressDialog("Downloading files...", "Cancel", 0,
                                   ui->assignments_tableWidget->rowCount(), this);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.show();

    int downloadCount = 0;
    bool wasCancelled = false;

    // Iterate through all visible rows
    for (int row = 0; row < ui->assignments_tableWidget->rowCount(); ++row) {
        // Skip hidden rows (filtered out by search)
        if (ui->assignments_tableWidget->isRowHidden(row)) {
            continue;
        }

        // Check if the operation was cancelled
        if (progressDialog.wasCanceled()) {
            wasCancelled = true;
            break;
        }

        // Get file ID and name from the table
        QTableWidgetItem* nameItem = ui->assignments_tableWidget->item(row, 1);
        if (nameItem) {
            QString fileId = nameItem->data(Qt::UserRole).toString();
            QString fileName = nameItem->text();

            // Update progress dialog
            progressDialog.setValue(downloadCount);
            progressDialog.setLabelText(QString("Downloading %1...").arg(fileName));

            // Create the download request
            QString apiUrl = QString("https://www.googleapis.com/drive/v3/files/%1?alt=media")
                                 .arg(fileId);
            QUrl url(apiUrl);
            QNetworkRequest request(url);
            request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());

            // Create network manager for this specific download
            QNetworkAccessManager* manager = new QNetworkAccessManager(this);
            QNetworkReply* reply = manager->get(request);

            // Create event loop to make the download synchronous
            QEventLoop loop;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() == QNetworkReply::NoError) {
                QByteArray response = reply->readAll();
                QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                                       + "/" + fileName;
                QFile file(downloadPath);

                if (file.open(QIODevice::WriteOnly)) {
                    file.write(response);
                    file.close();
                    downloadCount++;

                    // Store file content for plagiarism detection
                    FileContent fileContent;
                    fileContent.id = fileId;
                    fileContent.name = fileName;
                    fileContent.content = extractTextFromFile(downloadPath);
                    downloadedFiles.append(fileContent);
                }
            }

            reply->deleteLater();
            manager->deleteLater();
        }

        // Update the progress
        progressDialog.setValue(downloadCount);
        QApplication::processEvents();
    }

    // Update plagiarism button state
    updatePlagiarismButtonState();

    // Show completion message
    if (!wasCancelled) {
        QMessageBox::information(this, "Download Complete",
                                 QString("Successfully downloaded %1 files to your Downloads folder.").arg(downloadCount));
    } else {
        QMessageBox::information(this, "Download Cancelled",
                                 QString("Download cancelled. %1 files were downloaded.").arg(downloadCount));
    }
}

void Dashboard::updateLoginStatus(const QString& email) {
    QString status = isLoggedIn ? "Logged in as: " + email : "Not logged in";
    ui->statusbar->showMessage(status);
}

void Dashboard::exportSimilarityMatrix() {
    // Create a CSV file with the similarity matrix
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Save Similarity Matrix", "", "CSV files (*.csv)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);

        // Write header row
        QStringList fileNames;
        for (const auto& file : downloadedFiles) {
            fileNames << file.name;
            stream << "," << file.name;
        }
        stream << "\n";

        // Write similarity matrix
        for (int i = 0; i < fileNames.size(); ++i) {
            stream << fileNames[i];
            for (int j = 0; j < fileNames.size(); ++j) {
                double similarity = (i == j) ? 100.0 :
                                        calculateSimilarity(downloadedFiles[i].content,
                                                            downloadedFiles[j].content);
                stream << "," << QString::number(similarity, 'f', 2);
            }
            stream << "\n";
        }
    }
    file.close();
}



/* todo
 * 1. Fetch more than 100 files or fetch everything
 * DONE: - 1. disable login button after login/change it to logout
 * DONE: - 3. prevent editing names after download
 * DONE: - 4. implement check for similarity and plagiarism and give a score(can chatgpt work?)
 * DONE: - 5. app icon adjustment to stop stretching in full screen mode
 * DONE: - 6. download only from a specific drive (can be provided by user)
 * DOne: - 7. column for displaying file type
 * DONE: - 8. implement class average score
 * DONE: - 9. implement search and filtering - search should be by name and dynamic
 * DONE: - 10. implement populating files only after pressing refresh button
 * DONE: - 11. Display total files fetched on the total submitted label and not the total displayed files
 * Done: - 12. Rejected assighment button should toggle between total accepted assignment and all files
 * DONE: - 12. Implement a button for download all
 * DONE: - 13. Logged in as
 * DONE: - 14. Customize the table to display results in a better format (names on the colum and row title then scores to be simlar to matrix entries.)
 * DONE: - 15. Make the above file downloadable in a pdf format.
 * */
