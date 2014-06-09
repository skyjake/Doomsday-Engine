/** @file fs_windows.cpp  Windows-specific file system operations.
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

#include "doomsday/filesys/fs_windows.h"
#include <stdio.h>
#include <de/String>

using namespace de;

FILE *FS_Win32_fopen(char const *filenameUtf8, char const *mode)
{
    return _wfopen(String(filenameUtf8).toStdWString().c_str(),
                   String(mode).toStdWString().c_str());
}

int FS_Win32_access(char const *pathUtf8, int mode)
{
    return _waccess(String(pathUtf8).toStdWString().c_str(), mode);
}

int FS_Win32_mkdir(char const *dirnameUtf8)
{
    return _wmkdir(String(dirnameUtf8).toStdWString().c_str());
}
