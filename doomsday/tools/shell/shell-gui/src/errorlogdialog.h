#ifndef ERRORLOGDIALOG_H
#define ERRORLOGDIALOG_H

#include <QDialog>
#include <de/libdeng2.h>

class ErrorLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ErrorLogDialog(QWidget *parent = 0);

    void setMessage(QString const &message);
    void setLogContent(QString const &text);

signals:

public slots:

private:
    DENG2_PRIVATE(d)
};

#endif // ERRORLOGDIALOG_H
