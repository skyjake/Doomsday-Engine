/**
 * @file dfilebuilder.h
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_FILESYS_DFILEBUILDER_H
#define LIBDENG_FILESYS_DFILEBUILDER_H

#include "fs_main.h"

#ifdef __cplusplus

class DFileBuilder
{
public:
    static void init();
    static void shutdown();

    /**
     * Create a new handle on the AbstractFile @a af.
     *
     * @param af            VFS object representing the file being opened.
     */
    static DFile* fromAbstractFile(AbstractFile& af);

    /**
     * Create a new handle on a lump of AbstractFile @a af.
     *
     * @param af            VFS object representing the container of the lump to be opened.
     * @param lumpIdx       Logical index of the lump within @a af to be opened.
     * @param dontBuffer    @c true= do not buffer a copy of the lump.
     */
    static DFile* fromAbstractFileLump(AbstractFile& af, int lumpIdx, bool dontBuffer);

    /**
     * Open a new handle on the specified system file.
     *
     * @param file          File system handle to the file being opened.
     * @param baseOffset    Offset from the start of the file in bytes to begin.
     */
    static DFile* fromFile(FILE& file, size_t baseOffset);

    /**
     * Create a duplicate of handle @a hndl. Note that the duplicate is in
     * fact a "reference" to the original, so all changes to the file which they
     * represent are implicitly shared.
     *
     * @param hndl          DFile handle to be duplicated.
     */
    static DFile* dup(DFile const& hndl);
};

extern "C" {
#endif

/**
 * Non-public methods of DFile. Placed here temporarily.
 */

DFile* DFile_New(void);

void DFile_Delete(DFile* file, boolean recycle);

/// @return  File object represented by this handle.
AbstractFile* DFile_File(DFile* file);

/// @return  FileList object which owns this handle.
FileList* DFile_List(DFile* file);

DFile* DFile_SetList(DFile* file, FileList* list);

/// @return  File object represented by this handle.
AbstractFile* DFile_File_const(DFile const* file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_DFILEBUILDER_H */
