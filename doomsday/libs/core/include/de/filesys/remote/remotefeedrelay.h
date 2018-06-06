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

#ifndef LIBCORE_REMOTEFEEDRELAY_H
#define LIBCORE_REMOTEFEEDRELAY_H

#include "../../RemoteFeed"
#include "../../Record"
#include "../../DictionaryValue"
#include "../../AsyncCallback"
#include "../Query"
#include "../Link"

namespace de {
namespace filesys {

/**
 * Connects to one or more remote file repositories and provides metadata and file
 * contents over a network connection.
 */
class DE_PUBLIC RemoteFeedRelay
{
public:
    static RemoteFeedRelay &get();

    enum Status { Disconnected, Connected };

    DE_DEFINE_AUDIENCE2(Status, void remoteRepositoryStatusChanged(String const &address, Status))

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

    void addRepository(String const &address, String const &localRootPath);

    void removeRepository(String const &address);

    Link *repository(String const &address) const;

    StringList repositories() const;

    bool isConnected(String const &address) const;

    /**
     * Queries all the connected repositories for a set of packages. The local paths
     * representing the remote packages are returned.
     *
     * @param packageIds  Packages to find.
     *
     * @return Hash of [packageId -> local path].
     */
    PackagePaths locatePackages(StringList const &packageIds) const;

    Request<FileMetadata> fetchFileList(String const &repository,
                                        String folderPath,
                                        FileMetadata metadataReceived);

    Request<FileContents> fetchFileContents(String const &repository,
                                            String filePath,
                                            FileContents contentsReceived);

//    QNetworkAccessManager &network();

private:
    DE_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // LIBCORE_REMOTEFEEDRELAY_H
