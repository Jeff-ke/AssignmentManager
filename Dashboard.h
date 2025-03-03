#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "GoogleAuthManager.h"
#include "GoogleDriveService.h"
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


    void updateDateLabel();
    void startDateTimer();
    void setAppLogo();
    void downloadFile(const QString &fileId, const QString &fileName);
    void onDriveFilesFetched();
    void fetchDriveFiles(const QString &folderId);
    void fetchDriveFolders();
    double getAverageSimilarity() const;
    void clearDownloadedFiles();
    void exportToPDF(QTableWidget* table, const QString& averageText, const QString& summaryText);
    void exportSimilarityMatrix();
    void updateLoginStatus(const QString& email);
    void downloadAllFiles();
    void filterAssignmentsBySearch(const QString& query);
    void updateSimilarityDisplay(const QMap<QString, double>& highestSimilarities,
                                 double averageSimilarity);

    void onUserProfileFetched();
    void onLoginStatusChanged(bool isLoggedIn);
    void onAuthTokenReceived();
    void onAuthenticationError(const QString &errorMessage);
    void onUserLoggedOut();
    void onTokenRefreshed();
    void on_deletedAssignments_pushButton_clicked();
    void on_refresh_pushButton_clicked();
    void on_folderComboBox_currentIndexChanged(int index);
    void on_login_pushButton_clicked();
    void onDriveFoldersFetched();

//  void verifyState();
//  void on_refresh_pushButton_clicked(QString folderId);
//  void on_fetchDriveFiles_pushButton_clicked(QString folderId );


private:

    Ui::Dashboard *ui;//UI pointer and other pointers and declarations
    QString folderId;
    QString selectedFolderId;
    QProgressDialog* progressDialog = nullptr;
    QToolBar* toolBar;
    QLineEdit* searchBox;
    GoogleAuthManager *authManager;
    GoogleDriveService *driveService;
    void fetchUserProfile();
    bool showingHighSimilarityOnly=false;
    void filterAssignmentsBySimilarity(bool showHighSimilarityOnly);
    void resetClassAverage();
    void compareFiles();
    void showPlagiarismResults(const QVector<QPair<QPair<QString, QString>, double>>& results);
    void updatePlagiarismButtonState() {
        if (ui->checkPlagiarism_pushButton) {
            ui->checkPlagiarism_pushButton->setEnabled(downloadedFiles.size() >= 2);
        }
    }


    // QVector<QPair<QString, double>> originalAssignments;
    // QPushButton* filterButton;
    // bool showingFilteredResults;
    // void toggleSimilarityFilter();
    // void refreshClassAverage();
    // void restoreFullTable();
    // QVector<int> hiddenRows;  // To store rows that are filtered out

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
