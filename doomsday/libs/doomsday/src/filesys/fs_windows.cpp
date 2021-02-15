/** @file fs_windows.cpp  Windows-specific file system operations.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#if defined (_MSC_VER) || defined (__MINGW32__)

#include "doomsday/filesys/fs_windows.h"
#include <stdio.h>
#include <de/string.h>

using namespace de;

static inline const wchar_t *pwstr(const Block &utf16)
{
    return reinterpret_cast<const wchar_t *>(utf16.data());
}

FILE *FS_Win32_fopen(const char *filenameUtf8, const char *mode)
{
    return _wfopen(pwstr(String(filenameUtf8).toUtf16()),
                   pwstr(String(mode).toUtf16()));
}

int FS_Win32_access(const char *pathUtf8, int mode)
{
    return _waccess(pwstr(String(pathUtf8).toUtf16()), mode);
}

int FS_Win32_mkdir(const char *dirnameUtf8)
{
    return _wmkdir(pwstr(String(dirnameUtf8).toUtf16()));
}

#endif // defined (_MSC_VER)
