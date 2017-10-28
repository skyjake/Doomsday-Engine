/** @file
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

#include "de/filesys/WebHostedLink"

#include "de/Async"
#include "de/Folder"
#include "de/PathTree"
#include "de/RecordValue"
#include "de/RemoteFeedRelay"
#include "de/TextValue"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>

namespace de {
namespace filesys {

DENG2_PIMPL(WebHostedLink), public Lockable
{
    QSet<QNetworkReply *> pendingRequests;
    std::shared_ptr<FileTree> fileTree;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        foreach (auto *reply, pendingRequests)
        {
            reply->deleteLater();
        }
    }

    Block metaIdForFileEntry(FileEntry const &entry) const
    {
        if (entry.isBranch()) return Block(); // not applicable
        return md5Hash(self().address(), entry.path(), entry.size, entry.modTime);
    }

    void handleFileListQueryAsync(Query query)
    {
        QueryId const id = query.id;
        String const queryPath = query.path;
        self().scope() += async([this, queryPath] () -> std::shared_ptr<DictionaryValue>
        {
            DENG2_GUARD(this);
            if (auto const *dir = fileTree->tryFind
                    (queryPath, FileTree::MatchFull | FileTree::NoLeaf))
            {
                std::shared_ptr<DictionaryValue> list(new DictionaryValue);

                static String const VAR_TYPE       ("type");
                static String const VAR_MODIFIED_AT("modifiedAt");
                static String const VAR_SIZE       ("size");
                static String const VAR_META_ID    ("metaId");

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
            self().metadataReceived(id, list? *list : DictionaryValue());
        });
    }

    void handleReply(QueryId id, QNetworkReply *reply)
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

WebHostedLink::WebHostedLink(String const &address, String const &indexPath)
    : Link(address)
    , d(new Impl(this))
{
    // Fetch the repository index.
    {
        QNetworkRequest req(QUrl(address / indexPath));
        req.setRawHeader("User-Agent", Version::currentBuild().userAgent().toLatin1());

        QNetworkReply *reply = filesys::RemoteFeedRelay::get().network().get(req);
        QObject::connect(reply, &QNetworkReply::finished, [this, reply] ()
        {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
            {
                parseRepositoryIndex(reply->readAll());
            }
            else
            {
                handleError(reply->errorString());
                wasDisconnected();
            }
        });
    }
}

void WebHostedLink::setFileTree(FileTree *tree)
{
    DENG2_GUARD(d);
    d->fileTree.reset(tree);
}

WebHostedLink::FileTree const &WebHostedLink::fileTree() const
{
    return *d->fileTree;
}

filesys::PackagePaths WebHostedLink::locatePackages(StringList const &packageIds) const
{
    PackagePaths remotePaths;
    foreach (String packageId, packageIds)
    {
        if (String remotePath = findPackagePath(packageId))
        {
            remotePaths.insert(packageId,
                               RepositoryPath(*this, localRoot().path()/packageId, remotePath));
        }
    }
    return remotePaths;
}

void WebHostedLink::transmit(Query const &query)
{
    // We can answer population queries instantly because an index was
    // downloaded when the connection was opened.
    if (query.fileMetadata)
    {
        d->handleFileListQueryAsync(query);
        return;
    }

    String url = address();
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", Version::currentBuild().userAgent().toLatin1());

    // TODO: Configure the request.

    QNetworkReply *reply = RemoteFeedRelay::get().network().get(req);
    d->pendingRequests.insert(reply);

    auto const id = query.id;
    QObject::connect(reply, &QNetworkReply::finished, [this, id, reply] ()
    {
        d->handleReply(id, reply);
    });
}

Block WebHostedLink::FileEntry::metaId(Link const &link) const
{
    return md5Hash(link.address(), path(), size, modTime);
}

} // namespace filesys
} // namespace de
