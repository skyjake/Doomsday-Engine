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

#ifndef DENG2_FILESYS_QUERY_H
#define DENG2_FILESYS_QUERY_H

#include "../../AsyncCallback"
#include "../../DictionaryValue"
#include "../../IdentifiedPacket"

namespace de {
namespace filesys {

typedef DictionaryValue FileList;

typedef std::function<void (FileList const &)> FileListFunc;
typedef std::function<void (duint64 startOffset, Block const &, duint64 remainingBytes)> DataReceivedFunc;

typedef std::shared_ptr<AsyncCallback<FileListFunc>>     FileListRequest;
typedef std::shared_ptr<AsyncCallback<DataReceivedFunc>> FileContentsRequest;

using QueryId = IdentifiedPacket::Id;

/**
 * Query about information stored in the remote repository. The callbacks will
 * be called when a reply is received.
 */
struct DENG2_PUBLIC Query
{
    // Query parameters:
    QueryId id;
    String path;

    // Callbacks:
    FileListRequest fileList;
    FileContentsRequest fileContents;

    // Internal status:
    duint64 receivedBytes = 0;
    duint64 fileSize = 0;

    Query(FileListRequest req, String path);
    Query(FileContentsRequest req, String path);
    bool isValid() const;
    void cancel();
};

} // namespace filesys
} // namespace de

#endif // DENG2_FILESYS_QUERY_H
