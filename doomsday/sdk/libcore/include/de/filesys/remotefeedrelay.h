/** @file remotefeedrelay.h  Manages one or more connections to remote feed repositories.
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

#ifndef LIBDENG2_REMOTEFEEDRELAY_H
#define LIBDENG2_REMOTEFEEDRELAY_H

#include "../RemoteFeed"
#include "../Record"
#include "../DictionaryValue"
#include "../AsyncCallback"

namespace de {

/**
 * Connects to one or more remote file repositories and provides metadata and file
 * contents over a network connection.
 */
class DENG2_PUBLIC RemoteFeedRelay
{
public:
    typedef DictionaryValue FileList;

    typedef std::function<void (FileList const &)> FileListFunc;
    typedef std::function<void (duint64 startOffset, Block const &, duint64 remainingBytes)> DataReceivedFunc;

    typedef std::shared_ptr<AsyncCallback<FileListFunc>>     FileListRequest;
    typedef std::shared_ptr<AsyncCallback<DataReceivedFunc>> FileContentsRequest;

    static RemoteFeedRelay &get();

public:
    RemoteFeedRelay();

    RemoteFeed *addServerRepository(String const &serverAddress);

    RemoteFeed *addRepository(String const &address);

    void removeRepository(String const &address);

    StringList repositories() const;

    FileListRequest fetchFileList(String const &repository,
                                  String folderPath,
                                  FileListFunc result);

    FileContentsRequest fetchFileContents(String const &repository,
                                          String filePath,
                                          DataReceivedFunc dataReceived);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_REMOTEFEEDRELAY_H
