/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_ARCHIVEFILE_H
#define LIBDENG2_ARCHIVEFILE_H

#include "../File"
#include "../String"

namespace de
{
    class Archive;
    
    /**
     * Accesses data of a file within an archive.
     *
     * @ingroup fs
     */
    class ArchiveFile : public File
    {
    public:
        /**
         * Constructs an archive file.
         *
         * @param name       Name of the file.
         * @param archive    Archive where the contents of the file are located.
         * @param entryPath  Path of the file's entry within the archive.
         */
        ArchiveFile(const String& name, Archive& archive, const String& entryPath);
        
        ~ArchiveFile();

        void clear();

        /// Returns the archive of the file.
        Archive& archive() { return _archive; }

        /// Returns the archive of the file (non-modifiable).
        const Archive& archive() const { return _archive; }

        // Implements IByteArray.
        void get(Offset at, Byte* values, Size count) const;
        void set(Offset at, const Byte* values, Size count);
        
    private:
        Archive& _archive;
        
        /// Path of the entry within the archive.
        String _entryPath;
    };
}

#endif /* LIBDENG2_ARCHIVEFILE_H */
