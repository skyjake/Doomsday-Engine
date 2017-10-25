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

DENG2_PIMPL(RemoteFeedRelay)
{
    QList<Link::Constructor> linkConstructors;
    QHash<String, filesys::Link *> repositories; // owned
    std::unique_ptr<QNetworkAccessManager> network;

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

    DENG2_PIMPL_AUDIENCE(Status)
};

DENG2_AUDIENCE_METHOD(RemoteFeedRelay, Status)

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

RemoteFeed *RemoteFeedRelay::addRepository(String const &address, String const &remoteRoot)
{
    for (auto constructor : d->linkConstructors)
    {
        if (auto *link = constructor(address))
        {
            d->repositories.insert(address, link);
            return new RemoteFeed(address, remoteRoot);
        }
    }
    return nullptr;
}

void RemoteFeedRelay::removeRepository(String const &address)
{
    if (auto *repo = d->repositories.take(address))
    {
        delete repo;
    }
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
    auto found = d->repositories.constFind(address);
    if (found != d->repositories.constEnd())
    {
        return found.value()->state() == filesys::Link::Ready;
    }
    return false;
}

FileListRequest
RemoteFeedRelay::fetchFileList(String const &repository, String folderPath, FileListFunc result)
{
    DENG2_ASSERT(d->repositories.contains(repository));

    Waitable done;
    FileListRequest request;
    Loop::mainCall([&] ()
    {
        // The repository sockets are handled in the main thread.
        auto *repo = d->repositories[repository];
        request.reset(new FileListRequest::element_type(result));
        repo->sendQuery(Query(request, folderPath));
        done.post();
    });
    done.wait();
    return request;
}

FileContentsRequest
RemoteFeedRelay::fetchFileContents(String const &repository, String filePath, DataReceivedFunc dataReceived)
{
    DENG2_ASSERT(d->repositories.contains(repository));

    Waitable done;
    FileContentsRequest request;
    Loop::mainCall([&] ()
    {
        // The repository sockets are handled in the main thread.
        auto *repo = d->repositories[repository];
        FileContentsRequest request(new FileContentsRequest::element_type(dataReceived));
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
