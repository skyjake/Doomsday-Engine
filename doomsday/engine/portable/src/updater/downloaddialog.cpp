#include "downloaddialog.h"
#include "updatersettings.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <de/Log>
#include <QDebug>

static bool downloadInProgress;

struct DownloadDialog::Instance
{
    DownloadDialog* self;
    QNetworkAccessManager* network;
    bool downloading;
    QPushButton* install;
    QProgressBar* bar;
    QLabel* progText;
    QUrl uri;
    de::String savedFilePath;
    bool fileReady;
    QNetworkReply* reply;
    de::String redirected;

    Instance(DownloadDialog* d, de::String downloadUri)
        : self(d), downloading(false), uri(downloadUri), fileReady(false), reply(0)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout;
        self->setLayout(mainLayout);

        bar = new QProgressBar;
        bar->setTextVisible(false);
        bar->setRange(0, 100);

        QDialogButtonBox* bbox = new QDialogButtonBox;
        /// @todo If game in progress, use "Autosave and Install"
        install = bbox->addButton(tr("Install"), QDialogButtonBox::AcceptRole);
        install->setEnabled(false);
        QPushButton* cancel = bbox->addButton(QDialogButtonBox::Cancel);
        QObject::connect(install, SIGNAL(clicked()), self, SLOT(accept()));
        QObject::connect(cancel, SIGNAL(clicked()), self, SLOT(reject()));

        mainLayout->addWidget(new QLabel(tr("Downloading update from <b>%1</b>...").arg(uri.host())));
        mainLayout->addWidget(bar);

        progText = new QLabel;
        setProgressText(tr("Connecting..."));
        mainLayout->addWidget(progText);

        mainLayout->addWidget(bbox);

        network = new QNetworkAccessManager(self);
        QObject::connect(network, SIGNAL(finished(QNetworkReply*)), self, SLOT(finished(QNetworkReply*)));

        startDownload();
    }

    void startDownload()
    {
        downloading = true;
        redirected.clear();

        de::String path = uri.path();
        savedFilePath = UpdaterSettings().downloadPath() / path.fileName();

        reply = network->get(QNetworkRequest(uri));
        QObject::connect(reply, SIGNAL(metaDataChanged()), self, SLOT(replyMetaDataChanged()));
        QObject::connect(reply, SIGNAL(downloadProgress(qint64,qint64)), self, SLOT(progress(qint64,qint64)));

        LOG_INFO("Downloading %s, saving as: %s") << uri.toString() << savedFilePath;

        // Global state flag.
        downloadInProgress = true;
    }

    void setProgressText(de::String text)
    {
        progText->setText("<small>" + text + "</small>");
    }
};

DownloadDialog::DownloadDialog(de::String downloadUri, QWidget *parent)
    : QDialog(parent)
{
    d = new Instance(this, downloadUri);
}

DownloadDialog::~DownloadDialog()
{
    downloadInProgress = false;
    delete d;
}

de::String DownloadDialog::downloadedFilePath() const
{
    if(!d->fileReady) return "";
    return d->savedFilePath;
}

void DownloadDialog::finished(QNetworkReply* reply)
{
    LOG_AS("Download");

    downloadInProgress = false;
    reply->deleteLater();
    d->reply = 0;

    if(reply->error() != QNetworkReply::NoError)
    {
        LOG_WARNING("Failure: ") << reply->errorString();
        d->setProgressText(reply->errorString());
        return;
    }

    /// @todo If/when we include WebKit, this can be done more intelligently using QWebPage. -jk

    if(!d->redirected.isEmpty())
    {
        LOG_INFO("Redirected to: %s") << d->redirected;
        d->uri = QUrl(d->redirected);
        d->redirected.clear();
        d->startDownload();
        return;
    }

    if(!d->downloading)
    {
        // This does not look like a binary file... Let's see if we can parse the page.
        QString html = QString::fromUtf8(reply->readAll());

        /// @todo Use a regular expression for parsing the redirection.

        int start = html.indexOf("<meta http-equiv=\"refresh\"", 0, Qt::CaseInsensitive);
        if(start < 0)
        {
            emit downloadFailed(d->uri.toString());
            return;
        }
        start = html.indexOf("url=\"", start, Qt::CaseInsensitive);
        if(start < 0)
        {
            emit downloadFailed(d->uri.toString());
            return;
        }
        start += 5; // skip: url="
        QString equivRefresh = html.mid(start, html.indexOf("\"", start) - start);
        equivRefresh.replace("&amp;", "&");

        // This is what we should actually be downloading.
        d->uri = QUrl::fromEncoded(equivRefresh.toAscii());

        LOG_INFO("Redirected to: %s") << d->uri.toString();

        d->startDownload();
        return;
    }

    // Save the received data.
    QFile file(d->savedFilePath);
    if(file.open(QFile::WriteOnly | QFile::Truncate))
    {
        file.write(reply->readAll());
    }
    else
    {
        LOG_WARNING("Failed to write to: %s") << d->savedFilePath;
        emit downloadFailed(d->uri.toString());
    }

    raise();
    activateWindow();
    d->fileReady = true;
    d->setProgressText(tr("Ready to install"));
    d->install->setEnabled(true);

    LOG_DEBUG("Request finished.");
}

void DownloadDialog::progress(qint64 received, qint64 total)
{
    LOG_AS("Download");

    if(d->downloading && total > 0)
    {
        d->bar->setValue(received * 100 / total);
        const double MB = 1.0e6; // MiB would be 2^20
        d->setProgressText(tr("Received %1 MB out of total %2 MB")
                           .arg(received/MB, 0, 'f', 1).arg(total/MB, 0, 'f', 1));
    }
}

void DownloadDialog::replyMetaDataChanged()
{
    LOG_AS("Download");

    de::String contentType = d->reply->header(QNetworkRequest::ContentTypeHeader).toString();
    de::String redirection = d->reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();

    if(!redirection.isEmpty())
    {
        d->redirected = redirection;
    }
    else if(contentType.startsWith("text/html"))
    {
        // Looks like a redirection page.
        d->downloading = false;
    }
    else
    {
        LOG_DEBUG("Receiving content of type '%s'.") << contentType;
    }
}

int Updater_IsDownloadInProgress(void)
{
    return downloadInProgress;
}
