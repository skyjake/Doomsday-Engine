/** @file archivefolder.h  Folder whose contents represent an archive.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ARCHIVEFOLDER_H
#define LIBCORE_ARCHIVEFOLDER_H

#include "de/folder.h"
#include "de/archive.h"

namespace de {

/**
 * Specialized Folder that gets its contents from an archive (via ArchiveFeed).
 * @ingroup fs
 *
 * When a generic ZIP-format file is interpreted, it will be represented in the file
 * system by an ArchiveFolder, while the contents of the ZIP file are represented by
 * ArchiveEntryFile instances.
 *
 * @see ArchiveFeed
 */
class DE_PUBLIC ArchiveFolder : public Folder
{
public:
    /**
     * Construct a package folder that feeds from a file. The archive is read
     * and a feed is attached for populating the folder with entries from the
     * archive.
     *
     * @param sourceArchiveFile  Source file that contains an archive. The
     *                           ArchiveFeed that will be attached to the
     *                           ArchiveFolder uses this file as its source.
     * @param name               Name for the folder.
     */
    ArchiveFolder(File &sourceArchiveFile, const String &name = String());

    virtual ~ArchiveFolder();

    void flush();

    String describe() const;

    /**
     * Returns the Archive of the package.
     */
    Archive &archive();

    /// @copydoc archive()
    const Archive &archive() const;
};

} // namespace de

#endif // LIBCORE_ARCHIVEFOLDER_H
