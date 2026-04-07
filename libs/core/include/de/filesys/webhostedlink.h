/** @file remote/webhostedlink.h
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

#ifndef DE_FILESYS_WEBHOSTEDLINK_H
#define DE_FILESYS_WEBHOSTEDLINK_H

#include "link.h"
#include "../pathtree.h"

namespace de { namespace filesys {

/**
 * Repository of files hosted on a web server as a file tree. Assumed to come
 * with a Unix-style "ls-laR.gz" directory tree index (e.g., an idgames mirror).
 */
class DE_PUBLIC WebHostedLink : public Link
{
public:
    struct DE_PUBLIC FileEntry : public PathTree::Node {
        duint64 size = 0;
        Time    modTime;

        FileEntry(const PathTree::NodeArgs &args) : Node(args) {}
        FileEntry() = delete;
        Block metaId(const Link &link) const;
    };

    using FileTree = PathTreeT<FileEntry>;

public:
    WebHostedLink(const String &address, const String &indexPath);

    PackagePaths locatePackages(const StringList &packageIds) const override;

protected:
    virtual void setFileTree(FileTree *tree);

    const FileTree &fileTree() const;

    const FileEntry *findFile(const Path &path) const;

    virtual void parseRepositoryIndex(const Block &data) = 0;

    virtual String findPackagePath(const String &packageId) const = 0;

    void transmit(const Query &query) override;

private:
    DE_PRIVATE(d)
};

}} // namespace de::filesys

#endif // DE_FILESYS_WEBHOSTEDLINK_H
