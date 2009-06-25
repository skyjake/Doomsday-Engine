/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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

/**
 * sys_findfile.h: Win32-Style File Finding
 */

#ifndef __DOOMSDAY_FILE_FIND_H__
#define __DOOMSDAY_FILE_FIND_H__

// File attributes.
#define A_SUBDIR    0x1
#define A_RDONLY    0x2
#define A_HIDDEN    0x4
#define A_ARCH      0x8

typedef struct finddata_s {
    void           *finddata;
    long            date;
    long            time;
    long            size;
    char           *name;
    long            attrib;
} finddata_t;

/*
 * The functions return zero if successful.
 */

int             myfindfirst(const char *filename, finddata_t * dta);
int             myfindnext(finddata_t * dta);
void            myfindend(finddata_t * dta);

#endif
