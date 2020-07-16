/**
 * @file fs_util.h
 *
 * Miscellaneous file system utility routines.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_FILESYS_UTIL_H
#define DE_FILESYS_UTIL_H

#include "../libdoomsday.h"
#include "../filesys/file.h"
#include "dd_types.h"

#ifdef WIN32
#  include "fs_windows.h"
#endif

#ifdef __cplusplus

#include <de/block.h>
#include <de/file.h>

extern "C" {
#endif

LIBDOOMSDAY_PUBLIC int F_Access(const char *nativePath);

LIBDOOMSDAY_PUBLIC int F_FileExists(const char *path);

LIBDOOMSDAY_PUBLIC dd_bool F_MakePath(const char *path);

/**
 * Converts directory slashes to our internal '/'.
 * @return  @c true iff the path was modified.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_FixSlashes(ddstring_t *dst, const ddstring_t *src);

/**
 * Appends a slash at the end of @a path if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_AppendMissingSlashCString(char *path, size_t maxLen);

/**
 * Converts directory slashes to tha used by the host file system.
 * @return  @c true iff the path was modified.
 */
LIBDOOMSDAY_PUBLIC dd_bool F_ToNativeSlashes(ddstring_t *dst, const ddstring_t *src);

/**
 * @return  @c true, if the given path is absolute (starts with \ or / or the
 *          second character is a ':' (drive).
 */
LIBDOOMSDAY_PUBLIC dd_bool F_IsAbsolute(const ddstring_t *path);

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
LIBDOOMSDAY_PUBLIC dd_bool F_ExpandBasePath(ddstring_t *dst, const ddstring_t *src);

LIBDOOMSDAY_PUBLIC const char *F_PrettyPath(const char *path);

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
LIBDOOMSDAY_PUBLIC dd_bool F_DumpFile(res::File1 &file, const char *outputPath);

#ifdef __cplusplus
} // extern "C"

/**
 * Write data into a native file.
 *
 * @param data      Data to write.
 * @param filePath  Path of the file to create (existing file replaced).
 *
 * @return @c true if successful, otherwise @c false.
 */
LIBDOOMSDAY_PUBLIC bool F_DumpNativeFile(const de::Block &data, const de::NativePath &filePath);

#endif

#endif // DE_FILESYS_UTIL_H
