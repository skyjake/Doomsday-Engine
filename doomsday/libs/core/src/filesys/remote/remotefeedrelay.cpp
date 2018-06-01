/** @file remotefeedrelay.cpp
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

#include "de/RemoteFeedRelay"

#include "de/App"
#include "de/Async"
#include "de/Date"
#include "de/DictionaryValue"
#include "de/filesys/Link"
#include "de/filesys/NativeLink"
#include "de/Loop"
#include "de/Message"
#include "de/RemoteFeedProtocol"
#include "de/Version"
#include "de/charsymbols.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>

#include <QFile>

namespace de {
namespace filesys {

DE_PIMPL(RemoteFeedRelay)
{
    std::unique_ptr<QNetworkAccessManager> network;
    QList<Link::Constructor> linkConstructors;
    QHash<String, filesys::Link *> repositories; // owned

    Impl(Public *i) : Base(i)
    {
        network.reset(new QNetworkAccessManager);

        auto *cache = new QNetworkDiskCache;
        String const dir = NativePath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                / "RemoteFiles";
        cache->setCacheDirectory(dir);
        network->setCache(cache);
    }

    ~Impl()
    {
        qDeleteAll(repositories.values());
    }

    DE_PIMPL_AUDIENCE(Status)
};

DE_AUDIENCE_METHOD(RemoteFeedRelay, Status)

RemoteFeedRelay &RemoteFeedRelay::get()
{
    return App::remoteFeedRelay();
}

RemoteFeedRelay::RemoteFeedRelay()
    : d(new Impl(this))
{
    // Built-in constructors.
    defineLink(NativeLink::construct);
}

void RemoteFeedRelay::defineLink(filesys::Link::Constructor linkConstructor)
{
    d->linkConstructors.push_front(linkConstructor);
}

void RemoteFeedRelay::addRepository(String const &address,
                                    String const &localRootPath)
{
    for (auto constructor : d->linkConstructors)
    {
        if (auto *link = constructor(address))
        {
            d->repositories.insert(address, link);
            link->setLocalRoot(localRootPath);
            return;
        }
    }
}

void RemoteFeedRelay::removeRepository(String const &address)
{
    if (auto *repo = d->repositories.take(address))
    {
        delete repo;
    }
}

Link *RemoteFeedRelay::repository(String const &address) const
{
    auto found = d->repositories.constFind(address);
    if (found != d->repositories.constEnd())
    {
        return found.value();
    }
    return nullptr;
}

StringList RemoteFeedRelay::repositories() const
{
    StringList repos;
    foreach (String a, d->repositories.keys())
    {
        repos << a;
    }
    return repos;
}

bool RemoteFeedRelay::isConnected(String const &address) const
{
    if (auto *repo = repository(address))
    {
        return repo->state() == Link::Ready;
    }
    return false;
}

PackagePaths RemoteFeedRelay::locatePackages(StringList const &packageIds) const
{
    PackagePaths located;
    foreach (auto *repo, d->repositories)
    {
        if (repo->state() == Link::Ready)
        {
            auto const paths = repo->locatePackages(packageIds);
            for (auto i = paths.begin(); i != paths.end(); ++i)
            {
                if (!located.contains(i.key()))
                {
                    located.insert(i.key(), i.value());
                }
            }
        }
    }
    return located;
}

Request<FileMetadata>
RemoteFeedRelay::fetchFileList(String const &repository, String folderPath, FileMetadata metadataReceived)
{
    DE_ASSERT(d->repositories.contains(repository));

    Waitable done;
    Request<FileMetadata> request;
    Loop::mainCall([&] ()
    {
        // The repository sockets are handled in the main thread.
        auto *repo = d->repositories[repository];
        request.reset(new Request<FileMetadata>::element_type(metadataReceived));
        repo->sendQuery(Query(request, folderPath));
        done.post();
    });
    done.wait();
    return request;
}

Request<FileContents>
RemoteFeedRelay::fetchFileContents(String const &repository, String filePath, FileContents contentsReceived)
{
    DE_ASSERT(d->repositories.contains(repository));

    Waitable done;
    Request<FileContents> request;
    Loop::mainCall([&] ()
    {
        // The repository sockets are handled in the main thread.
        auto *repo = d->repositories[repository];
        request.reset(new Request<FileContents>::element_type(contentsReceived));
        repo->sendQuery(Query(request, filePath));
        done.post();
    });
    done.wait();
    return request;
}

QNetworkAccessManager &RemoteFeedRelay::network()
{
    return *d->network;
}

} // namespace filesys
} // namespace de
