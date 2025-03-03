#ifndef PLAGIARISMDETECTOR_H
#define PLAGIARISMDETECTOR_H

#include <QObject>

class PlagiarismDetector : public QObject
{
    Q_OBJECT
public:
    explicit PlagiarismDetector(QObject *parent = nullptr);

signals:
};

#endif // PLAGIARISMDETECTOR_H
