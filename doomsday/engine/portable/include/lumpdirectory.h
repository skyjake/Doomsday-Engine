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

#include "abstractfile.h"
#include "lumpinfo.h"

struct lumpdirectory_lumprecord_s;

/**
 * LumpDirectory.
 *
 * @ingroup fs
 */
typedef struct lumpdirectory_s {
    int _flags; /// @see lumpDirectoryFlags
    int _numRecords;
    struct lumpdirectory_lumprecord_s* _records;
} lumpdirectory_t;

lumpdirectory_t* LumpDirectory_New(void);
void LumpDirectory_Delete(lumpdirectory_t* ld);

/// Number of lumps in the directory.
int LumpDirectory_NumLumps(lumpdirectory_t* ld);

/// Clear the directory back to its default (i.e., empty state).
void LumpDirectory_Clear(lumpdirectory_t* ld);

/**
 * Iterate over LumpInfo records in the directory making a callback for each.
 * Iteration ends when all LumpInfos have been processed or a callback returns non-zero.
 *
 * @param callback  Callback to make for each iteration.
 * @param paramaters  User data to be passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LumpDirectory_Iterate2(lumpdirectory_t* ld, int (*callback) (const lumpinfo_t*, void*), void* paramaters);
int LumpDirectory_Iterate(lumpdirectory_t* ld, int (*callback) (const lumpinfo_t*, void*));

/// @return  Index associated with the last lump with @a name if found else @c -1
lumpnum_t LumpDirectory_IndexForName(lumpdirectory_t* ld, const char* name);

/// @return  Index associated with the last lump with variable-length @a path if found else @c -1
lumpnum_t LumpDirectory_IndexForPath(lumpdirectory_t* ld, const char* path);

/// @return  @c true iff @a lumpNum can be interpreted as a valid lump index.
boolean LumpDirectory_IsValidIndex(lumpdirectory_t* ld, lumpnum_t lumpNum);

/**
 * Append a new set of lumps to the directory.
 *
 * \post Lump name hashes may be invalidated (will be rebuilt upon next search).
 *
 * @param lumpInfo  Metadata for each lump. Must be at least @a lumpInfoCount elements.
 * @param numLumpInfo  Number of elements pointed to by @a lumpInfo.
 * @param fsObject  File system record object for the file from which lumps are being added.
 */
void LumpDirectory_Append(lumpdirectory_t* ld, const lumpinfo_t* lumpInfo,
    int lumpInfoCount, abstractfile_t* fsObject);

/**
 * Prune all lumps in the directory originating from @a fsObject.
 * @return  Number of pruned lumps.
 */
int LumpDirectory_PruneByFile(lumpdirectory_t* ld, abstractfile_t* fsObject);

/**
 * @param matchLumpName  @c true = Use lump name when performing comparisions,
 *      else use lump path (with Zips).
 */
void LumpDirectory_PruneDuplicateRecords(lumpdirectory_t* ld, boolean matchLumpName);

/// @return  Info associated to the lump with index @a lumpNum.
const lumpinfo_t* LumpDirectory_LumpInfo(lumpdirectory_t* ld, lumpnum_t lumpNum);

/// @return  File system object associated to the lump with index @a lumpNum.
abstractfile_t* LumpDirectory_SourceFile(lumpdirectory_t* ld, lumpnum_t lumpNum);

/**
 * Print a content listing of lumps in this directory to stdout (for debug).
 */
void LumpDirectory_Print(lumpdirectory_t* ld);

/**
 * Accessors (for convenience):
 */
const char* LumpDirectory_LumpName(lumpdirectory_t* ld, lumpnum_t lumpNum);
const char* LumpDirectory_LumpPath(lumpdirectory_t* ld, lumpnum_t lumpNum);
size_t LumpDirectory_LumpSize(lumpdirectory_t* ld, lumpnum_t lumpNum);
size_t LumpDirectory_LumpBaseOffset(lumpdirectory_t* ld, lumpnum_t lumpNum);

#endif /* LIBDENG_FILESYS_LUMPDIRECTORY_H */
