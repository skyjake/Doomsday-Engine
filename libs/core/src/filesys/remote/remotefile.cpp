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

#include "de/remotefile.h"

#include "de/app.h"
#include "de/directoryfeed.h"
#include "de/filesystem.h"
#include "de/filesys/remotefeedrelay.h"
#include "de/dscript.h"
#include "de/timevalue.h"

namespace de {

using namespace de::filesys;

const String RemoteFile::CACHE_PATH = "/home/cache/remote";

DE_PIMPL(RemoteFile)
{
    String remotePath;
    Block remoteMetaId;
    String repositoryAddress; // If empty, use feed's repository.
    Block buffer;
    Request<FileContents> fetching;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        if (fetching)
        {
            fetching->cancel();
        }
    }

    String cachePath() const
    {
        const String hex = remoteMetaId.asHexadecimalText();
        String path = CACHE_PATH / hex.right(CharPos(1));
        String original = self().objectNamespace().gets("package.path", remotePath);
        return path / hex + "_" + original.fileName();
    }

    void findCachedFile(bool requireExists = true)
    {
        if (!self().isBroken()) return;

        if (self().state() == NotReady)
        {
            throw UnfetchedError("RemoteFile::operator >>",
                                 self().description() + " not downloaded");
        }
        if (self().isBroken())
        {
            self().setTarget(FS::tryLocate<File const>(cachePath()));
        }
        if (requireExists && self().isBroken())
        {
            throw InputError("RemoteFile::operator >>",
                             self().description() + " has no locally cached data");
        }
    }

    void setTarget(const File &cachedFile)
    {
        self().setTarget(cachedFile);

        auto &ns = self().objectNamespace();
        if (ns.has("package.path"))
        {
            ns["package.path"] = self().target().path();
        }
    }

    bool checkExistingCache()
    {
        if (const File *cached = FS::tryLocate<File const>(cachePath()))
        {
            if (cached->status() == self().status())
            {
                // Seems to match (including part of the meta hash).
                LOG_RES_MSG("Using local cached copy of %s") << cached->description();
                setTarget(*cached);
                return true;
            }
        }
        return false;
    }

    String repository() const
    {
        if (repositoryAddress)
        {
            return repositoryAddress;
        }
        DE_ASSERT(is<RemoteFeed>(self().originFeed()));
        return self().originFeed()->as<RemoteFeed>().repository();
    }
};

RemoteFile::RemoteFile(const String &name, const String &remotePath, const Block &remoteMetaId,
                       const String &repositoryAddress)
    : LinkFile(name)
    , d(new Impl(this))
{
    objectNamespace().addSuperRecord(ScriptSystem::builtInClass(DE_STR("RemoteFile")));
    d->repositoryAddress = repositoryAddress;
    d->remotePath        = remotePath;
    d->remoteMetaId      = remoteMetaId;

    debug("RemoteFile remotePath: %s", remotePath.c_str());

    setState(NotReady);
}

void RemoteFile::download()
{
    if (state() != NotReady) return;

    setState(Recovering);

    if (d->checkExistingCache())
    {
        DE_NOTIFY_VAR(Download, i)
        {
            i->downloadProgress(*this, 0);
        }
        setState(Ready);
        return;
    }

    LOG_NET_MSG("Requesting download of \"%s\"") << name();

    d->fetching = filesys::RemoteFeedRelay::get().fetchFileContents
            (d->repository(),
             d->remotePath,
             [this] (duint64 startOffset, const Block &chunk, duint64 remainingBytes)
    {
        DE_ASSERT_IN_MAIN_THREAD();
        DE_NOTIFY_VAR(Download, i)
        {
            i->downloadProgress(*this, remainingBytes);
        }

        // Keep received data in a buffer.
        if (d->buffer.size() < remainingBytes)
        {
            d->buffer.resize(remainingBytes);
        }

        d->buffer.set(startOffset, chunk.data(), chunk.size());

        // When fully transferred, the file can be cached locally and interpreted.
        if (remainingBytes == 0)
        {
            LOG_NET_MSG("\"%s\" downloaded (%i bytes)") << name() << d->buffer.size();

            d->fetching = nullptr;

            const String fn = d->cachePath();
            Folder &cacheFolder = FS::get().makeFolder(fn.fileNamePath());
            File &data = cacheFolder.replaceFile(fn);
            data << d->buffer;
            d->buffer.clear();
            data.release();

            // Override the last modified time.
            {
                auto st = data.status();
                st.modifiedAt = status().modifiedAt;
                data.setStatus(st);

                // Remember this for later as well.
                DirectoryFeed::setFileModifiedTime(data.correspondingNativePath(),
                                                   st.modifiedAt);
            }

            setTarget(data.reinterpret());
            if (objectNamespace().has("package.path"))
            {
                objectNamespace()["package.path"] = target().path();
            }

            setState(Ready);

            // Now this RemoteFile can become the source of an interpreted file,
            // which replaces the RemoteFile within the parent folder.
        }
    });
}

void RemoteFile::cancelDownload()
{
    if (d->fetching)
    {
        d->fetching->cancel();
        d->fetching = nullptr;
        d->buffer.clear();
        setState(NotReady);
    }
}

void RemoteFile::deleteCache()
{
    setState(NotReady);
    FS::get().root().tryDestroyFile(d->cachePath());
}

const IIStream &RemoteFile::operator >> (IByteArray &bytes) const
{
    if (state() != Ready)
    {
        throw UnfetchedError("RemoteFile::operator >>",
                             description() + " not downloaded");
    }
    DE_ASSERT(!isBroken());
    return LinkFile::operator >> (bytes);
}

String RemoteFile::describe() const
{
    if (isReady())
    {
        return Stringf("\"%s\"", name().c_str());
    }
    String targetDesc;
    if (!isBroken())
    {
        targetDesc = " cached in " + target().description();
    }
    return Stringf("remote file \"%s\" (%s)%s",
                          name().c_str(),
                          state() == NotReady   ? "not ready"
                        : state() == Recovering ? "downloading"
                                                : "ready",
                          targetDesc.c_str());
}

Block RemoteFile::metaId() const
{
    return d->remoteMetaId;
}

Asset &RemoteFile::asset()
{
    return *this;
}

const Asset &RemoteFile::asset() const
{
    return *this;
}

dsize RemoteFile::downloadSize() const
{
    return size();
}

} // namespace de
