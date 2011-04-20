/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * WAD Files and Data Lump Cache.
 */

#ifndef LIBDENG_WAD_H
#define LIBDENG_WAD_H

void DD_RegisterVFS(void);

/**
 * Initializes the file system.
 */
void W_Init(void);

int W_NumLumps(void);

/**
 * Calculated using the lumps of the main IWAD.
 */
uint W_CRCNumber(void);

/**
 * @return              @c true, iff the file exists and appears to be an IWAD.
 */
int W_IsIWAD(const char* fn);

/**
 * \post No more WADs will be loaded in startup mode.
 */
void W_EndStartup(void);

/**
 * Remove all records flagged Runtime.
 */
void W_Reset(void);

/**
 * Copies the file name of the IWAD to the given buffer.
 */
void W_GetIWADFileName(char* buf, size_t bufSize);

/**
 * Compiles a list of PWAD file names, separated by the specified character.
 */
void W_GetPWADFileNames(char* buf, size_t bufSize, char separator);

void W_DumpLumpDir(void);

/**
 * Files with a .wad extension are wadlink files with multiple lumps,
 * other files are single lumps with the base filename for the lump name.
 *
 * \note Lump names can appear multiple times. The name searcher looks backwards,
 * so a later file can override an earlier one.
 *
 * @return              @c true, if the operation is successful.
 */
boolean W_AddFile(const char* fileName, boolean allowDuplicate);
boolean W_AddFiles(const char* const* filenames, size_t num, boolean allowDuplicate);

boolean W_RemoveFile(const char* fileName);
boolean W_RemoveFiles(const char* const* filenames, size_t num);

lumpnum_t W_OpenAuxiliary(const char* fileName);

/**
 * @return              @c -1, if name not found, else lump num.
 */
lumpnum_t W_CheckNumForName(const char* name);
lumpnum_t W_CheckNumForName2(const char* name, boolean silent);

/**
 * Calls W_CheckNumForName, but bombs out if not found.
 */
lumpnum_t W_GetNumForName(const char* name);

/**
 * Get the name of the given lump.
 */
const char* W_LumpName(lumpnum_t lumpNum);

/**
 * @return              The buffer size needed to load the given lump.
 */
size_t W_LumpLength(lumpnum_t lumpNum);

/**
 * @return              The name of the WAD file where the given lump resides.
 *                      Always returns a valid filename (or an empty string).
 */
const char* W_LumpSourceFile(lumpnum_t lumpNum);

/**
 * @return              @ true, if the specified lump is in an IWAD.
 *                      Otherwise it's from a PWAD.
 */
boolean W_LumpFromIWAD(lumpnum_t lumpNum);

/**
 * Loads the lump into the given buffer, which must be >= W_LumpLength().
 */
void W_ReadLump(lumpnum_t lumpNum, void* dest);
void W_ReadLumpSection(lumpnum_t lumpNum, void* dest, size_t startOffset, size_t length);

/**
 * If called with the special purgelevel PU_GETNAME, returns a pointer
 * to the name of the lump.
 */
const void* W_CacheLumpNum(lumpnum_t lumpNum, int tag);

/**
 * Identical to @see W_CacheLumpNum except the lump reference is a name.
 */
const void* W_CacheLumpName(const char* name, int tag);

void W_ChangeCacheTag(lumpnum_t lumpNum, int tag);

/**
 * Writes the specifed lump to file with the given name.
 */
boolean W_DumpLump(lumpnum_t, const char* fileName);

#endif /* LIBDENG_WAD_H */
