#include "Dashboard.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Dashboard w;
    w.show();
    QIcon appIcon(":/../../Downloads/assignment-manager-logo.ico");
    w.setWindowIcon(appIcon);


    return a.exec();
}


