/**\file fs_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * the primary and the auxiliary caches (e.g., F_CheckLumpNumForName()).
 */

#ifndef LIBDENG_FILESYS_MAIN_H
#define LIBDENG_FILESYS_MAIN_H

#include <stdio.h>

#include "zipfile.h"
#include "wadfile.h"
#include "lumpfile.h"
#include "filelist.h"
#include "pathdirectory.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * Reset known fileId records so that the next time F_CheckFileId() is
 * called on a file, it will pass.
 */
void F_ResetFileIds(void);

/**
 * Calculate an identifier for the file based on its full path name.
 * The identifier is the MD5 hash of the path.
 */
void F_GenerateFileId(const char* str, byte identifier[16]);

void F_PrintFileId(byte identifier[16]);

/**
 * Maintains a list of identifiers already seen.
 *
 * @return @c true if the given file can be opened, or
 *         @c false, if it has already been opened.
 */
boolean F_CheckFileId(const char* path);

/// @return  @c true if the FileId associated with @a path was released.
boolean F_ReleaseFileId(const char* path);

/// @return  Number of files in the currently active primary LumpDirectory.
int F_LumpCount(void);

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum);

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum);

const char* F_LumpName(lumpnum_t absoluteLumpNum);

size_t F_LumpLength(lumpnum_t absoluteLumpNum);

const char* F_LumpSourceFile(lumpnum_t absoluteLumpNum);

uint F_LumpLastModified(lumpnum_t absoluteLumpNum);

/**
 * Given a logical @a lumpnum retrieve the associated file object.
 *
 * \post The active LumpDirectory may have changed!
 *
 * @param absoluteLumpNum  Logical lumpnum associated to the file being looked up.
 * @param lumpIdx  If not @c NULL the translated lumpnum within the owning file object is written here.
 * @return  Found file object else @c NULL
 */
AbstractFile* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx);
AbstractFile* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum);

const LumpInfo* F_FindInfoForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx);
const LumpInfo* F_FindInfoForLumpNum(lumpnum_t absoluteLumpNum);

lumpnum_t F_CheckLumpNumForName2(const char* name, boolean silent);
lumpnum_t F_CheckLumpNumForName(const char* name);

/**
 * Try to open the specified WAD archive into the auxiliary lump cache.
 * @return  Base index for lumps in this archive.
 */
lumpnum_t F_OpenAuxiliary3(const char* fileName, size_t baseOffset, boolean silent);
lumpnum_t F_OpenAuxiliary2(const char* fileName, size_t baseOffset); /* silent = false */
lumpnum_t F_OpenAuxiliary(const char* fileName); /* baseOffset = 0 */

void F_CloseAuxiliary(void);

/**
 * Find a lump in the Zip LumpDirectory.
 *
 * @param path  Path to search for. Relative paths are made absolute if necessary.
 * @param lumpIdx  If not @c NULL the translated lumpnum within the owning file object is written here.
 * @return  File system object representing the file which contains the found lump else @c NULL.
 */
AbstractFile* F_FindLumpFile(const char* path, int* lumpIdx);

/**
 * Files with a .wad extension are archived data files with multiple 'lumps',
 * other files are single lumps whose base filename will become the lump name.
 *
 * @param path  Path to the file to be opened. Either a "real" file in the local
 *              file system, or a "virtual" file in the virtual file system.
 * @param baseOffset  Offset from the start of the file in bytes to begin.
 * @param allowDuplicate  @c true = allow opening multiple copies of the same file.
 * @return  Handle to the opened file if the operation is successful, else @c NULL.
 */
DFile* F_AddFile(const char* path, size_t baseOffset, boolean allowDuplicate);

/**
 * Attempt to remove a file from the virtual file system.
 *
 * @param permitRequired  @c true= allow removal of resources marked as "required"
 *                        by the currently loaded Game.
 * @return @c true if the operation is successful.
 */
boolean F_RemoveFile(const char* path, boolean permitRequired);

boolean F_AddFiles(const char* const* paths, size_t num, boolean allowDuplicate);
boolean F_RemoveFiles(const char* const* paths, size_t num, boolean permitRequired);

/**
 * @return  @c true if the file can be opened for reading.
 */
int F_Access(const char* path);

