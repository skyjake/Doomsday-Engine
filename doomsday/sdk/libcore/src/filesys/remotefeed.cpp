/** @file remotefeed.cpp  Feed for remote files.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
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

#include "de/RemoteFeed"

#include "de/Path"
#include "de/RemoteFeedRelay"
#include "de/Socket"
#include "de/Waitable"
#include "de/charsymbols.h"

namespace de {

static TimeDelta const POPULATE_TIMEOUT = 10.0;

DENG2_PIMPL(RemoteFeed)
{
    Address backendAddress;
    Path remotePath;

    Impl(Public *i) : Base(i)
    {}
};

RemoteFeed::RemoteFeed(Address const &backend, String const &remotePath)
    : d(new Impl(this))
{
    d->backendAddress = backend;
    d->remotePath     = remotePath;
}

Address RemoteFeed::backend() const
{
    return d->backendAddress;
}

String RemoteFeed::description() const
{
    return String("Remote file repository %1 at path %2")
            .arg(d->backendAddress.asText())
            .arg(d->remotePath);
}

Feed::PopulatedFiles RemoteFeed::populate(Folder const &folder)
{
    PopulatedFiles files;
    auto request = RemoteFeedRelay::get().fetchFileList
            (d->backendAddress,
             d->remotePath,
             [this, &folder, &files]
             (RemoteFeedRelay::FileList const &fileList)
    {
        qDebug() << "Received file listing:";
        qDebug() << fileList.asText();

    });
    request->wait(POPULATE_TIMEOUT);
    return files;
}

bool RemoteFeed::prune(File &) const
{
    return false;
}

} // namespace de
