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

namespace de {

class DFileBuilder
{
public:
    static void init();
    static void shutdown();

    /**
     * Create a new handle on the AbstractFile @a file.
     *
     * @param af            VFS object representing the file being opened.
     */
    static DFile* fromFile(AbstractFile& file);

    /**
     * Create a new handle on a lump of AbstractFile @a file.
     *
     * @param af            VFS object representing the container of the lump to be opened.
     * @param lumpIdx       Logical index of the lump within @a file to be opened.
     * @param dontBuffer    @c true= do not buffer a copy of the lump.
     */
    static DFile* fromFileLump(AbstractFile& file, int lumpIdx, bool dontBuffer);

    /**
     * Open a new handle on the specified native file.
     *
     * @param nativeFile    Native file system handle to the file being opened.
     * @param baseOffset    Offset from the start of the file in bytes to begin.
     */
    static DFile* fromNativeFile(FILE& nativeFile, size_t baseOffset);

    /**
     * Create a duplicate of handle @a hndl. Note that the duplicate is in
     * fact a "reference" to the original, so all changes to the file which they
     * represent are implicitly shared.
     *
     * @param hndl          Handle to be duplicated.
     */
    static DFile* dup(DFile const& hndl);
};

} // namespace de

extern "C" {
#endif

/**
 * Non-public methods of DFile. Placed here temporarily.
 */

/// @return  File object represented by this handle.
AbstractFile* DFile_File(DFile* hndl);

/// @return  FileList object which owns this handle.
struct filelist_s* DFile_List(DFile* hndl);

DFile* DFile_SetList(DFile* hndl, struct filelist_s* list);

/// @return  File object represented by this handle.
AbstractFile* DFile_File_const(DFile const* hndl);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_DFILEBUILDER_H */
