/** @file api_filesys.h Public API of the file system.
 * Primary header file for the Doomsday Engine Public API
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/str.h>
#include "api_base.h"

DENG_API_TYPEDEF(F)
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
     * @return  The time when the file/directory was last modified, as seconds since
     *          the Epoch else zero if @a path is not found.
     *
     * @attention This only works on native paths.
     */
    unsigned int    (*GetLastModified)(const char* path);

    /**
     * Check that the given directory exists. If it doesn't, create it.
     *
     * @return  @c true if successful.
     */
    boolean         (*MakePath)(const char* path);

    void            (*FileName)(Str* dst, const char* src);
    void            (*ExtractFileBase)(char* dst, const char* path, size_t len);
    const char*     (*FindFileExtension)(const char* path);
    boolean         (*TranslatePath)(ddstring_t* dst, const Str* src);

    /**
     * @warning Not thread-safe!
     * @return  A prettier copy of the original path.
     */
    const char*     (*PrettyPath)(const char* path);

    size_t          (*ReadFile)(const char* path, char** buffer);
    boolean         (*WriteFile)(const char* path, const char* source, size_t length);
}
DENG_API_T(F);

#ifndef DENG_NO_API_MACROS_FILESYS
#define F_Access                _api_F.Access
#define F_FileExists            _api_F.FileExists
#define F_GetLastModified       _api_F.GetLastModified
#define F_MakePath              _api_F.MakePath
#define F_FileName              _api_F.FileName
#define F_ExtractFileBase       _api_F.ExtractFileBase
#define F_FindFileExtension     _api_F.FindFileExtension
#define F_TranslatePath         _api_F.TranslatePath
#define F_PrettyPath            _api_F.PrettyPath
#define M_ReadFile              _api_F.ReadFile
#define M_WriteFile             _api_F.WriteFile
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(F);
#endif

#endif // DOOMSDAY_API_FILE_H
