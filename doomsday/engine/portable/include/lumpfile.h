/**\file lumpfile_h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_LUMPFILE_H
#define LIBDENG_FILESYS_LUMPFILE_H

#include "lumpinfo.h"
#include "abstractfile.h"

struct lumpdirectory_s;

/**
 * LumpFile. Runtime representation of a lump-file for use with LumpDirectory
 *
 * @ingroup FS
 */
typedef struct lumpfile_s {
    // Base file.
    abstractfile_t _base;
    void** _cacheData;
} lumpfile_t;

lumpfile_t* LumpFile_New(size_t baseOffset, const lumpinfo_t* info, streamfile_t* sf);
void LumpFile_Delete(lumpfile_t* lump);

/// Close this file if open and release any acquired file identifiers.
void LumpFile_Close(lumpfile_t* lump);

int LumpFile_PublishLumpsToDirectory(lumpfile_t* file, struct lumpdirectory_s* directory);

const lumpinfo_t* LumpFile_LumpInfo(lumpfile_t* file, int lumpIdx);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t LumpFile_ReadLump2(lumpfile_t* lump, int lumpIdx, uint8_t* buffer, boolean tryCache);
size_t LumpFile_ReadLump(lumpfile_t* lump, int lumpIdx, uint8_t* buffer);

/**
 * Read a subsection of the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param startOffset  Offset from the beginning of the lump to start reading.
 * @param length  Number of bytes to be read.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t LumpFile_ReadLumpSection2(lumpfile_t* lump, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache);
size_t LumpFile_ReadLumpSection(lumpfile_t* lump, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length);

/**
 * Read the data associated with the specified lump index into the cache.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const uint8_t* LumpFile_CacheLump(lumpfile_t* lump, int lumpIdx, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param lumpIdx  Lump index associated with the cached data being changed.
 * @param tag  Zone purge level/cache tag to use.
 */
void LumpFile_ChangeLumpCacheTag(lumpfile_t* lump, int lumpIdx, int tag);

void LumpFile_ClearLumpCache(lumpfile_t* lump);

/**
 * Accessors:
 */

/// @return  Number of lumps contained within this file.
int LumpFile_LumpCount(lumpfile_t* lump);

#endif /* LIBDENG_FILESYS_LUMPFILE_H */
