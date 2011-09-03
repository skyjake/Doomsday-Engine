/**\file wadfile.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_WADFILE_H
#define LIBDENG_FILESYS_WADFILE_H

#include <stdio.h>

#include "lumpinfo.h"
#include "abstractfile.h"

struct lumpdirectory_s;

/**
 * WadFile. Runtime representation of a WAD file.
 *
 * @ingroup FS
 */
typedef struct wadfile_s {
    // Base file.
    abstractfile_t _base;
    int _lumpCount;
    size_t _lumpRecordsOffset;
    lumpinfo_t* _lumpInfo;
    void** _lumpCache;
} wadfile_t;

wadfile_t* WadFile_New(DFILE* handle, const char* absolutePath);
void WadFile_Delete(wadfile_t* wad);

/// Close this file if open and release any acquired file identifiers.
void WadFile_Close(wadfile_t* wad);

int WadFile_PublishLumpsToDirectory(wadfile_t* file, struct lumpdirectory_s* directory);

const lumpinfo_t* WadFile_LumpInfo(wadfile_t* file, int lumpIdx);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param dest  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t WadFile_ReadLump2(wadfile_t* wad, int lumpIdx, uint8_t* dest, boolean tryCache);
size_t WadFile_ReadLump(wadfile_t* wad, int lumpIdx, uint8_t* dest);

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
size_t WadFile_ReadLumpSection2(wadfile_t* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache);
size_t WadFile_ReadLumpSection(wadfile_t* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length);

/**
 * Read the data associated with the specified lump index into the cache.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const uint8_t* WadFile_CacheLump(wadfile_t* wad, int lumpIdx, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param lumpIdx  Lump index associated with the cached data being changed.
 * @param tag  Zone purge level/cache tag to use.
 */
void WadFile_ChangeLumpCacheTag(wadfile_t* wad, int lumpIdx, int tag);

void WadFile_ClearLumpCache(wadfile_t* wad);

/**
 * An extremely simple formula. Does not conform to any CRC standard.
 * (So why's it called CRC, then?)
 */
uint WadFile_CalculateCRC(const wadfile_t* wad);

/**
 * Accessors:
 */

/// @return  Number of lumps contained within this file.
int WadFile_LumpCount(wadfile_t* wad);

/**
 * Static members:
 */

/**
 * Does the specified file appear to be in WAD format.
 * @return  @c true iff this is a file that can be represented using WadFile.
 */
boolean WadFile_Recognise(DFILE* handle);

#endif /* LIBDENG_FILESYS_WADFILE_H */
