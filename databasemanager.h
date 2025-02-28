#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>

class dataBaseManager : public QObject
{
    Q_OBJECT
public:
    explicit dataBaseManager(QObject *parent = nullptr);

signals:
};

#endif // DATABASEMANAGER_H
