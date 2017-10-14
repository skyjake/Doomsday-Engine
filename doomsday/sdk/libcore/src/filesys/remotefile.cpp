/** @file remotefile.cpp  Remote file.
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

#include "de/RemoteFile"

namespace de {

DENG2_PIMPL(RemoteFile)
{
    String remotePath;
    Block remoteMetaId;

    Impl(Public *i) : Base(i) {}
};

RemoteFile::RemoteFile(String const &name, String const &remotePath, Block const &remoteMetaId)
    : File(name)
    , d(new Impl(this))
{
    d->remotePath = remotePath;
    d->remoteMetaId = remoteMetaId;
    setState(NotReady);
}

void RemoteFile::fetchContents()
{
    setState(Recovering);
}

String RemoteFile::describe() const
{
    return String("remote file \"%1\" (%2)")
            .arg(name())
            .arg(  state() == NotReady   ? "not ready"
                 : state() == Recovering ? "downloading"
                                         : "ready");
}

Block RemoteFile::metaId() const
{
    return d->remoteMetaId;
}

} // namespace de
