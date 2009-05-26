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

/**
 * r_extres.h: External Resources
 */

#ifndef __DOOMSDAY_REFRESH_EXT_RES_H__
#define __DOOMSDAY_REFRESH_EXT_RES_H__

void            R_InitResourceLocator(void);
void            R_ShutdownResourceLocator(void);

void            R_SetDataPath(const char* path);
const char*     R_GetDataPath(void);
void            R_PrependDataPath(const char* origPath, char* newPath);

void            R_AddClassDataPath(resourceclass_t resClass,
                                   const char* addPath, boolean append);
void            R_ClearClassDataPath(resourceclass_t resClass);
const char*     R_GetClassDataPath(resourceclass_t resClass);

boolean         R_FindResource(resourcetype_t resType, const char* name,
                               const char* optionalSuffix, char* fileName);
boolean         R_FindResource2(resourcetype_t resType,
                               resourceclass_t resClass, const char* name,
                               const char* optionalSuffix, char* fileName);
#endif
