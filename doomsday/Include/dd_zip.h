/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_zip.h: Zip/Pk3 Files
 */

#ifndef __DOOMSDAY_ZIP_PACKAGE_H__
#define __DOOMSDAY_ZIP_PACKAGE_H__

#include "sys_file.h"

// Zip entry indices are invalidated when a new Zip file is read.
typedef int zipindex_t;

void		Zip_Init(void);
void		Zip_Shutdown(void);

boolean		Zip_Open(const char *fileName, DFILE *prevOpened);
zipindex_t	Zip_Find(const char *fileName);
zipindex_t	Zip_Iterate(int (*iterator)(const char*, void*), void *parm);
uint		Zip_GetSize(zipindex_t index);
uint		Zip_Read(zipindex_t index, void *buffer);

#endif 
