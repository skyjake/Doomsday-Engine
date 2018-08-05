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

#include "de/BlockValue"
#include "de/Folder"
#include "de/Path"
#include "de/RecordValue"
#include "de/RemoteFeedProtocol"
#include "de/RemoteFeedRelay"
#include "de/RemoteFile"
#include "de/Socket"
#include "de/TimeValue"
#include "de/Waitable"
#include "de/charsymbols.h"

namespace de {

static constexpr ddouble POPULATE_TIMEOUT = 15.0;

DE_PIMPL(RemoteFeed)
, DE_OBSERVES(filesys::RemoteFeedRelay, Status)
{
    String repository;
    Path remotePath;
    std::unique_ptr<DictionaryValue> fileMetadata;
    SafePtr<Folder> pendingPopulation;

    Impl(Public *i) : Base(i)
    {}

    PopulatedFiles populate()
    {
        PopulatedFiles populated;
        for (const auto &i : fileMetadata->elements())
        {
            String const path = remotePath / i.first.value->asText();

            if (RecordValue const *meta = maybeAs<RecordValue>(i.second))
            {
                const Record &   md       = *meta->record();
                const File::Type fileType = RemoteFeedMetadataPacket::toFileType(md.geti("type", 0));
                const dsize      fileSize = md.getui("size", 0);
                const Time       modTime  = md.getAs<TimeValue>("modifiedAt").time();

                File *file = nullptr;
                if (fileType == File::Type::File)
                {
                    file = new RemoteFile(path.fileName(), path, md.getAs<BlockValue>("metaId").block());
                }
                else
                {
                    Folder *subfolder = new Folder(path.fileName());
                    // Make a subfeed of this feed.
                    subfolder->attach(new RemoteFeed(*thisPublic, path));
                    file = subfolder;
                }
                if (md.has("package"))
                {
                    file->objectNamespace().add("package", new Record(md.subrecord("package")));
                }
                file->setStatus(File::Status(fileType, fileSize, modTime));
                file->setOriginFeed(thisPublic);

                populated << file;
            }
        }
        return populated;
    }

    void remoteRepositoryStatusChanged(String const &address, filesys::RemoteFeedRelay::Status status) override
    {
        if (repository == address && status == filesys::RemoteFeedRelay::Connected)
        {
            if (pendingPopulation)
            {
                // There is a pending population request, let's do it now.
                pendingPopulation->populate(Folder::PopulateAsyncFullTree);
                pendingPopulation.reset();
            }
            filesys::RemoteFeedRelay::get().audienceForStatus() -= this;
        }
    }
};

RemoteFeed::RemoteFeed(String const &repository, String const &remotePath)
    : d(new Impl(this))
{
    d->repository = repository;
    d->remotePath = remotePath;
}

RemoteFeed::RemoteFeed(RemoteFeed const &parentFeed, String const &remotePath)
    : d(new Impl(this))
{
    d->repository = parentFeed.d->repository;
    d->remotePath = remotePath;
}

String RemoteFeed::repository() const
{
    return d->repository;
}

String RemoteFeed::description() const
{
    return "remote repository \"" + d->repository / d->remotePath + "\"";
}

Feed::PopulatedFiles RemoteFeed::populate(Folder const &folder)
{
    LOG_AS("RemoteFeed");
    auto &relay = filesys::RemoteFeedRelay::get();
    PopulatedFiles files;
    if (!relay.isConnected(d->repository))
    {
        //qDebug() << "Population deferred:" << folder.path();
        d->pendingPopulation.reset(const_cast<Folder *>(&folder));
        relay.audienceForStatus() += d;
        return files;
    }
    auto request = relay.fetchFileList(
        d->repository, d->remotePath, [this, &files](const DictionaryValue &fileMetadata) {
            d->fileMetadata.reset(static_cast<DictionaryValue *>(fileMetadata.duplicate()));
            files = d->populate();
        });
    request->wait(POPULATE_TIMEOUT);
    return files;
}

bool RemoteFeed::prune(File &) const
{
    return false;
}

} // namespace de
