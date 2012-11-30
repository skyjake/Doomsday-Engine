/** @file packagefolder.h Folder that hosts a data package archive.
 * @ingroup fs
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_PACKAGEFOLDER_H
#define LIBDENG2_PACKAGEFOLDER_H

#include "../Folder"
#include "../Archive"

namespace de {

/**
 * Specialized Folder that hosts a data package archive. When a ZIP-format file
 * is interpreted, it will be represented in the file system by a
 * PackageFolder, while the contents of the ZIP file are represented by
 * ArchiveEntryFile instances.
 *
 * @see ArchiveFeed
 */
class PackageFolder : public Folder
{
public:
    /**
     * Construct a package folder that feeds from a file. The archive is read
     * and a feed is attached for populating the folder with entries from the
     * archive.
     *
     * @param sourceArchiveFile  Source file that contains an archive.
     * @param name               Name for the folder.
     */
    PackageFolder(File &sourceArchiveFile, String const &name = "");

    virtual ~PackageFolder();

    /**
     * Returns the Archive of the package.
     */
    Archive &archive();

    /// @copydoc archive()
    Archive const &archive() const;
};

} // namespace de

#endif // LIBDENG2_PACKAGEFOLDER_H
