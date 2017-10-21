/** @file packagedownloader.cpp  Utility for downloading packages from a remote repository.
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

#include "network/packagedownloader.h"

#include <de/App>
#include <de/FileSystem>
#include <de/Folder>
#include <de/Garbage>
#include <de/LinkFile>
#include <de/Loop>
#include <de/PackageLoader>
#include <de/RemoteFeedRelay>
#include <de/RemoteFile>
#include <numeric>

using namespace de;

static String const PATH_REMOTE_PACKS  = "/remote/packs";
static String const PATH_REMOTE_SERVER = "/remote/server"; // local folder for RemoteFeed
static String const PATH_SERVER_REPOSITORY_ROOT = "/sys/server/files"; // serverside folder

DENG2_PIMPL(PackageDownloader)
, DENG2_OBSERVES(Asset, StateChange)
, DENG2_OBSERVES(RemoteFile, Download)
, DENG2_OBSERVES(Deletable, Deletion)
{
    String fileRepository;
    AssetGroup downloads;
    QHash<RemoteFile *, dsize> downloadBytesRemaining;
    std::function<void ()> postDownloadCallback;

    Impl(Public *i) : Base(i)
    {}

    void downloadFile(File &file)
    {
        if (auto *folder = maybeAs<Folder>(file))
        {
            folder->forContents([this] (String, File &f)
            {
                downloadFile(f);
                return LoopContinue;
            });
        }
        if (auto *remoteFile = maybeAs<RemoteFile>(file))
        {
            LOG_NET_VERBOSE("Downloading from server: %s") << remoteFile->description();
            downloads.insert(*remoteFile);
            remoteFile->audienceForDownload() += this;
            remoteFile->Deletable::audienceForDeletion += this;
            downloadBytesRemaining.insert(remoteFile, remoteFile->size());
            remoteFile->fetchContents();
        }
    }

    void remoteFileDownloading(RemoteFile &file, dsize remainingBytes) override
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        auto found = downloadBytesRemaining.find(&file);
        if (found == downloadBytesRemaining.end()) return;

        found.value() = remainingBytes;

        if (remainingBytes == 0)
        {
            file.audienceForDownload() -= this;
            file.Deletable::audienceForDeletion -= this;
            downloadBytesRemaining.erase(found);
        }

        // Update total for the UI popup.
        {
            dsize const totalRemaining = std::accumulate(downloadBytesRemaining.constBegin(),
                                                         downloadBytesRemaining.constEnd(),
                                                         dsize(0));
            qDebug() << "Total remaining:" << totalRemaining;
        }
    }

    void objectWasDeleted(Deletable *del) override
    {
        DENG2_ASSERT_IN_MAIN_THREAD();
        downloadBytesRemaining.remove(static_cast<RemoteFile *>(del));
    }

    void assetStateChanged(Asset &) override
    {
        DENG2_ASSERT(!downloads.isEmpty());
        if (downloads.isReady())
        {
            LOG_NET_VERBOSE("All downloads of remote files finished");
            Loop::mainCall([this] ()
            {
                DENG2_ASSERT(downloadBytesRemaining.isEmpty());
                if (postDownloadCallback) postDownloadCallback();
            });
        }
    }

    void clearDownloads()
    {
        for (auto i = downloadBytesRemaining.begin(); i != downloadBytesRemaining.end(); ++i)
        {
            i.key()->audienceForDownload() -= this;
            i.key()->Deletable::audienceForDeletion -= this;
        }
        downloadBytesRemaining.clear();
        downloads.clear();
        postDownloadCallback = nullptr;
    }

    /**
     * Makes remote packages available for loading locally.
     *
     * Once remote files have been downloaded, PackageLoader still needs to be made aware
     * that the packages are available. This is done via link files that have the ".pack"
     * extension and thus are treated as loadable packages.
     *
     * @param ids  Identifiers of remote packages that have been downloaded and are now
     *             being prepared for loading.
     */
    void linkRemotePackages(StringList const &ids)
    {
        Folder &remotePacks = FS::get().makeFolder(PATH_REMOTE_PACKS);
        foreach (String pkgId, ids)
        {
            LOG_RES_VERBOSE("Registering remote package \"%s\"") << pkgId;
            if (auto *file = FS::tryLocate<File>(PATH_REMOTE_SERVER / pkgId))
            {
                LOGDEV_RES_VERBOSE("Cached metadata:\n") << file->objectNamespace().asText();

                auto *pack = LinkFile::newLinkToFile(*file, file->name() + ".pack");
                Record &meta = pack->objectNamespace();
                meta.add("package", new Record(file->objectNamespace().subrecord("package")));
                meta.set("package.path", file->path());
                remotePacks.add(pack);
                FS::get().index(*pack);

                LOG_RES_VERBOSE("\"%s\" linked as ") << pkgId << pack->path();
            }
        }
    }

    void unlinkRemotePackages()
    {
        // Unload all server packages. Note that the entire folder will be destroyed, too.
        if (std::unique_ptr<Folder> remotePacks { FS::tryLocate<Folder>(PATH_REMOTE_PACKS) })
        {
            remotePacks->forContents([] (String, File &file)
            {
                LOG_RES_VERBOSE("Unloading remote package: ") << file.description();
                PackageLoader::get().unload(Package::identifierForFile(file));
                return LoopContinue;
            });
        }
    }
};

PackageDownloader::PackageDownloader()
    : d(new Impl(this))
{}

String PackageDownloader::fileRepository() const
{
    return d->fileRepository;
}

void PackageDownloader::mountFileRepository(shell::ServerInfo const &info)
{
    d->fileRepository = info.address().asText();
    FS::get().makeFolderWithFeed
            (PATH_REMOTE_SERVER,
             RemoteFeedRelay::get().addServerRepository(d->fileRepository, PATH_SERVER_REPOSITORY_ROOT),
             Folder::PopulateAsyncFullTree);
}

void PackageDownloader::unmountFileRepository()
{
    d->clearDownloads();
    d->unlinkRemotePackages();
    if (Folder *remoteFiles = FS::tryLocate<Folder>(PATH_REMOTE_SERVER))
    {
        trash(remoteFiles);
    }
    RemoteFeedRelay::get().removeRepository(d->fileRepository);
    d->fileRepository.clear();
}

void PackageDownloader::download(StringList packageIds, std::function<void ()> callback)
{
    d->downloads.clear();
    foreach (String missing, packageIds)
    {
        if (File *rem = FS::tryLocate<File>(PATH_REMOTE_SERVER / missing))
        {
            d->downloadFile(*rem);
        }
    }

    auto finished = [this, packageIds, callback] ()
    {
        // Let's finalize the downloads so we can load all the packages.
        d->downloads.audienceForStateChange() -= d;
        d->downloads.clear();

        d->linkRemotePackages(packageIds);

        callback();
    };

    // If nothing needs to be downloaded, let's just continue right away.
    if (d->downloads.isReady())
    {
        d->postDownloadCallback = nullptr;
        finished();
    }
    else
    {
        d->postDownloadCallback = finished;
        d->downloads.audienceForStateChange() += d;
    }
}
