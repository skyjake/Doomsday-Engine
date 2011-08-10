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

#include "sys_file.h"

#define AUXILIARY_BASE      100000000

struct lumpdirectory_s;

// LumpInfo record. POD.
typedef struct {
    lumpname_t name; /// Ends in '\0'.
    size_t position; /// Offset from start of WAD file.
    size_t size;
} wadfile_lumpinfo_t;

/**
 * @defgroup wadFileFlags  Wad file flags.
 */
/*@{*/
#define WFF_IWAD                    0x1 // File is marked IWAD (else its a PWAD).
#define WFF_RUNTIME                 0x2 // Loaded at runtime (for reset).
/*@}*/

/**
 * WadFile. Runtime representation of a WAD file.
 *
 * @ingroup FS
 */
typedef struct wadfile_s {
    int _flags; /// @see wadFileFlags
    size_t _numLumps;
    wadfile_lumpinfo_t* _lumpInfo;
    void** _lumpCache;
    DFILE* _handle;
    struct lumpdirectory_s* _directory; // All lumps from this file go into the same LumpDirectory
    ddstring_t _absolutePath;

    struct wadfile_s* next;
} wadfile_t;

struct lumpdirectory_s* WadFile_Directory(wadfile_t* fr);
size_t WadFile_NumLumps(wadfile_t* fr);
/// @return  @see wadFileFlags
int WadFile_Flags(wadfile_t* fr);
DFILE* WadFile_Handle(wadfile_t* fr);
const ddstring_t* WadFile_AbsolutePath(wadfile_t* fr);

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

wadfile_t* W_AddArchive(const char* fileName, DFILE* handle);
wadfile_t* W_AddFile(const char* fileName, DFILE* handle, boolean isDehackedPatch);

/// \note Also used with archives.
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

/**
 * Read the data associated with @a lumpNum into buffer @a dest.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 * @param dest  Buffer to read into. Must be at least W_LumpLength() bytes.
 */
void W_ReadLump(lumpnum_t lumpNum, char* dest);

/**
 * Read a subsection of the data associated with @a lumpNum into buffer @a dest.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 * @param dest  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param startOffset  Offset from the beginning of the lump to start reading.
 * @param length  Number of bytes to be read.
 */
void W_ReadLumpSection(lumpnum_t lumpNum, void* dest, size_t startOffset, size_t length);

/**
 * Read the data associated with @a lumpNum into the cache.
 *
 * @param lumpNum  Logical lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const char* W_CacheLump(lumpnum_t lumpNum, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param lumpNum  Logical lump index associated with the data.
 * @param tag  Zone purge level/cache tag to use.
 */
void W_CacheChangeTag(lumpnum_t lumpNum, int tag);

/// @return  Name of the lump associated with @a lumpNum.
const char* W_LumpName(lumpnum_t lumpNum);

/// @return  Buffer size needed to load the data associated with @a lumpNum in bytes.
size_t W_LumpLength(lumpnum_t lumpNum);

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

#endif /* LIBDENG_FILESYS_WAD_H */
