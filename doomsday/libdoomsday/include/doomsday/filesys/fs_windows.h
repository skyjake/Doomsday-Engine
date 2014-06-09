/** @file fs_windows.h  Windows-specific file system operations.
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

#ifndef LIBDOOMSDAY_FS_WINDOWS_H
#define LIBDOOMSDAY_FS_WINDOWS_H

#include "../libdoomsday.h"
#include <stdio.h>

#define fopen  FS_Win32_fopen
#define access FS_Win32_access
#define mkdir  FS_Win32_mkdir

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC FILE *FS_Win32_fopen(char const *filenameUtf8, char const *mode);
LIBDOOMSDAY_PUBLIC int FS_Win32_access(char const *pathUtf8, int mode);
LIBDOOMSDAY_PUBLIC int FS_Win32_mkdir(char const *dirnameUtf8);

#ifdef __cplusplus
} //extern "C"
#endif

#endif // LIBDOOMSDAY_FS_WINDOWS_H
