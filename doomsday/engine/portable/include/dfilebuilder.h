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

#include "abstractfile.h"
#include "dfile.h"
#include "filelist.h"

#ifdef __cplusplus
extern "C" {
#endif

void DFileBuilder_Init(void);
void DFileBuilder_Shutdown(void);

/**
 * Open a new read-only stream on the specified file.
 *
 * @param af  File system object representing the file being opened.
 */
DFile* DFileBuilder_NewFromAbstractFile(AbstractFile* af);

/**
 * Open a new read-only stream on the specified lump-file.
 *
 * @param af  File system object representing the container of the lump to be opened.
 * @param lumpIdx  Logical index of the lump within @a container to be opened.
 * @param dontBuffer  @c true= do not buffer a copy of the lump.
 */
DFile* DFileBuilder_NewFromAbstractFileLump(AbstractFile* af, int lumpIdx, boolean dontBuffer);

/**
 * Open a new read-only stream on the specified system file.
 * @param file  File system handle to the file being opened.
 * @param baseOffset  Offset from the start of the file in bytes to begin.
 */
DFile* DFileBuilder_NewFromFile(FILE* hndl, size_t baseOffset);

DFile* DFileBuilder_NewCopy(const DFile* file);

/**
 * Non-public methods of DFile. Placed here temporarily.
 */

DFile* DFile_New(void);

void DFile_Delete(DFile* file, boolean recycle);

/// @return  File object represented by this handle.
struct abstractfile_s* DFile_File(DFile* file);

/// @return  FileList object which owns this handle.
FileList* DFile_List(DFile* file);

void DFile_SetList(DFile* file, FileList* list);

/// @return  File object represented by this handle.
struct abstractfile_s* DFile_File_Const(const DFile* file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_DFILEBUILDER_H */
