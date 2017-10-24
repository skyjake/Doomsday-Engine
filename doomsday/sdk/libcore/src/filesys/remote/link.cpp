/** @file remote/link.cpp
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

#include "de/filesys/remote/link.h"

#include <de/App>
#include <de/Async>
#include <de/charsymbols.h>

namespace de {
namespace filesys {

DENG2_PIMPL(Link), public AsyncScope
{
    State state = Initializing;
    String address;
    QueryId nextQueryId = 1;
    QList<Query> deferredQueries;
    QHash<QueryId, Query> pendingQueries;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        cancelAllQueries();
    }

    void cancelAllQueries()
    {
        for (auto i = deferredQueries.begin(); i != deferredQueries.end(); ++i)
        {
            i->cancel();
        }
        for (auto i = pendingQueries.begin(); i != pendingQueries.end(); ++i)
        {
            i.value().cancel();
        }
    }

    /// Remove all invalid/cancelled queries.
    void cleanup()
    {
        QMutableHashIterator<QueryId, Query> iter(pendingQueries);
        while (iter.hasNext())
        {
            iter.next();
            if (!iter.value().isValid())
            {
                iter.remove();
            }
        }
    }

    void sendQuery(Query query)
    {
        try
        {
            query.id = nextQueryId++;
            if (state == Ready)
            {
                self().transmit(query);
                pendingQueries.insert(query.id, query);
                cleanup();
            }
            else
            {
                deferredQueries.append(query);
            }
        }
        catch (Error const &er)
        {
            LOG_NET_ERROR("Error sending file repository query: %s")
                    << er.asText();
        }
    }

    void sendDeferredQueries()
    {
        foreach (Query const &query, deferredQueries)
        {
            if (query.isValid())
            {
                self().transmit(query);
                pendingQueries.insert(query.id, query);
            }
        }
        deferredQueries.clear();
    }

    void notifyStatus(RemoteFeedRelay::Status status)
    {
        using Relay = RemoteFeedRelay;
        DENG2_FOR_EACH_OBSERVER(Relay::StatusAudience, i,
                                Relay::get().audienceForStatus())
        {
            i->remoteRepositoryStatusChanged(address, status);
        }
    }
};

Link::Link(String const &address)
    : d(new Impl(this))
{
    d->address = address;
}

String Link::address() const
{
    return d->address;
}

Link::State Link::state() const
{
    return d->state;
}

void Link::wasConnected()
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    d->state = Ready;
    d->sendDeferredQueries();
    d->notifyStatus(RemoteFeedRelay::Connected);
}

void Link::wasDisconnected()
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    d->state = Deinitialized;
    d->cancelAllQueries();
    d->cleanup();
    d->notifyStatus(RemoteFeedRelay::Disconnected);
}

void Link::handleError(QString errorMessage)
{
    LOG_NET_ERROR("Error accessing remote file repository \"%s\": %s " DENG2_CHAR_MDASH
                  " files from repository may not be available")
            << d->address
            << errorMessage;
}

AsyncScope &Link::scope()
{
    return *d;
}

void Link::cancelAllQueries()
{
    d->cancelAllQueries();
}

void Link::cleanupQueries()
{
    d->cleanup();
}

Link::Query *Link::findQuery(QueryId id)
{
    auto found = d->pendingQueries.find(id);
    if (found != d->pendingQueries.end())
    {
        return &found.value();
    }
    return nullptr;
}

void Link::sendQuery(Query query)
{
    try
    {
        query.id = d->nextQueryId++;
        if (d->state == Ready)
        {
            transmit(query);
            d->pendingQueries.insert(query.id, query);
            d->cleanup();
        }
        else
        {
            d->deferredQueries.append(query);
        }
    }
    catch (Error const &er)
    {
        LOG_NET_ERROR("Error sending file repository query: %s") << er.asText();
    }
}

void Link::metadataReceived(QueryId id, DictionaryValue const &metadata)
{
    if (auto *query = findQuery(id))
    {
        if (query->fileList)
        {
            query->fileList->call(metadata);
        }
        d->pendingQueries.remove(id);
    }
}

void Link::chunkReceived(QueryId id, duint64 startOffset, Block const &chunk, duint64 fileSize)
{
    if (auto *query = findQuery(id))
    {
        // Get rid of cancelled queries.
        if (!query->isValid())
        {
            d->pendingQueries.remove(id);
            return;
        }

        // Before the first chunk, notify about the total size.
        if (!query->fileSize)
        {
            query->fileContents->call(0, Block(), fileSize);
        }

        query->fileSize = fileSize;
        query->receivedBytes += chunk.size();

        // Notify about progress and provide the data chunk to the requestor.
        query->fileContents->call(startOffset, chunk,
                                  fileSize - query->receivedBytes);

        if (fileSize == query->receivedBytes)
        {
            // Transfer complete.
            d->pendingQueries.remove(id);
        }
    }
}

//---------------------------------------------------------------------------------------

Link::Query::Query(RemoteFeedRelay::FileListRequest req, String path)
    : path(path), fileList(req)
{}

Link::Query::Query(RemoteFeedRelay::FileContentsRequest req, String path)
    : path(path), fileContents(req)
{}

bool Link::Query::isValid() const
{
    if (fileList)     return fileList    ->isValid();
    if (fileContents) return fileContents->isValid();
    return false;
}

void Link::Query::cancel()
{
    if (fileList)     fileList    ->cancel();
    if (fileContents) fileContents->cancel();
}

} // namespace filesys
} // namespace de
