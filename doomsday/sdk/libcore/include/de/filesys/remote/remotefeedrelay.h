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

#include "../../RemoteFeed"
#include "../../Record"
#include "../../DictionaryValue"
#include "../../AsyncCallback"
#include "../Query"
#include "../Link"

#include <QNetworkAccessManager>

namespace de {
namespace filesys {

/**
 * Connects to one or more remote file repositories and provides metadata and file
 * contents over a network connection.
 */
class DENG2_PUBLIC RemoteFeedRelay
{
public:
    static RemoteFeedRelay &get();

//    enum RepositoryType {
//        Server,
//        NativePackages,
//        IdgamesFileTree,
//    };

    enum Status { Disconnected, Connected };

    DENG2_DEFINE_AUDIENCE2(Status, void remoteRepositoryStatusChanged(String const &address, Status))

public:
    RemoteFeedRelay();

    /**
     * Defines a new type of remote repository link. The defined links are each
     * offered a remote repository address, and the first one to create a Link instance
     * based on the address will be used to communicate with the repository.
     *
     * @param linkConstructor  Constructor method.
     */
    void defineLink(Link::Constructor linkConstructor);

    RemoteFeed *addRepository(String const &address, String const &remoteRoot = "/");

    void removeRepository(String const &address);

    StringList repositories() const;

    bool isConnected(String const &address) const;

    FileListRequest fetchFileList(String const &repository,
                                  String folderPath,
                                  FileListFunc result);

    FileContentsRequest fetchFileContents(String const &repository,
                                          String filePath,
                                          DataReceivedFunc dataReceived);

    QNetworkAccessManager &network();

private:
    DENG2_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // LIBDENG2_REMOTEFEEDRELAY_H
