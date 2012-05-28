#include "downloaddialog.h"
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

struct DownloadDialog::Instance
{
    DownloadDialog* self;
    QNetworkAccessManager* network;
    bool downloading;
    QProgressBar* bar;
    QUrl uri;

    Instance(DownloadDialog* d, de::String downloadUri) : self(d), downloading(false), uri(downloadUri)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout;
        self->setLayout(mainLayout);

        bar = new QProgressBar;
        bar->setRange(0, 100);

        QDialogButtonBox* bbox = new QDialogButtonBox;
        QPushButton* install = bbox->addButton("Install", QDialogButtonBox::AcceptRole);
        install->setEnabled(false);
        QPushButton* cancel = bbox->addButton(QDialogButtonBox::Cancel);
        QObject::connect(install, SIGNAL(clicked()), self, SLOT(accept()));
        QObject::connect(cancel, SIGNAL(clicked()), self, SLOT(reject()));

        mainLayout->addWidget(new QLabel(QString("Downloading update from <b>%1</b>...").arg(uri.host())));
        mainLayout->addWidget(bar);
        mainLayout->addWidget(bbox);

        network = new QNetworkAccessManager(self);
        QObject::connect(network, SIGNAL(finished(QNetworkReply*)), self, SLOT(finished(QNetworkReply*)));

        startDownload();
    }

    void startDownload()
    {
        LOG_INFO("Downloading %s.") << uri.toString();

        QNetworkReply* reply = network->get(QNetworkRequest(uri));
        QObject::connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
                         self, SLOT(progress(qint64,qint64)));
    }
};

DownloadDialog::DownloadDialog(de::String downloadUri, QWidget *parent)
    : QDialog(parent)
{
    d = new Instance(this, downloadUri);
}

DownloadDialog::~DownloadDialog()
{
    delete d;
}

void DownloadDialog::finished(QNetworkReply* reply)
{
    reply->deleteLater();

    /// @todo If/when we include WebKit, this can be done more intelligent using QWebPage. -jk

    QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if(contentType == "text/html; charset=utf-8")
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
        start += 5;
        QString equivRefresh = html.mid(start, html.indexOf("\"", start) - start);
        equivRefresh.replace("&amp;", "&");

        // This is what we should actually be downloading.
        d->uri = QUrl::fromEncoded(equivRefresh.toAscii());

        //d->startDownload();

        de::String path = d->uri.path();
        qDebug() << path.fileName();
        downloading = true;


    }
    else
    {
        //QFile file()
    }

    LOG_DEBUG("Request finished.");

}

void DownloadDialog::progress(qint64 received, qint64 total)
{
    if(downloading)
    {
        LOG_DEBUG("Progress: %i bytes received, total %i bytes.") << received << total;
        d->bar->setValue(received * 100 / total);
    }
}
