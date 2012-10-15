/**
 * @file fs_util.h
 *
 * Miscellaneous file system utility routines.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_UTIL_H
#define LIBDENG_FILESYS_UTIL_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void F_FileDir(ddstring_t* dst, const ddstring_t* str);

void F_FileName(ddstring_t* dst, const char* src);

void F_FileNameAndExtension(ddstring_t* dst, const char* src);

const char* F_FindFileExtension(const char* path);

void F_ExtractFileBase2(char* dest, const char* path, size_t len, int ignore);
void F_ExtractFileBase(char* dest, const char* path, size_t len);

/**
 * Checks if a file exists in the native file system.
 *
 * @param file  File to check existence of. Relative path directives are expanded
 *              automatically: '>' '}' (plus '~' on Unix-based platforms).
 *
 * @return @c 0 if the path points to a readable file on the local file system.
 */
int F_FileExists(const char* path);

/**
 * @return  The time when the file/directory was last modified, as seconds since
 *          the Epoch else zero if @a path is not found.
 *
 * @attention This only works on native paths.
 */
uint F_GetLastModified(const char* path);

/**
 * Check that the given directory exists. If it doesn't, create it.
 *
 * @return  @c true if successful.
 */
boolean F_MakePath(const char* path);

/**
 * Converts directory slashes to our internal '/'.
 * @return  @c true iff the path was modified.
 */
boolean F_FixSlashes(ddstring_t* dst, const ddstring_t* src);

/**
 * Appends a slash at the end of @a pathStr if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
boolean F_AppendMissingSlash(ddstring_t* pathStr);

/**
 * Appends a slash at the end of @a path if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
boolean F_AppendMissingSlashCString(char* path, size_t maxLen);

/**
 * Converts directory slashes to tha used by the host file system.
 * @return  @c true iff the path was modified.
 */
boolean F_ToNativeSlashes(ddstring_t* dst, const ddstring_t* src);

/**
 * Convert the symbolic path into a real path.
 * @todo: This seems rather redundant; refactor callers.
 */
void F_ResolveSymbolicPath(ddstring_t* dst, const ddstring_t* src);

/**
 * @return  @c true, if the given path is absolute (starts with \ or / or the
 *          second character is a ':' (drive).
 */
boolean F_IsAbsolute(const ddstring_t* path);

/**
 * @return  @c true iff the path can be made into a relative path.
 */
boolean F_IsRelativeToBase(const char* path, const char* base);

/**
 * Attempt to remove the base path if found at the beginning of the path.
 *
 * @param dst  Potential base-relative path written here.
 * @param src  Possibly absolute path.
 * @param base  Base to attempt to remove from @a src.
 *
 * @return  @c true iff the base path was found and removed.
 */
boolean F_RemoveBasePath2(ddstring_t* dst, const ddstring_t* src, const ddstring_t* base);
boolean F_RemoveBasePath(ddstring_t* dst, const ddstring_t* src /*, const ddstring_t* base=ddBasePath*/);

/**
 * Attempt to prepend the base path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 * @param base  Base to attempt to prepend to @a src.
 *
 * @return  @c true iff the path was prepended.
 */
boolean F_PrependBasePath2(ddstring_t* dst, const ddstring_t* src, const ddstring_t* base);
boolean F_PrependBasePath(ddstring_t* dst, const ddstring_t* src /*, const ddstring_t* base=ddBasePath*/);

/**
 * Attempt to prepend the current work path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 *
 * @return  @c true iff the path was prepended.
 */
boolean F_PrependWorkPath(ddstring_t* dst, const ddstring_t* src);

/**
 * Expands relative path directives like '>'.
 *
 * @note Despite appearances this function is *not* an alternative version of
 *       M_TranslatePath accepting ddstring_t arguments. Key differences:
 *
 * ! Handles '~' on UNIX-based platforms.
 * ! No other transform applied to @a src path.
 *
 * @param dst  Expanded path written here.
 * @param src  Original path.
 *
 * @return  @c true iff the path was expanded.
 */
boolean F_ExpandBasePath(ddstring_t* dst, const ddstring_t* src);

boolean F_MakeAbsolute(ddstring_t* dst, const ddstring_t* src);

/// @todo Refactor me away (duplication).
boolean F_TranslatePath(ddstring_t* dst, const ddstring_t* src);

/**
 * @warning Not thread-safe!
 * @return  A prettier copy of the original path.
 */
const char* F_PrettyPath(const char* path);

/**
 * This is a case-insensitive test.
 * I do hope this algorithm works like it should...
 *
 * @return  @c true, if @a filePath matches the pattern.
 */
int F_MatchFileName(const char* filePath, const char* pattern);

/**
 * Write the data associated with the specified lump index to @a fileName.
 *
 * @param absolutelumpNum   Absolute index of the lump to open.
 * @param fileName          If not @c NULL write the associated data to this path.
 *                          Can be @c NULL in which case the fileName will be chosen automatically.
 *
 * @return  @c true iff successful.
 */
boolean F_DumpLump2(lumpnum_t lumpNum, char const* fileName);
boolean F_DumpLump(lumpnum_t lumpNum/*, fileName = 0*/);

/**
 * Write data into a file.
 *
 * @param data  Data to write.
 * @param size  Size of the data in bytes.
 * @param path  Path of the file to create (existing file replaced).
 *
 * @return @c true if successful, otherwise @c false.
 */
boolean F_Dump(void const* data, size_t size, char const* path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_FILESYS_UTIL_H
