/**
 * @file updatedownloaddialog.h
 * Dialog that downloads a distribution package. @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_UPDATEDOWNLOADDIALOG_H
#define DE_CLIENT_UPDATEDOWNLOADDIALOG_H

#include "ui/dialogs/downloaddialog.h"

/**
 * Dialog for downloading an update in the background and then starting
 * the (re)installation process.
 */
class UpdateDownloadDialog : public DownloadDialog
{
public:
    DE_AUDIENCE(Progress, void downloadProgress(int percentage))
    DE_AUDIENCE(Failure,  void downloadFailed(const de::String &uri))

public:
    UpdateDownloadDialog(de::String downloadUri, de::String fallbackUri);

    ~UpdateDownloadDialog() override;

    /**
     * Returns the path of the downloaded file.
     *
     * @return Path, or an empty string if the download did not finish
     * successfully.
     */
    de::String downloadedFilePath() const;

    bool isReadyToInstall() const;
    bool isFailed() const;

public:
    static bool isDownloadInProgress();
    static UpdateDownloadDialog &currentDownload();
    static void showCompletedDownload();

//    void replyMetaDataChanged();
//    void progress(qint64 received, qint64 total);
//    void finished(QNetworkReply *);
    void cancel() override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UPDATEDOWNLOADDIALOG_H
