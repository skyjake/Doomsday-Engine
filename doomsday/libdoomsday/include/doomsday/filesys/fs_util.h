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
#include "../filesys/file.h"
#include "dd_types.h"

#ifdef WIN32
#  include "fs_windows.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC int F_FileExists(char const *path);

/**
 * Converts directory slashes to our internal '/'.
 * @return  @c true iff the path was modified.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_FixSlashes(ddstring_t *dst, ddstring_t const *src);

/**
 * Appends a slash at the end of @a path if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_AppendMissingSlashCString(char *path, size_t maxLen);

/**
 * Converts directory slashes to tha used by the host file system.
 * @return  @c true iff the path was modified.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_ToNativeSlashes(ddstring_t *dst, ddstring_t const *src);

/**
 * @return  @c true, if the given path is absolute (starts with \ or / or the
 *          second character is a ':' (drive).
 */
LIBDOOMSDAY_PUBLIC dd_bool F_IsAbsolute(ddstring_t const *path);

/**
 * Attempt to prepend the base path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 * @param base  Base to attempt to prepend to @a src.
 *
 * @return  @c true iff the path was prepended.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_PrependBasePath2(ddstring_t *dst, ddstring_t const *src, ddstring_t const *base);
LIBDOOMSDAY_PUBLIC dd_bool F_PrependBasePath(ddstring_t *dst, ddstring_t const *src /*, ddstring_t const *base = ddBasePath*/);

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
LIBDOOMSDAY_PUBLIC dd_bool F_ExpandBasePath(ddstring_t *dst, ddstring_t const *src);

/**
 * Write the data associated with the specified lump index to @a outputPath.
 *
 * @param file        File to be dumped.
 * @param outputPath  If not @c NULL write the associated data to this path.
 *                    Can be @c NULL in which case the path and file name will
 *                    be chosen automatically.
 *
 * @return  @c true iff successful.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_DumpFile(de::File1 &file, char const *fileName);

/**
 * Write data into a file.
 *
 * @param data  Data to write.
 * @param size  Size of the data in bytes.
 * @param path  Path of the file to create (existing file replaced).
 *
 * @return @c true if successful, otherwise @c false.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_Dump(void const *data, size_t size, char const *path);

LIBDOOMSDAY_PUBLIC char const *F_PrettyPath(char const *path);

LIBDOOMSDAY_PUBLIC dd_bool F_MakePath(char const *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_FILESYS_UTIL_H
