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

#ifndef DE_FILESYS_LINK_H
#define DE_FILESYS_LINK_H

#include "../dictionaryvalue.h"
#include "../identifiedpacket.h"
#include "../string.h"
#include "query.h"

namespace de {

class AsyncScope;
class File;
class Folder;

namespace filesys {

/**
 * Base class for an active connection to a remote repository. Specialized subclasses
 * handle specific types of repositories. One link instance is shared by all RemoteFeed
 * instances accessing the same repository.
 *
 * @ingroup fs
 */
class DE_PUBLIC Link
{
public:
    enum State { Deinitialized, Initializing, Ready };

    using Constructor = std::function<Link * (const String &address)>;

public:
    virtual ~Link();

    virtual void setLocalRoot(const String &rootPath);

    Folder &localRoot() const;
    String  address() const;
    State   state() const;

    /**
     * Uses locally available indexes to determine which remote paths for a set of
     * packages.
     *
     * @param packageIds  Packages to locate. The identifiers may include versions.
     *
     * @return Paths for located packages. May contain fewer entries than was provided
     * via @a packageIds -- empty if nothing was found.
     */
    virtual PackagePaths locatePackages(const StringList &packageIds) const = 0;

    /**
     * Returns a list of the categories in the repository. These can be used as tags
     * for filtering.
     *
     * @return Category tags.
     */
    virtual StringList categoryTags() const;

    /**
     * Iterates the full list of all packages available in the repository. Note this
     * may be large set of packages.
     */
    virtual LoopResult forPackageIds(std::function<LoopResult (const String &packageId)> func) const = 0;

    QueryId sendQuery(Query query);

    virtual File *populateRemotePath(const String &packageId, const RepositoryPath &path) const;

protected:
    Link(const String &address);

    AsyncScope &scope();

    Query *findQuery(QueryId id);

    void cancelAllQueries();
    void cleanupQueries();

    void metadataReceived(QueryId id, const DictionaryValue &metadata);
    void chunkReceived(QueryId id, duint64 startOffset, const Block &chunk, duint64 fileSize);

    virtual void wasConnected();
    virtual void wasDisconnected();
    virtual void handleError(const String &errorMessage);
    virtual void transmit(const Query &query) = 0;

private:
    DE_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // DE_FILESYS_LINK_H
