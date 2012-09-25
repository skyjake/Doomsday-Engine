/**
 * @file lumpdirectory.h
 * Indexed directory of lumps. @ingroup fs
 *
 * Virtual file system component used to model an indexable collection of
 * lumps. A single directory may include lumps originating from many different
 * file containers.
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_LUMPDIRECTORY_H
#define LIBDENG_FILESYS_LUMPDIRECTORY_H

#include "abstractfile.h"
#include "lumpinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

struct lumpdirectory_s;

/**
 * LumpDirectory instance.
 */
typedef struct lumpdirectory_s LumpDirectory;

/**
 * @defgroup lumpDirectoryFlags Lump Directory Flags
 */
///{
#define LDF_UNIQUE_PATHS                0x1 ///< Lumps in the directory must have unique paths.
                                            /// Inserting a lump with the same path as one which
                                            /// already exists will result in the earlier lump
                                            /// being pruned.
///}

/**
 * Construct a new (empty) instance of LumpDirectory.
 *
 * @param flags  @ref lumpDirectoryFlags
 */
LumpDirectory* LumpDirectory_NewWithFlags(int flags);
LumpDirectory* LumpDirectory_New(void); /*flags=0*/

void LumpDirectory_Delete(LumpDirectory* dir);

/// Number of lumps in the directory.
int LumpDirectory_Size(LumpDirectory* dir);

/// Clear the directory back to its default (i.e., empty state).
void LumpDirectory_Clear(LumpDirectory* dir);

/**
 * Are any lumps from @a file published in this directory?
 *
 * @param dir  LumpDirectory instance.
 * @param file  File containing the lumps to look for.
 * @return  @c true if one or more lumps are included.
 */
boolean LumpDirectory_Catalogues(LumpDirectory* dir, abstractfile_t* file);

/// @return  Index associated with the last lump with variable-length @a path if found else @c -1
lumpnum_t LumpDirectory_IndexForPath(LumpDirectory* dir, const char* path);

/// @return  @c true iff @a lumpNum can be interpreted as a valid lump index.
boolean LumpDirectory_IsValidIndex(LumpDirectory* dir, lumpnum_t lumpNum);

/// @return  LumpInfo for the lump with index @a lumpNum.
const LumpInfo* LumpDirectory_LumpInfo(LumpDirectory* dir, lumpnum_t lumpNum);

/**
 * Append a new set of lumps to the directory.
 *
 * \post Lump name hashes may be invalidated (will be rebuilt upon next search).
 *
 * @param dir  LumpDirectory instance.
 * @param file  File from which lumps are to be being added.
 * @param lumpIdxBase  Base index for the range of lumps being added.
 * @param lumpIdxCount  Number of lumps in the range being added.
 */
void LumpDirectory_CatalogLumps(LumpDirectory* dir, abstractfile_t* file,
    int lumpIdxBase, int lumpIdxCount);

/**
 * Prune all lumps catalogued from @a file.
 *
 * @param dir  LumpDirectory instance.
 * @param file  File containing the lumps to prune
 * @return  Number of lumps pruned.
 */
int LumpDirectory_PruneByFile(LumpDirectory* dir, abstractfile_t* file);

/**
 * Prune the lump referenced by @a lumpInfo.
 *
 * @param dir  LumpDirectory instance.
 * @param lumpInfo  Unique info descriptor for the lump to prune.
 * @return  @c true if found and pruned.
 */
boolean LumpDirectory_PruneLump(LumpDirectory* dir, LumpInfo* lumpInfo);

/**
 * Iterate over lumps in the directory making a callback for each.
 * Iteration ends when all LumpInfos have been processed or a callback returns non-zero.
 *
 * @param dir  LumpDirectory instance.
 * @param file  If not @c NULL only consider lumps from this file.
 * @param callback  Callback to make for each iteration.
 * @param paramaters  User data to be passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LumpDirectory_Iterate2(LumpDirectory* dir, abstractfile_t* file,
    int (*callback) (const LumpInfo*, void*), void* paramaters);
int LumpDirectory_Iterate(LumpDirectory* dir, abstractfile_t* file,
    int (*callback) (const LumpInfo*, void*));

/**
 * Print a content listing of lumps in this directory to stdout (for debug).
 */
void LumpDirectory_Print(LumpDirectory* dir);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_FILESYS_LUMPDIRECTORY_H
