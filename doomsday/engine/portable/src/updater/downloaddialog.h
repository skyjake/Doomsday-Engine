/**
 * @file downloaddialog.h
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

#ifndef LIBDENG_DOWNLOADDIALOG_H
#define LIBDENG_DOWNLOADDIALOG_H

#ifdef __cplusplus

#include "updaterdialog.h"
#include <de/String>

class QNetworkReply;

/**
 * Dialog for downloading an update in the background and then starting
 * the (re)installation process.
 */
class DownloadDialog : public UpdaterDialog
{
    Q_OBJECT

public:
    explicit DownloadDialog(de::String downloadUri, de::String fallbackUri, QWidget *parent = 0);
    ~DownloadDialog();

    /**
     * Returns the path of the downloaded file.
     *
     * @return Path, or an empty string if the download did not finish
     * successfully.
     */
    de::String downloadedFilePath() const;

    bool isReadyToInstall() const;
    
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

void Updater_RaiseCompletedDownloadDialog(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_DOWNLOADDIALOG_H
