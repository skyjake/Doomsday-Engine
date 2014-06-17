/** @file api_filesys.cpp  Public FS1 API.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>

#define DENG_NO_API_MACROS_FILESYS
#include "api_filesys.h"

/*
extern int F_FileExists(char const *path);
extern uint F_GetLastModified(char const *path);
extern dd_bool F_MakePath(char const *path);
extern void F_FileName(ddstring_t *dst, char const *src);
extern const char* F_PrettyPath(char const *path);
*/

// m_misc.c
DENG_EXTERN_C size_t M_ReadFile(char const *name, char **buffer);
DENG_EXTERN_C AutoStr* M_ReadFileIntoString(ddstring_t const *path, dd_bool *isCustom);
DENG_EXTERN_C dd_bool M_WriteFile(char const *name, char const *source, size_t length);

DENG_DECLARE_API(F) =
{
    { DE_API_FILE_SYSTEM },

    F_Access,
    F_FileExists,
    F_GetLastModified,
    F_MakePath,
    F_FileName,
    F_PrettyPath,
    M_ReadFile,
    M_ReadFileIntoString,
    M_WriteFile
};
