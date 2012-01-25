/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "dd_types.h"

/**
 * Win32-Style File Finding.
 */

#ifndef LIBDENG_FILESYS_FILEFINDER_H
#define LIBDENG_FILESYS_FILEFINDER_H

// File attributes.
#define A_SUBDIR                0x1
#define A_RDONLY                0x2
#define A_HIDDEN                0x4
#define A_ARCH                  0x8

typedef struct finddata_s {
    void* finddata;
#if defined(UNIX)
    long date;
    long time;
    long size;
#else
    time_t date;
    time_t time;
    size_t size;
#endif
    ddstring_t name;
    long attrib;
} finddata_t;

/**
 * @return  @c 0, if successful(!).
 */
int myfindfirst(const char* filename, finddata_t* findData);

/**
 * @return  @c 0, if successful(!).
 */
int myfindnext(finddata_t* findData);

/**
 * @return  @c 0, if successful(!).
 */
void myfindend(finddata_t* findData);

#endif /* LIBDENG_FILESYS_FILEFINDER_H */