/**
 * Opens the given file (will be translated) for reading.
 *
 * \post If @a allowDuplicate = @c false a new file ID for this will have been
 * added to the list of known file identifiers if this file hasn't yet been
 * opened. It is the responsibility of the caller to release this identifier when done.
 *
 * @param path  Possibly relative or mapped path to the resource being opened.
 * @param mode
 *      't' = text mode (with real files, lumps are always binary)
 *      'b' = binary
 *      'f' = must be a real file in the local file system
 * @param baseOffset  Offset from the start of the file in bytes to begin.
 * @param allowDuplicate  @c false = open only if not already opened.
 * @return  Opened file reference/handle else @c NULL.
 */
DFile* F_Open3(const char* path, const char* mode, size_t baseOffset, boolean allowDuplicate);
DFile* F_Open2(const char* path, const char* mode, size_t baseOffset); /* allowDuplicate = true */
DFile* F_Open(const char* path, const char* mode); /* baseOffset = 0 */

/**
 * Try to locate the specified lump for reading.
 *
 * @param lumpNum  Absolute index of the lump to open.
 *
 * @return  Handle to the opened file if found.
 */
DFile* F_OpenLump(lumpnum_t lumpNum);

/**
 * Write the data associated with the specified lump index to @a fileName.
 *
 * @param lumpNum  Absolute index of the lump to open.
 * @param fileName  If not @c NULL write the associated data to this path.
 *      Can be @c NULL in which case the fileName will be chosen automatically.
 * @return  @c true iff successful.
 */
boolean F_DumpLump(lumpnum_t lumpNum, const char* fileName);

/**
 * Write data into a file.
 *
 * @param data  Data to write.
 * @param size  Size of the data in bytes.
 * @param path  Path of the file to create (existing file replaced).
 *
 * @return @c true if successful, otherwise @c false.
 */
boolean F_Dump(const void *data, size_t size, const char* path);

/**
 * @return  The time when the file was last modified, as seconds since
 * the Epoch else zero if the file is not found.
 */
uint F_GetLastModified(const char* fileName);

void F_InitVirtualDirectoryMappings(void);

/**
 * Add a new virtual directory mapping from source to destination in the vfs.
 * \note Paths will be transformed into absolute paths if needed.
 */
void F_AddVirtualDirectoryMapping(const char* source, const char* destination);

/// \note Should be called after WADs have been processed.
void F_InitLumpDirectoryMappings(void);

/**
 * Add a new lump mapping so that @a lumpName becomes visible as @a symbolicPath
 * throughout the vfs.
 * \note @a symbolicPath will be transformed into an absolute path if needed.
 */
void F_AddLumpDirectoryMapping(const char* lumpName, const char* symbolicPath);

/**
 * Compiles a list of PWAD file names, separated by @a delimiter.
 */
void F_GetPWADFileNames(char* buf, size_t bufSize, const char* delimiter);

/**
 * Calculate a CRC for the loaded file list.
 */
uint F_CRCNumber(void);

/// Clear all references to this file.
void F_ReleaseFile(AbstractFile* file);

/// Close this file.
void F_Close(DFile* file);

/// Completely destroy this file; close if open, clear references and any acquired identifiers.
void F_Delete(DFile* file);

AutoStr* F_ComposeLumpPath2(AbstractFile* file, int lumpIdx, char delimiter);
AutoStr* F_ComposeLumpPath(AbstractFile* file, int lumpIdx); /*delimiter='/'*/

struct pathdirectorynode_s* F_LumpDirectoryNode(AbstractFile* file, int lumpIdx);

const LumpInfo* F_LumpInfo(AbstractFile* file, int lumpIdx);

size_t F_ReadLumpSection(AbstractFile* file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length);

const uint8_t* F_CacheLump(AbstractFile* file, int lumpIdx);

void F_UnlockLump(AbstractFile* file, int lumpIdx);

/**
 * Parm is passed on to the callback, which is called for each file
 * matching the filespec. Absolute path names are given to the callback.
 * Zip directory, DD_DIREC and the real files are scanned.
 *
 * @param flags  @see searchPathFlags
 */
int F_AllResourcePaths2(const char* searchPath, int flags,
    int (*callback) (const ddstring_t* path, PathDirectoryNodeType type, void* paramaters),
    void* paramaters);
int F_AllResourcePaths(const char* searchPath, int flags,
    int (*callback) (const ddstring_t* path, PathDirectoryNodeType type, void* paramaters));

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_MAIN_H */
