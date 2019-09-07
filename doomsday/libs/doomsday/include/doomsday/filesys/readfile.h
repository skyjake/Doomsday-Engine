/** @file readfile.cpp  Legacy file reading utility routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_FILESYS_READFILE_H
#define LIBDOOMSDAY_FILESYS_READFILE_H

#include <de/legacy/str.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read a file into a buffer allocated using M_Malloc().
 */
DE_PUBLIC size_t M_ReadFile(const char *name, char **buffer);

DE_PUBLIC AutoStr *M_ReadFileIntoString(const ddstring_t *path, dd_bool *isCustom);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_FILESYS_READFILE_H
