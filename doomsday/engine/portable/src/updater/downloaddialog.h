#ifndef DOWNLOADDIALOG_H
#define DOWNLOADDIALOG_H

#include <QDialog>
#include <de/String>

class QNetworkReply;

/**
 * Dialog for downloading an update in the background and then starting
 * the (re)installation process.
 */
class DownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadDialog(de::String downloadUri, QWidget *parent = 0);
    ~DownloadDialog();
    
signals:
    void downloadFailed(QString uri);
    
public slots:
    void finished(QNetworkReply*);
    void progress(qint64 received, qint64 total);
    
private:
    struct Instance;
    Instance* d;
};

#endif // DOWNLOADDIALOG_H
