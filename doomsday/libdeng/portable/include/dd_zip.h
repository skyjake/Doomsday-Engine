/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/*
 * dd_zip.h: Zip/Pk3 Files
 */

#ifndef __DOOMSDAY_ZIP_PACKAGE_H__
#define __DOOMSDAY_ZIP_PACKAGE_H__

#include "sys_file.h"

// Zip entry indices are invalidated when a new Zip file is read.
typedef uint    zipindex_t;

void            Zip_Init(void);
void            Zip_Shutdown(void);

boolean         Zip_Open(const char *fileName, DFILE *prevOpened);
zipindex_t      Zip_Find(const char *fileName);
zipindex_t      Zip_Iterate(int (*iterator) (const char*, void*),
                            void *parm);
size_t          Zip_GetSize(zipindex_t index);
size_t          Zip_Read(zipindex_t index, void *buffer);
uint            Zip_GetLastModified(zipindex_t index);

#endif
