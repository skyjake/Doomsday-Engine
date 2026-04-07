/** @file remote/query.h
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

#ifndef DE_FILESYS_QUERY_H
#define DE_FILESYS_QUERY_H

#include "../asynccallback.h"
#include "../dictionaryvalue.h"
#include "../identifiedpacket.h"
#include "../hash.h"

namespace de {
namespace filesys {

class Link;

using QueryId = IdentifiedPacket::Id;

struct DE_PUBLIC RepositoryPath
{
    const Link *link = nullptr;
    String      localPath;
    String      remotePath;

    RepositoryPath() = default;

    RepositoryPath(const Link &link, const String &localPath, const String &remotePath)
        : link(&link)
        , localPath(localPath)
        , remotePath(remotePath)
    {}
};

using PackagePaths = Hash<String, RepositoryPath>;
using FileMetadata = std::function<void(const DictionaryValue &)>;
using FileContents = std::function<void(duint64 startOffset, const Block &, duint64 remainingBytes)>;

template <typename Callback>
using Request = std::shared_ptr<AsyncCallback<Callback>>;

/**
 * Query about information stored in the remote repository. The callbacks will
 * be called when a reply is received.
 */
struct DE_PUBLIC Query
{
    // Query parameters:
    QueryId    id;
    String     path;
    StringList packageIds;

    // Callbacks:
    Request<FileMetadata> fileMetadata;
    Request<FileContents> fileContents;

    // Internal status:
    duint64 receivedBytes = 0;
    duint64 fileSize      = 0;

public:
    Query(Request<FileMetadata> req, String path);
    Query(Request<FileContents> req, String path);
    bool isValid() const;
    void cancel();
};

} // namespace filesys
} // namespace de

#endif // DE_FILESYS_QUERY_H
