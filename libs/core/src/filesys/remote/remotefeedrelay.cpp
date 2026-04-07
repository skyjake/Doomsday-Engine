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

#include "de/filesys/remotefeedrelay.h"

#include "de/app.h"
#include "de/async.h"
#include "de/date.h"
#include "de/dictionaryvalue.h"
#include "de/filesys/link.h"
#include "de/filesys/nativelink.h"
#include "de/loop.h"
#include "de/message.h"
#include "de/remotefeedprotocol.h"
#include "de/regexp.h"
#include "de/version.h"
#include "de/charsymbols.h"

namespace de {
namespace filesys {

DE_PIMPL(RemoteFeedRelay)
{
    List<Link::Constructor>       linkConstructors;
    Hash<String, filesys::Link *> repositories; // owned

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        repositories.deleteAll();
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

void RemoteFeedRelay::defineLink(const Link::Constructor &linkConstructor)
{
    d->linkConstructors.push_front(linkConstructor);
}

void RemoteFeedRelay::addRepository(const String &address,
                                    const String &localRootPath)
{
    for (const auto &constructor : d->linkConstructors)
    {
        if (auto *link = constructor(address))
        {
            d->repositories.insert(address, link);
            link->setLocalRoot(localRootPath);
            return;
        }
    }
}

void RemoteFeedRelay::removeRepository(const String &address)
{
    if (auto *repo = d->repositories.take(address))
    {
        delete repo;
    }
}

Link *RemoteFeedRelay::repository(const String &address) const
{
    auto found = d->repositories.find(address);
    if (found != d->repositories.end())
    {
        return found->second;
    }
    return nullptr;
}

StringList RemoteFeedRelay::repositories() const
{
    StringList repos;
    for (const auto &a : d->repositories)
    {
        repos << a.first;
    }
    return repos;
}

bool RemoteFeedRelay::isConnected(const String &address) const
{
    if (auto *repo = repository(address))
    {
        return repo->state() == Link::Ready;
    }
    return false;
}

PackagePaths RemoteFeedRelay::locatePackages(const StringList &packageIds) const
{
    PackagePaths located;
    for (auto &r : d->repositories)
    {
        auto *repo = r.second;
        if (repo->state() == Link::Ready)
        {
            const auto paths = repo->locatePackages(packageIds);
            for (auto i = paths.begin(); i != paths.end(); ++i)
            {
                if (!located.contains(i->first))
                {
                    located.insert(i->first, i->second);
                }
            }
        }
    }
    return located;
}

Request<FileMetadata>
RemoteFeedRelay::fetchFileList(const String &repository, String folderPath, FileMetadata metadataReceived)
{
    DE_ASSERT(d->repositories.contains(repository));

    Waitable done;
    Request<FileMetadata> request;
    Loop::mainCall([&]() {
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
RemoteFeedRelay::fetchFileContents(const String &repository, String filePath, FileContents contentsReceived)
{
    DE_ASSERT(d->repositories.contains(repository));

    Waitable done;
    Request<FileContents> request;
    Loop::mainCall([&]() {
        // The repository sockets are handled in the main thread.
        auto *repo = d->repositories[repository];
        request.reset(new Request<FileContents>::element_type(contentsReceived));
        repo->sendQuery(Query(request, filePath));
        done.post();
    });
    done.wait();
    return request;
}

} // namespace filesys
} // namespace de
