/**
 * @file fs_util.h
 *
 * Miscellaneous file system utility routines.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "../libdoomsday.h"
#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC void F_FileDir(ddstring_t* dst, const ddstring_t* str);

LIBDOOMSDAY_PUBLIC void F_FileName(ddstring_t* dst, const char* src);

LIBDOOMSDAY_PUBLIC void F_FileNameAndExtension(ddstring_t* dst, const char* src);

LIBDOOMSDAY_PUBLIC int F_FileExists(const char *path);

LIBDOOMSDAY_PUBLIC const char* F_FindFileExtension(const char* path);

LIBDOOMSDAY_PUBLIC void F_ExtractFileBase(char* dest, const char* path, size_t len);

LIBDOOMSDAY_PUBLIC void F_ExtractFileBase2(char* dest, const char* path, size_t len, int ignore);

/**
 * Converts directory slashes to our internal '/'.
 * @return  @c true iff the path was modified.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_FixSlashes(ddstring_t* dst, const ddstring_t* src);

/**
 * Appends a slash at the end of @a pathStr if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_AppendMissingSlash(ddstring_t* pathStr);

/**
 * Appends a slash at the end of @a path if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_AppendMissingSlashCString(char* path, size_t maxLen);

/**
 * Converts directory slashes to tha used by the host file system.
 * @return  @c true iff the path was modified.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_ToNativeSlashes(ddstring_t* dst, const ddstring_t* src);

/**
 * Convert the symbolic path into a real path.
 * @todo: This seems rather redundant; refactor callers.
 */
LIBDOOMSDAY_PUBLIC void F_ResolveSymbolicPath(ddstring_t* dst, const ddstring_t* src);

/**
 * @return  @c true, if the given path is absolute (starts with \ or / or the
 *          second character is a ':' (drive).
 */
LIBDOOMSDAY_PUBLIC dd_bool F_IsAbsolute(const ddstring_t* path);

/**
 * @return  @c true iff the path can be made into a relative path.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_IsRelativeToBase(const char* path, const char* base);

/**
 * Attempt to remove the base path if found at the beginning of the path.
 *
 * @param dst  Potential base-relative path written here.
 * @param src  Possibly absolute path.
 * @param base  Base to attempt to remove from @a src.
 *
 * @return  @c true iff the base path was found and removed.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_RemoveBasePath2(ddstring_t* dst, const ddstring_t* src, const ddstring_t* base);
LIBDOOMSDAY_PUBLIC dd_bool F_RemoveBasePath(ddstring_t* dst, const ddstring_t* src /*, const ddstring_t* base=ddBasePath*/);

/**
 * Attempt to prepend the base path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 * @param base  Base to attempt to prepend to @a src.
 *
 * @return  @c true iff the path was prepended.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_PrependBasePath2(ddstring_t* dst, const ddstring_t* src, const ddstring_t* base);
LIBDOOMSDAY_PUBLIC dd_bool F_PrependBasePath(ddstring_t* dst, const ddstring_t* src /*, const ddstring_t* base=ddBasePath*/);

/**
 * Attempt to prepend the current work path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 *
 * @return  @c true iff the path was prepended.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_PrependWorkPath(ddstring_t* dst, const ddstring_t* src);

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
LIBDOOMSDAY_PUBLIC dd_bool F_ExpandBasePath(ddstring_t* dst, const ddstring_t* src);

LIBDOOMSDAY_PUBLIC dd_bool F_TranslatePath(ddstring_t* dst, const ddstring_t* src);

LIBDOOMSDAY_PUBLIC dd_bool F_MakeAbsolute(ddstring_t* dst, const ddstring_t* src);

/**
 * Write the data associated with the specified lump index to @a fileName.
 *
 * @param lumpNum           Absolute index of the lump to open.
 *
 * @return  @c true iff successful.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_DumpLump(lumpnum_t lumpNum/*, fileName = 0*/);

/**
 * @copydoc F_DumpLump
 * @param fileName          If not @c NULL write the associated data to this path.
 *                          Can be @c NULL in which case the fileName will be chosen automatically.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_DumpLump2(lumpnum_t lumpNum, char const* fileName);

/**
 * Write data into a file.
 *
 * @param data  Data to write.
 * @param size  Size of the data in bytes.
 * @param path  Path of the file to create (existing file replaced).
 *
 * @return @c true if successful, otherwise @c false.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_Dump(void const* data, size_t size, char const* path);

LIBDOOMSDAY_PUBLIC const char* F_PrettyPath(const char* path);

LIBDOOMSDAY_PUBLIC uint F_GetLastModified(const char* path);

LIBDOOMSDAY_PUBLIC dd_bool F_MakePath(const char* path);

LIBDOOMSDAY_PUBLIC void F_ReadLine(char* buffer, size_t len, struct filehandle_s *file);

#ifdef __cplusplus
} // extern "C"

#include <de/String>

/**
 * Performs a case-insensitive pattern match. The pattern can contain
 * wildcards.
 *
 * @param filePath  Path to match.
 * @param pattern   Pattern with * and ? as wildcards.
 *
 * @return  @c true, if @a filePath matches the pattern.
 */
LIBDOOMSDAY_PUBLIC bool F_MatchFileName(de::String const &filePath, de::String const &pattern);

#endif

#endif // LIBDENG_FILESYS_UTIL_H
