/** @file api_filesys.h
 * Primary header file for the Doomsday Engine Public API
 *
 * @todo Break this header file up into group-specific ones.
 * Including doomsday.h should include all of the public API headers.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
    int             (*FileExists)(const char* path);
    unsigned int    (*GetLastModified)(const char* path);
    boolean         (*MakePath)(const char* path);

    void            (*FileName)(Str* dst, const char* src);
    void            (*ExtractFileBase)(char* dst, const char* path, size_t len);
    const char*     (*FindFileExtension)(const char* path);
    boolean         (*TranslatePath)(ddstring_t* dst, const Str* src);
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
