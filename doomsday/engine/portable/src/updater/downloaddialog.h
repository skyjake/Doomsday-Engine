#ifndef DOWNLOADDIALOG_H
#define DOWNLOADDIALOG_H

#ifdef __cplusplus

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

    /**
     * Returns the path of the downloaded file.
     *
     * @return Path, or an empty string if the download did not finish
     * successfully.
     */
    de::String downloadedFilePath() const;
    
signals:
    void downloadFailed(QString uri);
    
public slots:
    void replyMetaDataChanged();
    void progress(qint64 received, qint64 total);
    void finished(QNetworkReply*);

private:
    struct Instance;
    Instance* d;
};

#endif // __cplusplus

// C API
#ifdef __cplusplus
extern "C" {
#endif

int Updater_IsDownloadInProgress(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DOWNLOADDIALOG_H
