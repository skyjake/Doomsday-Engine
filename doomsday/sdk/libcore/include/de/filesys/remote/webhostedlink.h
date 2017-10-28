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

#ifndef DENG2_FILESYS_WEBHOSTEDLINK_H
#define DENG2_FILESYS_WEBHOSTEDLINK_H

#include "../Link"
#include "../../PathTree"

namespace de {
namespace filesys {

/**
 * Repository of files hosted on a web server as a file tree. Assumed to come
 * with a Unix-style "ls-laR.gz" directory tree index (e.g., an idgames mirror).
 */
class WebHostedLink : public Link
{
public:
    struct FileEntry : public PathTree::Node
    {
        duint64 size = 0;
        Time modTime;

        FileEntry(PathTree::NodeArgs const &args) : Node(args) {}
        FileEntry() = delete;

        Block metaId(Link const &link) const;
    };

    using FileTree = PathTreeT<FileEntry>;

public:
    WebHostedLink(String const &address, String const &indexPath);

    PackagePaths locatePackages(StringList const &packageIds) const override;

protected:
    virtual void setFileTree(FileTree *tree);

    FileTree const &fileTree() const;

    virtual void parseRepositoryIndex(QByteArray data) = 0;

    virtual String findPackagePath(String const &packageId) const = 0;

    void transmit(Query const &query) override;

private:
    DENG2_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // DENG2_FILESYS_WEBHOSTEDLINK_H
