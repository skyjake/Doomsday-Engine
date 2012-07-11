/**\file wadfile.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "lumpinfo.h"
#include "abstractfile.h"

struct lumpdirectory_s;
struct pathdirectorynode_s;

/**
 * WadFile. Runtime representation of a WAD file.
 *
 * @ingroup FS
 */
struct wadfile_s; // The wadfile instance (opaque)
typedef struct wadfile_s WadFile;

WadFile* WadFile_New(DFile* file, const char* path, const LumpInfo* info);
void WadFile_Delete(WadFile* wad);

int WadFile_PublishLumpsToDirectory(WadFile* wad, struct lumpdirectory_s* directory);

struct pathdirectorynode_s* WadFile_LumpDirectoryNode(WadFile* wad, int lumpIdx);

ddstring_t* WadFile_ComposeLumpPath(WadFile* wad, int lumpIdx, char delimiter);

const LumpInfo* WadFile_LumpInfo(WadFile* wad, int lumpIdx);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t WadFile_ReadLump2(WadFile* wad, int lumpIdx, uint8_t* buffer, boolean tryCache);
size_t WadFile_ReadLump(WadFile* wad, int lumpIdx, uint8_t* buffer);

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
size_t WadFile_ReadLumpSection2(WadFile* wad, int lumpIdx,
    uint8_t* buffer, size_t startOffset, size_t length, boolean tryCache);
size_t WadFile_ReadLumpSection(WadFile* wad, int lumpIdx,
    uint8_t* buffer, size_t startOffset, size_t length);

/**
 * Read the data associated with the specified lump index into the cache.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const uint8_t* WadFile_CacheLump(WadFile* wad, int lumpIdx, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param lumpIdx  Lump index associated with the cached data being changed.
 * @param tag  Zone purge level/cache tag to use.
 */
void WadFile_ChangeLumpCacheTag(WadFile* wad, int lumpIdx, int tag);

void WadFile_ClearLumpCache(WadFile* wad);

/**
 * An extremely simple formula. Does not conform to any CRC standard.
 * (So why's it called CRC, then?)
 */
uint WadFile_CalculateCRC(WadFile* wad);

/**
 * Accessors:
 */

/// @return  Number of lumps contained within this file.
int WadFile_LumpCount(WadFile* wad);

/**
 * Static members:
 */

/**
 * Does the specified file appear to be in WAD format.
 * @param file  Stream file handle/wrapper to the file being interpreted.
 * @return  @c true iff this is a file that can be represented using WadFile.
 */
boolean WadFile_Recognise(DFile* file);

#endif /* LIBDENG_FILESYS_WADFILE_H */
