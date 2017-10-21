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
#include "network/packagedownloader.h"
#include "network/serverlink.h"

#include <de/charsymbols.h>

using namespace de;

DENG_GUI_PIMPL(FileDownloadDialog)
, DENG2_OBSERVES(PackageDownloader, Status)
{
    String message = tr("Downloading data files...");

    Impl(Public *i) : Base(i)
    {
        ServerLink::get().packageDownloader().audienceForStatus() += this;

        self().progressIndicator().setText(tr("%1\n" _E(l)_E(F) "%2"
                                              DENG2_CHAR_MDASH " files / "
                                              DENG2_CHAR_MDASH " MB")
                                           .arg(message)
                                           .arg(_E(l)));
    }

    void downloadStatusUpdate(Rangei64 const &bytes, Rangei const &files)
    {
        if (!bytes.end) return;

        auto &indicator = self().progressIndicator();
        indicator.setProgress(round<int>(100.0 * double(bytes.size())/double(bytes.end)));

        indicator.setText(tr("%1\n" _E(l)_E(F) "%2 file%3 / %4 MB")
                          .arg(message)
                          .arg(files.start)
                          .arg(DENG2_PLURAL_S(files.start))
                          .arg(bytes.start/1.0e6, 0, 'f', 1));
    }
};

FileDownloadDialog::FileDownloadDialog()
    : d(new Impl(this))
{}

void FileDownloadDialog::cancel()
{
    ServerLink::get().packageDownloader().cancel();
}

void FileDownloadDialog::finish(int result)
{
    if (!result) cancel();

    DownloadDialog::finish(result);
}
