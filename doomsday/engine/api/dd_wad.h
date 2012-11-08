/**
 * @file dd_wad.h
 * Wrapper API for accessing data stored in DOOM WAD files.
 *
 * All data is read through Doomsday's virtual file system (see @ref fs).
 *
 * @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#ifndef LIBDENG_API_WAD_H
#define LIBDENG_API_WAD_H

#include "dd_types.h"

/// @addtogroup resource
///@{

#ifdef __cplusplus
extern "C" {
#endif

/// @return  Buffer size needed to load the data associated with @a lumpNum in bytes.
size_t W_LumpLength(lumpnum_t lumpNum);

/// @return  Name of the lump associated with @a lumpNum.
AutoStr* W_LumpName(lumpnum_t lumpNum);

/// @return  "Last modified" timestamp of the zip entry.
uint W_LumpLastModified(lumpnum_t lumpNum);

/// @return  Name of the WAD file where the data associated with @a lumpNum resides.
///     Always returns a valid filename (or an empty string).
AutoStr* W_LumpSourceFile(lumpnum_t lumpNum);

/// @return  @c true iff the data associated with @a lumpNum does not originate from the current game.
boolean W_LumpIsCustom(lumpnum_t lumpNum);

/**
 * @param name      Name of the lump to search for.
 * @param silent    Do not print results to the console.
 *
 * @return  Unique index of the found lump in the primary lump directory else @c -1 if not found.
 */
lumpnum_t W_CheckLumpNumForName2(char const* name, boolean silent);
lumpnum_t W_CheckLumpNumForName(char const* name);

/// @note As per W_CheckLumpNumForName but results in a fatal error if not found.
lumpnum_t W_GetLumpNumForName(char const* name);

/**
 * Read the data associated with @a lumpNum into @a buffer.
 *
 * @param lumpNum   Logical lump index associated with the data being read.
 * @param buffer    Buffer to read into. Must be at least W_LumpLength() bytes.
 *
 * @return  Number of bytes read.
 */
size_t W_ReadLump(lumpnum_t lumpNum, uint8_t* buffer);

/**
 * Read a subsection of the data associated with @a lumpNum into @a buffer.
 *
 * @param lumpNum       Logical lump index associated with the data being read.
 * @param buffer        Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param startOffset   Offset from the beginning of the lump to start reading.
 * @param length        Number of bytes to be read.
 *
 * @return  Number of bytes read.
 */
size_t W_ReadLumpSection(lumpnum_t lumpNum, uint8_t* buffer, size_t startOffset, size_t length);

/**
 * Read the data associated with @a lumpNum into the cache.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 *
 * @return  Ptr to the cached copy of the associated data.
 */
uint8_t const* W_CacheLump(lumpnum_t lumpNum);

/**
 * Remove a lock on a cached data lump associated with @a lumpNum.
 *
 * @param lumpNum  Logical lump index associated with the data.
 */
void W_UnlockLump(lumpnum_t lumpNum);

#ifdef __cplusplus
} // extern "C"
#endif

///@}

#endif /* LIBDENG_API_WAD_H */
