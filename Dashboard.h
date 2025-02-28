#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "ui_Dashboard.h"
#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>

#include <QProgressDialog>

#include <QPrinter>
#include <QPainter>
#include <QFileDialog>
#include <QPageSize>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrinterInfo>

#include <QToolBar>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QStatusBar>



#include <poppler/qt6/poppler-qt6.h>

class Dashboard : public QMainWindow
{
    Q_OBJECT

public:
    explicit Dashboard(QWidget *parent = nullptr);

    ~Dashboard();

private slots:
    void startOAuthProcess();
    void requestAccessToken(const QString &authCode, const QString &folderId = QString());
    void refreshAccessToken();
    void scheduleTokenRefresh(int secondsUntilRefresh);

    void updateDateLabel();
    void startDateTimer();
    void setAppLogo();
    void verifyState();

    void on_login_pushButton_clicked();
    //void on_refresh_pushButton_clicked(QString folderId);


    void onAccessTokenReceived();
    void onReadyRead();
   // void on_fetchDriveFiles_pushButton_clicked(QString folderId );
    void downloadFile(const QString &fileId, const QString &fileName);
    void onDriveFilesFetched();
    void fetchDriveFiles(const QString &folderId);
    void fetchDriveFolders();
    void onDriveFoldersFetched();
    void on_folderComboBox_currentIndexChanged(int index);

    void logout();

    double getAverageSimilarity() const;

    void updateSimilarityDisplay(const QMap<QString, double>& highestSimilarities,
                                 double averageSimilarity);
    void on_deletedAssignments_pushButton_clicked();
    void on_refresh_pushButton_clicked();
    void clearDownloadedFiles();

    void exportToPDF(QTableWidget* table, const QString& averageText, const QString& summaryText);
    void exportSimilarityMatrix();
    void updateLoginStatus(const QString& email);
    void downloadAllFiles();
    void filterAssignmentsBySearch(const QString& query);
    void onUserProfileFetched();


private:
    Ui::Dashboard *ui;//UI pointer and other pointers and declarations
    QTcpServer *server;
    QTimer *tokenRefreshTimer;
    QString accessToken;
    QString refreshToken;
    QDateTime tokenExpiryTime;
    QString folderId;
    QString selectedFolderId;
    bool isLoggedIn;
    QProgressDialog* progressDialog = nullptr;



    QToolBar* toolBar;
    QLineEdit* searchBox;

    void fetchUserProfile();

    // QPushButton* filterButton;
    // bool showingFilteredResults;
    // void toggleSimilarityFilter();
    // void refreshClassAverage();
    // void restoreFullTable();
    // QVector<int> hiddenRows;  // To store rows that are filtered out



    bool showingHighSimilarityOnly=false;
    void filterAssignmentsBySimilarity(bool showHighSimilarityOnly);
    void resetClassAverage();
  //  QVector<QPair<QString, double>> originalAssignments;


     void compareFiles();
     void showPlagiarismResults(const QVector<QPair<QPair<QString, QString>, double>>& results);

     void updatePlagiarismButtonState() {
         if (ui->checkPlagiarism_pushButton) {
             ui->checkPlagiarism_pushButton->setEnabled(downloadedFiles.size() >= 2);
         }
     }

    struct FileContent {
        QString id;
        QString name;
        QString content;
    };

    QVector<FileContent> downloadedFiles;

    struct AssignmentData {
        QString dateModified;
        QString fileName;
        QString fileType;
        QString fileId;
        double similarity;
    };
    QVector<AssignmentData> originalAssignments;
    QVector<AssignmentData> filteredAssignments;


    double calculateSimilarity(const QString& text1, const QString& text2);
    QStringList preprocessText(const QString& text);
    QString extractTextFromFile(const QString& filePath);

    // Add member variables
    QVector<QPair<QPair<QString, QString>, double>> similarityResults;
    void updateTableRow(int row, const AssignmentData& assignment);
    int displayAssignment(int currentRow, const AssignmentData& assignment, bool showHighSimilarityOnly);
    void refreshTableDisplay(const QVector<AssignmentData>& assignments);

};



#endif // DASHBOARD_H
