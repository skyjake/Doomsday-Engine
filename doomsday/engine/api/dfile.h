/**
 * @file dfile.h
 * Unique file reference in the engine's virtual file system.
 *
 * @ingroup fs
 *
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_FILEHANDLE_H
#define LIBDENG_FILESYS_FILEHANDLE_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup fs
///@{

struct dfile_s; // The file handle instance (opaque).

/**
 * DFile instance.
 */
typedef struct dfile_s DFile;

/**
 * Close the file if open. Note that this clears any previously buffered data.
 */
void DFile_Close(DFile* file);

/// @return  @c true iff this handle's internal state is valid.
boolean DFile_IsValid(const DFile* file);

/// @return  The length of the file, in bytes.
size_t DFile_Length(DFile* file);

/// @return  Offset in bytes from the start of the file to begin read.
size_t DFile_BaseOffset(const DFile* file);

/**
 * @return  Number of bytes read (at most @a count bytes will be read).
 */
size_t DFile_Read(DFile* file, uint8_t* buffer, size_t count);

/**
 * Read a character from the stream, advancing the read position in the process.
 */
unsigned char DFile_GetC(DFile* file);

/**
 * @return  @c true iff the stream has reached the end of the file.
 */
boolean DFile_AtEnd(DFile* file);

/**
 * @return  Current position in the stream as an offset from the beginning of the file.
 */
size_t DFile_Tell(DFile* file);

/**
 * @return  The current position in the file, before the move, as an
 *      offset from the beginning of the file.
 */
size_t DFile_Seek(DFile* file, size_t offset, int whence);

/**
 * Rewind the stream to the start of the file.
 */
void DFile_Rewind(DFile* file);

#if _DEBUG
void DFile_Print(const DFile* file);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

///@}

#endif /* LIBDENG_FILESYS_FILEHANDLE_H */
