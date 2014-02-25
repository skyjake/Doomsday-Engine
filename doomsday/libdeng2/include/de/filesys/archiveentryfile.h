/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ARCHIVEENTRYFILE_H
#define LIBDENG2_ARCHIVEENTRYFILE_H

#include "../ByteArrayFile"

namespace de {

class Archive;

/**
 * Accesses data of an entry within an archive.
 *
 * @ingroup fs
 */
class ArchiveEntryFile : public ByteArrayFile
{
public:
    /**
     * Constructs an archive file.
     *
     * @param name       Name of the file.
     * @param archive    Archive where the contents of the file are located.
     * @param entryPath  Path of the file's entry within the archive.
     */
    ArchiveEntryFile(String const &name, Archive &archive, String const &entryPath);

    ~ArchiveEntryFile();

    String describe() const;
    String entryPath() const;

    void clear();

    /// Returns the archive of the file.
    Archive &archive() { return _archive; }

    /// Returns the archive of the file (non-modifiable).
    Archive const &archive() const { return _archive; }

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

private:
    Archive &_archive;

    /// Path of the entry within the archive.
    String _entryPath;
};

} // namespace de

#endif /* LIBDENG2_ARCHIVEENTRYFILE_H */
