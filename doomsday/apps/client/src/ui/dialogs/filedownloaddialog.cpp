/** @file filedownloaddialog.cpp  Dialog for monitoring file download progress.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/dialogs/filedownloaddialog.h"
#include "network/serverlink.h"

#include <de/charsymbols.h>

using namespace de;

DE_GUI_PIMPL(FileDownloadDialog)
, DE_OBSERVES(PackageDownloader, Status)
{
    PackageDownloader &downloader;
    String message = "Downloading data files...";

    Impl(Public *i, PackageDownloader &downloader)
        : Base(i)
        , downloader(downloader)
    {
        downloader.audienceForStatus() += this;

        self().progressIndicator().setText(message + "\n" _E(l) _E(F) _E(l) DE_CHAR_MDASH
                                           " files / " DE_CHAR_MDASH " MB");
    }

    void downloadStatusUpdate(const Rangei64 &bytes, const Rangei &files)
    {
        if (!bytes.end) return;

        auto &indicator = self().progressIndicator();
        indicator.setProgress(round<int>(100.0 * double(bytes.size())/double(bytes.end)));

        indicator.setText(Stringf("%s\n" _E(l) _E(F) "%i file%s / %.1f MB",
                                  message.c_str(),
                                  files.start,
                                  DE_PLURAL_S(files.start),
                                  bytes.start / 1.0e6));
    }
};

FileDownloadDialog::FileDownloadDialog(PackageDownloader &downloader)
    : d(new Impl(this, downloader))
{}

void FileDownloadDialog::cancel()
{
    d->downloader.cancel();
}

void FileDownloadDialog::finish(int result)
{
    if (!result) cancel();

    DownloadDialog::finish(result);
}
