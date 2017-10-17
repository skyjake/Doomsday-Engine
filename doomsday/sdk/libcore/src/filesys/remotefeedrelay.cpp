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
#include "de/Loop"
#include "de/Message"
#include "de/RemoteFeedProtocol"
#include "de/Socket"
#include "de/charsymbols.h"

namespace de {

DENG2_PIMPL(RemoteFeedRelay)
{
    /**
     * Active connection to a remote repository. One link is shared by all
     * RemoteFeed instances accessing the same repository.
     */
    struct RepositoryLink
    {
        RemoteFeedRelay::Impl *d;

        enum State { Deinitialized, Initializing, Ready };

        struct Query
        {
            using Id = IdentifiedPacket::Id;

            Id id;
            String path;
            FileListRequest fileList;
            FileContentsRequest fileContents;
            //Block receivedData;
            duint64 receivedBytes = 0;
            duint64 fileSize = 0;

            Query(FileListRequest req, String path) : path(path), fileList(req)
            {}

            Query(FileContentsRequest req, String path) : path(path), fileContents(req)
            {}

            bool isValid() const
            {
                if (fileList)     return fileList    ->isValid();
                if (fileContents) return fileContents->isValid();
                return false;
            }

            void cancel()
            {
                if (fileList)     fileList    ->cancel();
                if (fileContents) fileContents->cancel();
            }
        };

        State state = Initializing;
        String address;
        Query::Id nextQueryId = 1;
        QList<Query> deferredQueries;
        QHash<Query::Id, Query> pendingQueries;

        RepositoryLink(RemoteFeedRelay::Impl *d, String const &address) : d(d), address(address)
        {}

        virtual ~RepositoryLink()
        {
            // All queries will be cancelled.
            for (auto i = deferredQueries.begin(); i != deferredQueries.end(); ++i)
            {
                i->cancel();
            }
            for (auto i = pendingQueries.begin(); i != pendingQueries.end(); ++i)
            {
                i.value().cancel();
            }
        }

        virtual void wasConnected()
        {
            state = Ready;
            sendDeferredQueries();
        }

        virtual void wasDisconnected()
        {
            state = Deinitialized;
        }

        virtual void handleError(QString errorMessage)
        {
            LOG_NET_ERROR("Error accessing remote file repository \"%s\": %s " DENG2_CHAR_MDASH
                          "files from repository may not be available")
                    << address
                    << errorMessage;
        }

        void sendQuery(Query query)
        {
            try
            {
                query.id = nextQueryId++;
                if (state == Ready)
                {
                    transmit(query);
                    pendingQueries.insert(query.id, query);
                    cleanup();
                }
                else
                {
                    qDebug() << "[RemoteFeedRelay] Query" << query.id << "is deferred";
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
                    transmit(query);
                    pendingQueries.insert(query.id, query);
                }
            }
            deferredQueries.clear();
        }

        void metadataReceived(Query::Id id, DictionaryValue const &metadata)
        {
            auto found = pendingQueries.find(id);
            if (found != pendingQueries.end())
            {
                auto &query = found.value();
                if (query.fileList)
                {
                    query.fileList->call(metadata);
                }
                pendingQueries.erase(found);
            }
        }

        void chunkReceived(Query::Id id, duint64 startOffset, Block const &chunk, duint64 fileSize)
        {
            auto found = pendingQueries.find(id);
            if (found != pendingQueries.end())
            {
                auto &query = found.value();
                if (!query.isValid())
                {
                    pendingQueries.erase(found);
                    return;
                }
                /*if (query.receivedData.size() != fileSize)
                {
                    query.receivedData.resize(fileSize);
                }*/
                if (!query.fileSize)
                {
                    // Before the first chunk, notify about the total size.
                    qDebug() << "notifying full file size:" << fileSize;
                    query.fileContents->call(0, Block(), fileSize);
                }
                //query.receivedData.set(startOffset, chunk.data(), chunk.size());
                query.fileSize = fileSize;
                query.receivedBytes += chunk.size();

                // Notify about progress.
                qDebug() << "notifying chunk with" << chunk.size() << "bytes";
                query.fileContents->call(startOffset, chunk, fileSize - query.receivedBytes);

                if (fileSize == query.receivedBytes)
                {
                    // Transfer complete.
                    pendingQueries.erase(found);
                }
            }
        }

        /// Remove all invalid/cancelled queries.
        void cleanup()
        {
            QMutableHashIterator<Query::Id, Query> iter(pendingQueries);
            while (iter.hasNext())
            {
                iter.next();
                if (!iter.value().isValid())
                {
                    iter.remove();
                }
            }
        }

    protected:
        virtual void transmit(Query const &query) = 0;
    };

    /**
     * Link to a native Doomsday remote repository (see RemoteFeedUser on server).
     */
    struct NativeRepositoryLink : public RepositoryLink
    {
        Socket socket;

        NativeRepositoryLink(RemoteFeedRelay::Impl *d, String const &address)
            : RepositoryLink(d, address)
        {
            QObject::connect(&socket, &Socket::connected,     [this] () { wasConnected(); });
            QObject::connect(&socket, &Socket::disconnected,  [this] () { wasDisconnected(); });
            QObject::connect(&socket, &Socket::messagesReady, [this] () { receiveMessages(); });
            QObject::connect(&socket, &Socket::error,         [this] (QString msg) { handleError(msg); });

            socket.open(address);
        }

        void wasConnected() override
        {
            socket << ByteRefArray("RemoteFeed", 10);
            RepositoryLink::wasConnected();
        }

        void transmit(Query const &query) override
        {
            DENG2_ASSERT(query.isValid());

            qDebug() << "transmitting query" << query.id << query.path;

            RemoteFeedQueryPacket packet;
            packet.setId(query.id);
            packet.setPath(query.path);
            if (query.fileList)
            {
                packet.setQuery(RemoteFeedQueryPacket::ListFiles);
            }
            else if (query.fileContents)
            {
                packet.setQuery(RemoteFeedQueryPacket::FileContents);
            }
            socket.sendPacket(packet);
        }

        void receiveMessages()
        {
            while (socket.hasIncoming())
            {
                try
                {
                    DENG2_ASSERT_IN_MAIN_THREAD();

                    std::unique_ptr<Message> response(socket.receive());
                    std::unique_ptr<Packet>  packet  (d->protocol.interpret(*response));

                    if (!packet) continue;

                    switch (d->protocol.recognize(*packet))
                    {
                    case RemoteFeedProtocol::Metadata: {
                        auto const &md = packet->as<RemoteFeedMetadataPacket>();
                        metadataReceived(md.id(), md.metadata());
                        break; }

                    case RemoteFeedProtocol::FileContents: {
                        auto const &fc = packet->as<RemoteFeedFileContentsPacket>();
                        chunkReceived(fc.id(), fc.startOffset(), fc.data(), fc.fileSize());
                        break; }

                    default:
                        break;
                    }
                }
                catch (Error const &er)
                {
                    LOG_NET_ERROR("Error when handling remote feed response: %s")
                            << er.asText();
                }
            }
            cleanup();
        }
    };

    RemoteFeedProtocol protocol;
    QHash<String,       RepositoryLink *> repositories; // owned
    QHash<QHostAddress, RepositoryLink *> repositoriesByHost; // not owned

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        qDeleteAll(repositories.values());
    }
};

RemoteFeedRelay &RemoteFeedRelay::get()
{
    return App::remoteFeedRelay();
}

RemoteFeedRelay::RemoteFeedRelay()
    : d(new Impl(this))
{}

RemoteFeed *RemoteFeedRelay::addServerRepository(String const &serverAddress, String const &remoteRoot)
{
    auto *repo = new Impl::NativeRepositoryLink(d, serverAddress);
    d->repositories.insert(serverAddress, repo);
    return new RemoteFeed(serverAddress, remoteRoot);
}

RemoteFeed *RemoteFeedRelay::addRepository(String const &address)
{
    return nullptr;
}

void RemoteFeedRelay::removeRepository(const de::String &address)
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

RemoteFeedRelay::FileListRequest
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
        repo->sendQuery(Impl::RepositoryLink::Query(request, folderPath));
        done.post();
    });
    done.wait();
    return request;
}

RemoteFeedRelay::FileContentsRequest
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
        repo->sendQuery(Impl::RepositoryLink::Query(request, filePath));
        done.post();
    });
    done.wait();
    return request;
}

} // namespace de
