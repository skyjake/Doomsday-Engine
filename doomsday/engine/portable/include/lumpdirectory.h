/**\file lumpdirectory.h
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

#ifndef LIBDENG_FILESYS_LUMPDIRECTORY_H
#define LIBDENG_FILESYS_LUMPDIRECTORY_H

struct lumpdirectory_lumprecord_s;

/**
 * LumpDirectory.
 *
 * @ingroup fs
 */
typedef struct lumpdirectory_s {
    boolean _hashDirty;
    size_t _numRecords;
    struct lumpdirectory_lumprecord_s* _records;
} lumpdirectory_t;

lumpdirectory_t* LumpDirectory_Construct(void);
void LumpDirectory_Destruct(lumpdirectory_t* ld);

/// Number of lumps in the directory.
size_t LumpDirectory_NumLumps(lumpdirectory_t* ld);

/// Clear the directory back to its default (i.e., empty state).
void LumpDirectory_Clear(lumpdirectory_t* ld);

/// @return  Index associated with lump named @a name if found else @c -1
lumpnum_t LumpDirectory_IndexForName(lumpdirectory_t* ld, const char* name);

/// @return  @c true iff @a lumpNum can be interpreted as a valid lump index.
boolean LumpDirectory_IsValidIndex(lumpdirectory_t* ld, lumpnum_t lumpNum);

/**
 * Append a new set of lumps to the directory.
 *
 * \post Lump hash may be invalidated (will be rebuilt upon next search).
 *
 * @param lumpInfo  Lump metadata for each lump. Must be at least @a numLumpInfo elements.
 * @param numLumpInfo  Number of elements pointed to by @a lumpInfo.
 * @param fileRecord  Record associated with the file in which the lumps are to be found.
 */
void LumpDirectory_Append(lumpdirectory_t* ld, const wadfile_lumpinfo_t* lumpInfo,
    size_t numLumpInfo, wadfile_t* fileRecord);

/**
 * Prune all lumps in the directory originating from @a fileRecord.
 * @return  Number of pruned lumps.
 */
size_t LumpDirectory_PruneByFile(lumpdirectory_t* ld, wadfile_t* fileRecord);

/// @return  Info associated to the lump with index @a lumpNum.
const wadfile_lumpinfo_t* LumpDirectory_LumpInfo(lumpdirectory_t* ld, lumpnum_t lumpNum);

/// @return  File record associated to the lump with index @a lumpNum.
wadfile_t* LumpDirectory_LumpSourceFile(lumpdirectory_t* ld, lumpnum_t lumpNum);

/**
 * Print the contents of the directory to stdout.
 */
void LumpDirectory_Print(lumpdirectory_t* ld);

/**
 * Accessors (for convenience):
 */
const char* LumpDirectory_LumpName(lumpdirectory_t* ld, lumpnum_t lumpNum);
size_t LumpDirectory_LumpSize(lumpdirectory_t* ld, lumpnum_t lumpNum);
size_t LumpDirectory_LumpBaseOffset(lumpdirectory_t* ld, lumpnum_t lumpNum);

#endif /* LIBDENG_FILESYS_LUMPDIRECTORY_H */
