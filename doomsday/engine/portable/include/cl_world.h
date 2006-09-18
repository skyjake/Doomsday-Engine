/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * cl_world.h: Clientside World Management
 */

#ifndef __DOOMSDAY_CLIENT_WORLD_H__
#define __DOOMSDAY_CLIENT_WORLD_H__

typedef enum {
    MVT_FLOOR,
    MVT_CEILING
} clmovertype_t;

void            Cl_InitTranslations(void);
void            Cl_InitMovers(void);
void            Cl_RemoveMovers(void);
short           Cl_TranslateLump(short lump);
void            Cl_SetPolyMover(int number, int move, int rotate);
void            Cl_AddMover(int sectornum, clmovertype_t type, fixed_t dest, 
                            fixed_t speed);

int             Cl_ReadSectorDelta(void);
int             Cl_ReadLumpDelta(void);
int             Cl_ReadSideDelta(void);
int             Cl_ReadPolyDelta(void);

void            Cl_ReadSectorDelta2(int deltaType, boolean skip);
void            Cl_ReadSideDelta2(int deltaType, boolean skip);
void            Cl_ReadPolyDelta2(boolean skip);

#endif
