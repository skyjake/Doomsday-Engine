/**
 * @file downloaddialog.cpp
 * Dialog that downloads a distribution package. @ingroup updater
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "downloaddialog.h"
#include "updatersettings.h"
#include "dd_version.h"
#include "window.h"
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

static DownloadDialog* downloadInProgress;

struct DownloadDialog::Instance
{
    DownloadDialog* self;
    QNetworkAccessManager* network;
    bool downloading;
    QPushButton* install;
    QProgressBar* bar;
    QLabel* hostText;
    QLabel* progText;
    QUrl uri;
    QUrl uri2;
    de::String savedFilePath;
    bool fileReady;
    QNetworkReply* reply;
    de::String redirected;

    Instance(DownloadDialog* d, de::String downloadUri, de::String fallbackUri)
        : self(d), downloading(false), uri(downloadUri), uri2(fallbackUri), fileReady(false), reply(0)
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

        hostText = new QLabel;
        updateLocation(uri);
        mainLayout->addWidget(hostText);
        mainLayout->addWidget(bar);

        progText = new QLabel;
        setProgressText(tr("Connecting..."));
        mainLayout->addWidget(progText);

        mainLayout->addWidget(bbox);

        network = new QNetworkAccessManager(self);
        QObject::connect(network, SIGNAL(finished(QNetworkReply*)), self, SLOT(finished(QNetworkReply*)));

        startDownload();
    }

    void updateLocation(const QUrl& url)
    {
        hostText->setText(tr("Downloading update from <b>%1</b>...").arg(url.host()));
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
        downloadInProgress = self;
    }

    void setProgressText(de::String text)
    {
        progText->setText("<small>" + text + "</small>");
    }
};

DownloadDialog::DownloadDialog(de::String downloadUri, de::String fallbackUri, QWidget *parent)
    : UpdaterDialog(parent)
{
    d = new Instance(this, downloadUri, fallbackUri);

#ifndef MACOSX
    setWindowTitle(DOOMSDAY_NICENAME" Update");
    setWindowIcon(Window_Widget(Window_Main())->windowIcon());
#endif
}

DownloadDialog::~DownloadDialog()
{
    downloadInProgress = 0;
    delete d;
}

de::String DownloadDialog::downloadedFilePath() const
{
    if(!d->fileReady) return "";
    return d->savedFilePath;
}

bool DownloadDialog::isReadyToInstall() const
{
    return d->fileReady;
}

void DownloadDialog::finished(QNetworkReply* reply)
{
    LOG_AS("Download");

    reply->deleteLater();
    d->reply = 0;

    if(reply->error() != QNetworkReply::NoError)
    {
        LOG_WARNING("Failure: ") << reply->errorString();
        d->setProgressText(reply->errorString());
        downloadInProgress = 0;
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
            LOG_WARNING("Failed, received an HTML page.");

            // Do we have a fallback option?
            if(!d->uri2.isEmpty() && d->uri2 != d->uri)
            {
                d->uri = d->uri2;
                d->updateLocation(d->uri);
                d->startDownload();
                return;
            }

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
    return downloadInProgress != 0;
}

void Updater_RaiseCompletedDownloadDialog(void)
{
    if(downloadInProgress && downloadInProgress->isReadyToInstall())
    {
        downloadInProgress->show();
        downloadInProgress->raise();
        downloadInProgress->activateWindow();
    }
}
