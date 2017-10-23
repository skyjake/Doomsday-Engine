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
#include "de/Loop"
#include "de/Message"
#include "de/PathTree"
#include "de/RecordValue"
#include "de/RemoteFeedProtocol"
#include "de/Socket"
#include "de/TextValue"
#include "de/Version"
#include "de/charsymbols.h"
#include "de/data/gzip.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>

#include <QFile>

namespace de {

DENG2_PIMPL(RemoteFeedRelay)
{
    /**
     * Active connection to a remote repository. One link is shared by all
     * RemoteFeed instances accessing the same repository.
     */
    struct RepositoryLink : protected AsyncScope
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
            cancelAllQueries();
        }

        virtual void wasConnected()
        {
            DENG2_ASSERT_IN_MAIN_THREAD();
            state = Ready;
            sendDeferredQueries();
            DENG2_FOR_EACH_OBSERVER(StatusAudience, i, d->audienceForStatus)
            {
                i->remoteRepositoryStatusChanged(address, Connected);
            }
        }

        virtual void wasDisconnected()
        {
            state = Deinitialized;
            cancelAllQueries();
            cleanup();
            DENG2_FOR_EACH_OBSERVER(StatusAudience, i, d->audienceForStatus)
            {
                i->remoteRepositoryStatusChanged(address, Disconnected);
            }
        }

        virtual void handleError(QString errorMessage)
        {
            LOG_NET_ERROR("Error accessing remote file repository \"%s\": %s " DENG2_CHAR_MDASH
                          " files from repository may not be available")
                    << address
                    << errorMessage;
        }

        void cancelAllQueries()
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

        Query *findQuery(Query::Id id)
        {
            auto found = pendingQueries.find(id);
            if (found != pendingQueries.end())
            {
                return &found.value();
            }
            return nullptr;
        }

        void metadataReceived(Query::Id id, DictionaryValue const &metadata)
        {
            if (auto *query = findQuery(id))
            {
                if (query->fileList)
                {
                    query->fileList->call(metadata);
                }
                pendingQueries.remove(id);
            }
        }

