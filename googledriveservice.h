#ifndef GOOGLEDRIVESERVICE_H
#define GOOGLEDRIVESERVICE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QPair>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class GoogleDriveService : public QObject
{
    Q_OBJECT

public:
    explicit GoogleDriveService(QObject *parent = nullptr);
    ~GoogleDriveService();

    // File structure to store file information
    struct DriveFile {
        QString id;
        QString name;
        QString mimeType;
        QString modifiedTime;
        QString webViewLink;
        qint64 size;
    };

    // Folder structure to store folder information
    struct DriveFolder {
        QString id;
        QString name;
        QString parentId;
    };

    // Public methods for Drive operations
    void fetchFolders(const QString &accessToken);
    void fetchFiles(const QString &accessToken, const QString &folderId);
    void downloadFile(const QString &accessToken, const QString &fileId, const QString &fileName);

    // Additional functionality
    void getFileContent(const QString &accessToken, const QString &fileId);
    void searchFiles(const QString &accessToken, const QString &query);

signals:
    // Signals to notify about Drive operations
    void foldersReceived(const QVector<DriveFolder> &folders);
    void filesReceived(const QVector<DriveFile> &files);
    void fileDownloaded(const QString &fileId, const QString &filePath, const QString &content);
    void fileContentReceived(const QString &fileId, const QString &content);
    void requestError(const QString &errorMessage);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private slots:
    void onFoldersRequestFinished();
    void onFilesRequestFinished();
    void onFileDownloadFinished();
    void onFileContentRequestFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    // Member variables
    QNetworkAccessManager *m_networkManager;

    // Constants
    const QString API_BASE_URL = "https://www.googleapis.com/drive/v3";

    // Helper methods
    QNetworkReply* createGetRequest(const QString &url, const QString &accessToken);
    DriveFile parseFileJson(const QJsonObject &fileObject);
    DriveFolder parseFolderJson(const QJsonObject &folderObject);
    QString saveFileToDisk(const QString &fileName, const QByteArray &fileData);
    QString extractTextFromMimeType(const QString &mimeType, const QByteArray &fileData);
};

#endif // GOOGLEDRIVESERVICE_H
