/** @file linkfile.h  Symbolic link.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_LINKFILE_H
#define LIBCORE_LINKFILE_H

#include "de/file.h"

namespace de {

/**
 * Symbolic link that points to another file in the file system.
 *
 * Overrides the filesys::Node path navigation and child finding so that these operations
 * are carried out using the folder that the link points to.
 *
 * @todo Currently only supports links to folders, not to individual files. Linking to
 * files would require refactoring filesys::Node and File so that more functionality is
 * virtualized and therefore can be redirected by LinkFile. -jk
 *
 * @ingroup fs
 */
class DE_PUBLIC LinkFile : public File
{
public:
    LinkFile(const String &name = String());

    ~LinkFile();

    /**
     * Sets the link target of this file.
     *
     * @param file  Target file. This file will observe the target file for deletion.
     */
    void setTarget(const File &file);

    void setTarget(const File *fileOrNull);

    /**
     * Returns the file's target. This is used for indirection when descending into
     * subfolders, to implement symbolic links.
     *
     * The target is used when locating a file from a folder using a path. The
     * file system index does not contain any of the apparent contents under, say,
     * a linked folder.
     *
     * @see LinkFile
     */
    const File &target() const override;

    /// @copydoc target() const
    File &target() override;

    const Folder *targetFolder() const;

    Folder *targetFolder();

    /**
     * Determines if the link is broken, i.e., does not point to any other file.
     */
    bool isBroken() const;

    String describe() const override;

    // Stream access:
    const IIStream &operator >> (IByteArray &bytes) const override;

    // filesys::Node overrides:
    const Node *tryFollowPath(const PathRef &path) const override;
    const Node *tryGetChild(const String &name) const override;

public:
    /**
     * Creates a new link to an existing file.
     *
     * @param file      Target file.
     * @param linkName  Name of the created link file. If empty, the target file's
     *                  name is used as the link's name.
     *
     * @return New LinkFile instance. Caller gets ownership.
     */
    static LinkFile *newLinkToFile(const File &file, String linkName = "");

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_LINKFILE_H
