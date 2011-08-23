/**\file sys_file.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * Virtual file system and file (input) stream abstraction layer.
 *
 * This version supports runtime (un)loading.
 *
 * File input. Can read from real files or WAD lumps. Note that
 * reading from WAD lumps means that a copy is taken of the lump when
 * the corresponding 'file' is opened. With big files this uses
 * considerable memory and time.
 *
 * Internally, the cache has two parts: the Primary cache, which is loaded
 * from data files, and the Auxiliary cache, which is generated at runtime.
 * To outsiders, there is no difference between these two caches. The
 * only visible difference is that lumps in the auxiliary cache use indices
 * starting from AUXILIARY_BASE.
 *
 * Functions that don't know the lumpnum of file will have to check both
 * the primary and the auxiliary caches (e.g., W_CheckLumpNumForName()).
 */

#ifndef LIBDENG_FILESYS_MAIN_H
#define LIBDENG_FILESYS_MAIN_H

#include "zipfile.h"
#include "wadfile.h"
#include "lumpfile.h"
#include "pathdirectory.h"

#define AUXILIARY_BASE      100000000

/// Register the console commands, variables, etc..., of this module.
void F_Register(void);

/// Initialize this module. Cannot be re-initialized, must shutdown first.
void F_Init(void);

/// Shutdown this module.
void F_Shutdown(void);

/**
 * \post No more WADs will be loaded in startup mode.
 */
void F_EndStartup(void);

/**
 * Remove all file records flagged Runtime.
 * @return  Number of records removed.
 */
int F_Reset(void);

/// @return  Number of files in the currently active primary LumpDirectory.
int F_LumpCount(void);

zipfile_t* W_AddZipFile(const char* fileName, DFILE* handle);
wadfile_t* W_AddWadFile(const char* fileName, DFILE* handle);
lumpfile_t* W_AddLumpFile(const char* fileName, DFILE* handle, boolean isDehackedPatch);

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

/**
 * Files with a .wad extension are archived data files with multiple 'lumps',
 * other files are single lumps whose base filename will become the lump name.
 *
 * \note Lump names can appear multiple times. The name searcher looks backwards,
 * so a later file can override an earlier one.
 *
 * @return  @c true, if the operation is successful.
 */
boolean F_AddFile(const char* fileName, boolean allowDuplicate);
boolean F_AddFiles(const char* const* filenames, size_t num, boolean allowDuplicate);

boolean F_RemoveFile(const char* fileName);
boolean F_RemoveFiles(const char* const* filenames, size_t num);

/**
 * Opens the given file (will be translated) for reading.
 * "t" = text mode (with real files, lumps are always binary)
 * "b" = binary
 * "f" = must be a real file in the local file system.
 * "x" = just test for access (don't buffer anything)
 */
DFILE* F_Open(const char* path, const char* mode);

/**
 * @return  The time when the file was last modified, as seconds since
 * the Epoch else zero if the file is not found.
 */
unsigned int F_GetLastModified(const char* fileName);

void F_InitializeResourcePathMap(void);

/**
 * The path names are converted to full paths before adding to the table.
 * Files in the source directory are mapped to the target directory.
 */
void F_AddResourcePathMapping(const char* source, const char* destination);

/**
 * Initialize the lump directory > vfs translations.
 * \note Should be called after WADs have been processed.
 */
void F_InitDirec(void);

void F_ShutdownDirec(void);

/**
 * Compiles a list of PWAD file names, separated by @a delimiter.
 */
void F_GetPWADFileNames(char* buf, size_t bufSize, char delimiter);

/**
 * Calculated using the lumps of the main IWAD.
 */
uint F_CRCNumber(void);

/**
 * Print the contents of the primary lump directory to stdout.
 */
void F_PrintLumpDirectory(void);

/**
 * Write the data associated with @a lumpNum to @a fileName.
 *
 * @param lumpNum  Logical lump index associated with the data being dumped.
 * @param fileName  If not @c NULL write the associated data to this path.
 *      Can be @c NULL in which case the fileName will be chosen automatically.
 * @return  @c true iff successful.
 */
boolean F_DumpLump(lumpnum_t lumpNum, const char* fileName);

/**
 * Parm is passed on to the callback, which is called for each file
 * matching the filespec. Absolute path names are given to the callback.
 * Zip directory, DD_DIREC and the real files are scanned.
 */
int F_AllResourcePaths2(const char* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters),
    void* paramaters);
int F_AllResourcePaths(const char* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters));

#endif /* LIBDENG_FILESYS_MAIN_H */
