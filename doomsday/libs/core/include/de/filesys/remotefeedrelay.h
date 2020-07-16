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

#include "de/remotefeed.h"
#include "de/record.h"
#include "de/dictionaryvalue.h"
#include "de/asynccallback.h"
#include "de/filesys/query.h"
#include "de/filesys/link.h"

namespace de { namespace filesys {

/**
 * Connects to one or more remote file repositories and provides metadata and file
 * contents over a network connection.
 */
class DE_PUBLIC RemoteFeedRelay
{
public:
    static RemoteFeedRelay &get();

    enum Status { Disconnected, Connected };

    DE_AUDIENCE(Status, void remoteRepositoryStatusChanged(const String &address, Status))

public:
    RemoteFeedRelay();

    /**
     * Defines a new type of remote repository link. The defined links are each
     * offered a remote repository address, and the first one to create a Link instance
     * based on the address will be used to communicate with the repository.
     *
     * @param linkConstructor  Constructor method.
     */
    void defineLink(const Link::Constructor &linkConstructor);

    void       addRepository    (const String &address, const String &localRootPath);
    void       removeRepository (const String &address);
    Link *     repository       (const String &address) const;
    StringList repositories     () const;

    bool isConnected(const String &address) const;

    /**
     * Queries all the connected repositories for a set of packages. The local paths
     * representing the remote packages are returned.
     *
     * @param packageIds  Packages to find.
     *
     * @return Hash of [packageId -> local path].
     */
    PackagePaths locatePackages(const StringList &packageIds) const;

    Request<FileMetadata> fetchFileList(const String &repository,
                                        String        folderPath,
                                        FileMetadata  metadataReceived);

    Request<FileContents> fetchFileContents(const String &repository,
                                            String        filePath,
                                            FileContents  contentsReceived);

private:
    DE_PRIVATE(d)
};

}} // namespace de::filesys

#endif // LIBCORE_REMOTEFEEDRELAY_H
