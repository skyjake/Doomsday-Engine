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
#include "de/RemoteFeedRelay"
#include "de/App"
#include "de/FileSystem"
#include "de/ScriptSystem"

namespace de {

static String const CACHE_PATH = "/home/cache/remote";

DENG2_PIMPL(RemoteFile)
{
    String remotePath;
    Block remoteMetaId;
    Block buffer;
    RemoteFeedRelay::FileContentsRequest fetching;
    SafePtr<File const> cachedFile;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        if (fetching)
        {
            fetching->cancel();
        }
    }

    void checkCache()
    {
        if (self().state() != Ready)
        {
            throw UnfetchedError("RemoteFile::operator >>",
                                 self().description() + " not downloaded");
        }
        if (!cachedFile)
        {
            cachedFile.reset(FS::tryLocate<File const>
                             (CACHE_PATH / remoteMetaId.asHexadecimalText()));
            if (!cachedFile)
            {
                throw InputError("RemoteFile::operator >>",
                                 self().description() + " has no locally cached data although marked Ready");
            }
        }
    }
};

RemoteFile::RemoteFile(String const &name, String const &remotePath, Block const &remoteMetaId)
    : ByteArrayFile(name)
    , d(new Impl(this))
{
    objectNamespace().addSuperRecord(ScriptSystem::builtInClass(QStringLiteral("RemoteFile")));
    d->remotePath = remotePath;
    d->remoteMetaId = remoteMetaId;
    setState(NotReady);
}

void RemoteFile::fetchContents()
{
    DENG2_ASSERT(is<RemoteFeed>(originFeed()));

    if (state() != NotReady) return;

    setState(Recovering);

    d->fetching = RemoteFeedRelay::get().fetchFileContents
            (originFeed()->as<RemoteFeed>().repository(),
             d->remotePath,
             [this] (duint64 startOffset, Block const &chunk, duint64 remainingBytes)
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        qDebug() << "[RemoteFile]" << d->remotePath << startOffset
                 << "remaining:" << remainingBytes;

        // Keep received data in a buffer.
        if (d->buffer.size() < remainingBytes)
        {
            d->buffer.resize(remainingBytes);
        }
        d->buffer.set(startOffset, chunk.data(), chunk.size());

        // When fully transferred, the file can be cached locally and interpreted.
        if (remainingBytes == 0)
        {
            qDebug() << "[RemoteFile] Complete contents received" << d->buffer.size();
            d->fetching = nullptr;

            Folder &cacheFolder = FS::get().makeFolder(CACHE_PATH);
            File &data = cacheFolder.replaceFile(d->remoteMetaId.asHexadecimalText());
            data << d->buffer;
            data.flush();

            d->buffer.clear();
            d->cachedFile.reset(&data);
            setState(Ready);

            // Now this RemoteFile can become the source of an interpreted file,
            // which replaces the RemoteFile within the parent folder.
            reinterpret();
        }
    });
}

void RemoteFile::get(Offset at, Byte *values, Size count) const
{
    d->checkCache();
    if (auto *array = maybeAs<IByteArray>(d->cachedFile))
    {
        array->get(at, values, count);
    }
}

void RemoteFile::set(Offset, Byte const *, Size)
{
    verifyWriteAccess();
}

IIStream const &RemoteFile::operator >> (IByteArray &bytes) const
{
    d->checkCache();
    *d->cachedFile >> bytes;
    return *this;
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
