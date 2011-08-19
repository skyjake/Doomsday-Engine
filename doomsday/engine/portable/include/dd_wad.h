/**\file dd_wad.h
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

/**
 * WAD Files and Data Lump Cache
 *
 * This version supports runtime (un)loading.
 *
 * Internally, the cache has two parts: the Primary cache, which is loaded
 * from data files, and the Auxiliary cache, which is generated at runtime.
 * To outsiders, there is no difference between these two caches. The
 * only visible difference is that lumps in the auxiliary cache use indices
 * starting from AUXILIARY_BASE.
 *
 * The W_Select() function is responsible for activating the right cache
 * when a lump index is provided. Functions that don't know the lump index
 * will have to check both the primary and the auxiliary caches (e.g.,
 * W_CheckLumpNumForName()).
 */

#ifndef LIBDENG_FILESYS_WAD_H
#define LIBDENG_FILESYS_WAD_H

#include "dd_zip.h"
#include "wadfile.h"
#include "lumpfile.h"

#define AUXILIARY_BASE      100000000

/// Register the console commands, variables, etc..., of this module.
void W_Register(void);

/// Initialize this module. Cannot be re-initialized, must shutdown first.
void W_Init(void);

void W_Shutdown(void);

int W_LumpCount(void);

/**
 * \post No more WADs will be loaded in startup mode.
 */
void W_EndStartup(void);

/**
 * Remove all records flagged Runtime.
 * @return  Number of records removed.
 */
int W_Reset(void);

zipfile_t* W_AddZipFile(const char* fileName, DFILE* handle);
wadfile_t* W_AddWadFile(const char* fileName, DFILE* handle);
lumpfile_t* W_AddLumpFile(const char* fileName, DFILE* handle, boolean isDehackedPatch);

boolean W_RemoveFile(const char* fileName);

/**
 * Try to open the specified WAD archive into the auxiliary lump cache.
 *
 * @param prevOpened  If not @c NULL re-use this previously opened file rather
 *      than opening a new one. WAD loader takes ownership of the file.
 *      Release with W_CloseAuxiliary.
 * @return  Base index for lumps in this archive.
 */
lumpnum_t W_OpenAuxiliary3(const char* fileName, DFILE* prevOpened, boolean silent);
lumpnum_t W_OpenAuxiliary2(const char* fileName, DFILE* prevOpened);
lumpnum_t W_OpenAuxiliary(const char* fileName);

void W_CloseAuxiliary(void);

/**
 * @return  @c -1, if name not found, else lump num.
 */
lumpnum_t W_CheckLumpNumForName(const char* name);
lumpnum_t W_CheckLumpNumForName2(const char* name, boolean silent);

/// \note As per W_CheckLumpNumForName but results in a fatal error if not found.
lumpnum_t W_GetLumpNumForName(const char* name);

void W_ReadLump(lumpnum_t lumpNum, char* dest);
void W_ReadLumpSection(lumpnum_t lumpNum, char* dest, size_t startOffset, size_t length);
const char* W_CacheLump(lumpnum_t lumpNum, int tag);

void W_CacheChangeTag(lumpnum_t lumpNum, int tag);

/// @return  Name of the lump associated with @a lumpNum.
const char* W_LumpName(lumpnum_t lumpNum);

/// @return  Buffer size needed to load the data associated with @a lumpNum in bytes.
size_t W_LumpLength(lumpnum_t lumpNum);

/**
 * @return  "Last modified" timestamp of the zip entry.
 */
uint W_LumpLastModified(lumpnum_t lumpNum);

/// @return  Name of the WAD file where the data associated with @a lumpNum resides.
///     Always returns a valid filename (or an empty string).
const char* W_LumpSourceFile(lumpnum_t lumpNum);

/// @return  @c true iff the data associated with @a lumpNum resides in an IWAD.
boolean W_LumpIsFromIWAD(lumpnum_t lumpNum);

/**
 * Compiles a list of PWAD file names, separated by @a delimiter.
 */
void W_GetPWADFileNames(char* buf, size_t bufSize, char delimiter);

/**
 * Calculated using the lumps of the main IWAD.
 */
uint W_CRCNumber(void);

/**
 * Print the contents of the primary lump directory to stdout.
 */
void W_PrintLumpDirectory(void);

/**
 * Write the data associated with @a lumpNum to @a fileName.
 *
 * @param lumpNum  Logical lump index associated with the data being dumped.
 * @param fileName  If not @c NULL write the associated data to this path.
 *      Can be @c NULL in which case the fileName will be chosen automatically.
 * @return  @c true iff successful.
 */
boolean W_DumpLump(lumpnum_t lumpNum, const char* fileName);

/// @return  Size of a zipentry specified by index.
size_t Zip_GetSize(lumpnum_t lumpNum);

/// @return  "Last modified" timestamp of the zip entry.
uint Zip_LastModified(lumpnum_t lumpNum);

/// @return  The name of the Zip archive where the referenced file resides.
const char* Zip_SourceFile(lumpnum_t lumpNum);

void Zip_ReadFile(lumpnum_t lumpNum, char* buffer);
void Zip_ReadFileSection(lumpnum_t lumpNum, char* buffer, size_t startOffset, size_t length);

/**
 * Find a specific path in the Zip LumpDirectory.
 *
 * @param searchPath  Path to search for. Relative paths are converted are made absolute.
 *
 * @return  Non-zero if something is found.
 */
lumpnum_t Zip_Find(const char* searchPath);

/**
 * Iterate over nodes in the Zip LumpDirectory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Zip_Iterate2(int (*callback) (const lumpinfo_t*, void*), void* paramaters);
int Zip_Iterate(int (*callback) (const lumpinfo_t*, void*));

#endif /* LIBDENG_FILESYS_WAD_H */
