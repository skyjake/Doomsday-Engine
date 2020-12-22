/** @file api_filesys.h Public API of the file system.
 * Primary header file for the Doomsday Engine Public API
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_FILE_H
#define DOOMSDAY_API_FILE_H

#include <de/legacy/str.h>
#include "api_base.h"

DE_API_TYPEDEF(F)
{
    de_api_t api;

    int             (*Access)(const char* path);

    /**
     * Checks if a file exists in the native file system.
     *
     * @param file  File to check existence of. Relative path directives are expanded
     *              automatically: '>' '}' (plus '~' on Unix-based platforms).
     *
     * @return @c 0 if the path points to a readable file on the local file system.
     */
    int             (*FileExists)(const char* path);

    /**
     * Check that the given directory exists. If it doesn't, create it.
     *
     * @return  @c true if successful.
     */
    dd_bool         (*MakePath)(const char* path);

    /**
     * @warning Not thread-safe!
     * @return  A prettier copy of the original path.
     */
    const char*     (*PrettyPath)(const char* path);

    size_t          (*ReadFile)(const char* path, char** buffer);

    /**
     * Attempt to read a file on the specified @a path into a text string.
     *
     * @param path      Path to search for the file on.
     * @param isCustom  If not @c 0, the @em is-custom status of the found file is
     *                  written here (i.e., @c true= the file was @em not loaded
     *                  from some resource of the current game).
     */
    AutoStr*        (*ReadFileIntoString)(const Str *path, dd_bool *isCustom);

    /**
     * Returns a pointer to the global WAD lump index.
     */
    void const*     (*LumpIndex)();
}
DE_API_T(F);

#ifndef DE_NO_API_MACROS_FILESYS
#define F_Access                _api_F.Access
#define F_FileExists            _api_F.FileExists
#define F_MakePath              _api_F.MakePath
#define F_PrettyPath            _api_F.PrettyPath
#define M_ReadFile              _api_F.ReadFile
#define M_ReadFileIntoString    _api_F.ReadFileIntoString
#define F_LumpIndex             _api_F.LumpIndex
#endif

#ifdef __DOOMSDAY__
DE_USING_API(F);
#endif

#endif // DOOMSDAY_API_FILE_H