        void chunkReceived(Query::Id id, duint64 startOffset, Block const &chunk,duint64 fileSize)
        {
            if (auto *query = findQuery(id))
            {
                // Get rid of cancelled queries.
                if (!query->isValid())
                {
                    pendingQueries.remove(id);
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
                    pendingQueries.remove(id);
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

    /**
     * Repository of files hosted on a web server as a file tree. Assumed to come
     * with a Unix-style "ls-laR.gz" directory tree index (e.g., an idgames mirror).
     */
    struct WebRepositoryLink : public RepositoryLink
    {
        QSet<QNetworkReply *> pendingRequests;

        struct FileEntry : public PathTree::Node
        {
            duint64 size = 0;
            Time modTime;

            FileEntry(PathTree::NodeArgs const &args) : Node(args) {}
            FileEntry() = delete;
        };
        using FileTree = PathTreeT<FileEntry>;
        LockableT<std::shared_ptr<FileTree>> fileTree;

        WebRepositoryLink(RemoteFeedRelay::Impl *d, String const &address)
            : RepositoryLink(d, address)
        {
            // Fetch the repository index.
            {
                QNetworkRequest req(QUrl(address / "ls-laR.gz"));
                req.setRawHeader("User-Agent", Version::currentBuild().userAgent().toLatin1());

                QNetworkReply *reply = d->network->get(req);
                QObject::connect(reply, &QNetworkReply::finished, [this, reply] ()
                {
                    reply->deleteLater();
                    if (reply->error() == QNetworkReply::NoError)
                    {
                        parseUnixDirectoryListing(reply->readAll());
                    }
                    else
                    {
                        handleError(reply->errorString());
                        wasDisconnected();
                    }
                });
            }
        }

        void parseUnixDirectoryListing(QByteArray data)
        {
            // This may be a long list, so let's do it in a background thread.
            // The link will be marked connected only after the data has been parsed.

            *this += async([this, data] () -> String
            {
                Block const listing = gDecompress(data);
                QTextStream is(listing, QIODevice::ReadOnly);
                is.setCodec("UTF-8");
                QRegularExpression const reDir("^\\.?(.*):$");
                QRegularExpression const reTotal("^total\\s+\\d+$");
                QRegularExpression const reFile("^(-|d)[-rwxs]+\\s+\\d+\\s+\\w+\\s+\\w+\\s+"
                                                "(\\d+)\\s+(\\w+\\s+\\d+\\s+[0-9:]+)\\s+(.*)$",
                                                QRegularExpression::CaseInsensitiveOption);
                String currentPath;
                bool ignore = false;
                QRegularExpression const reIncludedPaths("^/(levels|music|sounds|themes)");

                std::shared_ptr<FileTree> tree(new FileTree);
                while (!is.atEnd())
                {
                    if (String const line = is.readLine().trimmed())
                    {
                        if (!currentPath)
                        {
                            // This should be a directory path.
                            auto match = reDir.match(line);
                            if (match.hasMatch())
                            {
                                currentPath = match.captured(1);
                                qDebug() << "[WebRepositoryLink] Parsing path:" << currentPath;

                                ignore = !reIncludedPaths.match(currentPath).hasMatch();
                            }
                        }
                        else if (!ignore && reTotal.match(line).hasMatch())
                        {
                            // Ignore directory sizes.
                        }
                        else if (!ignore)
                        {
                            auto match = reFile.match(line);
                            if (match.hasMatch())
                            {
                                bool const isFolder = (match.captured(1) == QStringLiteral("d"));
                                if (!isFolder)
                                {
                                    String const name = match.captured(4);
                                    if (name.startsWith(QChar('.')) || name.contains(" -> "))
                                        continue;

                                    auto &entry = tree->insert(currentPath / name);
                                    entry.size = match.captured(2).toULongLong(nullptr, 10);;
                                    entry.modTime = Time::fromText(match.captured(3), Time::UnixLsStyleDateTime);
                                }
                            }
                        }
                    }
                    else
                    {
                        currentPath.clear();
                    }
                }
                qDebug() << "file tree contains" << tree->size() << "entries";
                {
                    // It is now ready for use.
                    DENG2_GUARD(fileTree);
                    fileTree.value = tree;
                }
                return String();
            },
            [this] (String const &errorMessage)
            {
                if (!errorMessage)
                {
                    wasConnected();
                }
                else
                {
                    handleError("Failed to parse directory listing: " + errorMessage);
                    wasDisconnected();
                }
            });
        }

        ~WebRepositoryLink()
        {
            foreach (auto *reply, pendingRequests)
            {
                reply->deleteLater();
            }
        }

        void transmit(Query const &query) override
        {
            // We can answer population queries instantly.
            if (query.fileList)
            {
                handleFileListQueryAsync(query);
                return;
            }

            String url = address;
            QNetworkRequest req(url);
            req.setRawHeader("User-Agent", Version::currentBuild().userAgent().toLatin1());

            // TODO: Configure the request.

            QNetworkReply *reply = d->network->get(req);
            pendingRequests.insert(reply);

            auto const id = query.id;
            QObject::connect(reply, &QNetworkReply::finished, [this, id, reply] ()
            {
                handleReply(id, reply);
            });
        }

        Block metaIdForFileEntry(FileEntry const &entry) const
        {
            if (entry.isBranch()) return Block(); // not applicable

            Block data;
            Writer writer(data);
            writer << address << entry.path() << entry.size << entry.modTime;
            return data.md5Hash();
        }

        void handleFileListQueryAsync(Query query)
        {
            Query::Id id = query.id;
            String const queryPath = query.path;
            *this += async([this, queryPath] () -> std::shared_ptr<DictionaryValue>
            {
                DENG2_GUARD(fileTree);
                if (auto const *dir = fileTree.value->tryFind
                        (queryPath, FileTree::MatchFull | FileTree::NoLeaf))
                {
                    std::shared_ptr<DictionaryValue> list(new DictionaryValue);

                    static String const VAR_TYPE("type");
                    static String const VAR_MODIFIED_AT("modifiedAt");
                    static String const VAR_SIZE("size");
                    static String const VAR_META_ID("metaId");

                    auto addMeta = [this]
                            (DictionaryValue &list, PathTree::Nodes const &nodes)
                    {
                        for (auto i = nodes.begin(); i != nodes.end(); ++i)
                        {
                            auto const &entry = i.value()->as<FileEntry>();
                            list.add(new TextValue(entry.name()),
                                      RecordValue::takeRecord(
                                          Record::withMembers(
                                              VAR_TYPE, entry.isLeaf()? 0 : 1,
                                              VAR_SIZE, entry.size,
                                              VAR_MODIFIED_AT, entry.modTime,
                                              VAR_META_ID, metaIdForFileEntry(entry)
                                          )));
                        }
                    };

                    addMeta(*list.get(), dir->children().branches);
                    addMeta(*list.get(), dir->children().leaves);

                    return list;
                }
                return nullptr;
            },
            [this, id] (std::shared_ptr<DictionaryValue> list)
            {
                metadataReceived(id, list? *list : DictionaryValue());
            });
        }

        void handleReply(Query::Id id, QNetworkReply *reply)
        {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
            {
                QByteArray const data = reply->readAll();

            }
            else
            {
                LOG_NET_WARNING(reply->errorString());
            }
        }

    };

    RemoteFeedProtocol protocol;
    QHash<String, RepositoryLink *> repositories; // owned
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
{}

RemoteFeed *RemoteFeedRelay::addServerRepository(String const &serverAddress, String const &remoteRoot)
{
    auto *repo = new Impl::NativeRepositoryLink(d, serverAddress);
    d->repositories.insert(serverAddress, repo);
    return new RemoteFeed(serverAddress, remoteRoot);
}

RemoteFeed *RemoteFeedRelay::addRepository(String const &address)
{
    auto *repo = new Impl::WebRepositoryLink(d, address);
    d->repositories.insert(address, repo);
    return new RemoteFeed(address);
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

bool RemoteFeedRelay::isConnected(String const &address) const
{
    auto found = d->repositories.constFind(address);
    if (found != d->repositories.constEnd())
    {
        return found.value()->state == Impl::RepositoryLink::Ready;
    }
    return false;
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
