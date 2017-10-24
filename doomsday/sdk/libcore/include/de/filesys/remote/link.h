/** @file remote/link.h
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

#ifndef DENG2_FILESYS_LINK_H
#define DENG2_FILESYS_LINK_H

#include "../../DictionaryValue"
#include "../../IdentifiedPacket"
#include "../../RemoteFeedRelay"
#include "../../String"

namespace de {

class AsyncScope;

namespace filesys {

/**
 * Active connection to a remote repository. One link is shared by all
 * RemoteFeed instances accessing the same repository.
 */
class DENG2_PUBLIC Link
{
public:
    enum State { Deinitialized, Initializing, Ready };

    using QueryId = IdentifiedPacket::Id;

    struct DENG2_PUBLIC Query
    {
        QueryId id;
        String path;
        RemoteFeedRelay::FileListRequest fileList;
        RemoteFeedRelay::FileContentsRequest fileContents;
        duint64 receivedBytes = 0;
        duint64 fileSize = 0;

        Query(RemoteFeedRelay::FileListRequest req, String path);
        Query(RemoteFeedRelay::FileContentsRequest req, String path);
        bool isValid() const;
        void cancel();
    };

public:
    Link(String const &address);

    virtual ~Link();

    String address() const;

    State state() const;

    virtual void wasConnected();

    virtual void wasDisconnected();

    virtual void handleError(QString errorMessage);

    void sendQuery(Query query);

protected:
    AsyncScope &scope();

    Query *findQuery(QueryId id);

    void cancelAllQueries();

    void cleanupQueries();

    void metadataReceived(QueryId id, DictionaryValue const &metadata);

    void chunkReceived(QueryId id, duint64 startOffset, Block const &chunk, duint64 fileSize);

    virtual void transmit(Query const &query) = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // DENG2_REMOTE_REPOSITORYLINK_H
