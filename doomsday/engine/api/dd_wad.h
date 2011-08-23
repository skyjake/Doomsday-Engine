/**\file dd_wad.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Implements DOOM's WAD interface as a wrapper to the virtual file system.
 */

#ifndef LIBDENG_API_WAD_H
#define LIBDENG_API_WAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_types.h"

/// @return  Buffer size needed to load the data associated with @a lumpNum in bytes.
size_t W_LumpLength(lumpnum_t lumpNum);

/// @return  Name of the lump associated with @a lumpNum.
const char* W_LumpName(lumpnum_t lumpNum);

/// @return  "Last modified" timestamp of the zip entry.
uint W_LumpLastModified(lumpnum_t lumpNum);

/// @return  Name of the WAD file where the data associated with @a lumpNum resides.
///     Always returns a valid filename (or an empty string).
const char* W_LumpSourceFile(lumpnum_t lumpNum);

/// @return  @c true iff the data associated with @a lumpNum resides in an IWAD.
boolean W_LumpIsFromIWAD(lumpnum_t lumpNum);

/**
 * @param name  Name of the lump to search for.
 * @param silent  Do not print results to the console.
 * @return  Unique index of the found lump in the primary lump directory else @c -1 if not found.
 */
lumpnum_t W_CheckLumpNumForName2(const char* name, boolean silent);
lumpnum_t W_CheckLumpNumForName(const char* name);

/// \note As per W_CheckLumpNumForName but results in a fatal error if not found.
lumpnum_t W_GetLumpNumForName(const char* name);

/**
 * Read the data associated with @a lumpNum into @a buffer.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 * @param dest  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @return  Number of bytes read.
 */
size_t W_ReadLump(lumpnum_t lumpNum, uint8_t* buffer);

/**
 * Read a subsection of the data associated with @a lumpNum into @a buffer.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param startOffset  Offset from the beginning of the lump to start reading.
 * @param length  Number of bytes to be read.
 * @return  Number of bytes read.
 */
size_t W_ReadLumpSection(lumpnum_t lumpNum, uint8_t* buffer, size_t startOffset, size_t length);

/**
 * Read the data associated with @a lumpNum into the cache.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const uint8_t* W_CacheLump(lumpnum_t lumpNum, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param lumpNum  Logical lump index associated with the data.
 * @param tag  Zone purge level/cache tag to use.
 */
void W_CacheChangeTag(lumpnum_t lumpNum, int tag);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_WAD_H */
