#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QInputDialog>
#include <QDateEdit>
#include <QMessageBox>
#include <QDate>
#include <QLabel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    // Set up table columns
    ui->assignmentTable->setColumnCount(2);
    ui->assignmentTable->setHorizontalHeaderLabels(QStringList() << "Assignment" << "Due Date");

    connect(ui->addButton, &QPushButton::clicked, this, &MainWindow::onAddButtonClicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteButtonClicked);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onAddButtonClicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "Add Assignment", "Assignment Name:", QLineEdit::Normal, "", &ok);

    if (!ok || name.isEmpty()) return;

    // Create a dialog to select the due date
    QDialog dateDialog(this);
    dateDialog.setWindowTitle("Select Due Date");
    QVBoxLayout layout(&dateDialog);

    QLabel label("Due Date:");
    QDateEdit dateEdit;
    dateEdit.setCalendarPopup(true);
    dateEdit.setDate(QDate::currentDate().addDays(7));  // Default to one week later
    QPushButton okButton("OK");
    connect(&okButton, &QPushButton::clicked, &dateDialog, &QDialog::accept);

    layout.addWidget(&label);
    layout.addWidget(&dateEdit);
    layout.addWidget(&okButton);

    if (dateDialog.exec() == QDialog::Accepted) {
        QDate dueDate = dateEdit.date();
        assignments.append({name, dueDate});
        updateTable();
    }
}

void MainWindow::onDeleteButtonClicked() {
    int row = ui->assignmentTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Delete Assignment", "Please select an assignment to delete.");
        return;
    }
    assignments.remove(row);
    updateTable();
}

void MainWindow::updateTable() {
    ui->assignmentTable->setRowCount(assignments.size());
    for (int i = 0; i < assignments.size(); ++i) {
        ui->assignmentTable->setItem(i, 0, new QTableWidgetItem(assignments[i].name));
        ui->assignmentTable->setItem(i, 1, new QTableWidgetItem(assignments[i].dueDate.toString()));
    }
}
