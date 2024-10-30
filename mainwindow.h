#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDate>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct Assignment {
    QString name;
    QDate dueDate;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddButtonClicked();
    void onDeleteButtonClicked();

private:
    Ui::MainWindow *ui;
    QVector<Assignment> assignments;

    void updateTable();
};

#endif // MAINWINDOW_H
