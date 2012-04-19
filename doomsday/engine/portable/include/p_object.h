/**\file p_object.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Map Objects.
 */

#ifndef LIBDENG_MAP_MOBJ
#define LIBDENG_MAP_MOBJ

#include "p_mapdata.h"

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday p_object.h from a game"
#endif

// This macro can be used to calculate a mobj-specific 'random' number.
#define MOBJ_TO_ID(mo)          ( (long)(mo)->thinker.id * 48 + ((unsigned long)(mo)/1000) )

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
DD_BASE_MOBJ_ELEMENTS()} mobj_t;

#define MOBJ_SIZE               gx.mobjSize

#define DEFAULT_FRICTION        FIX2FLT(0xe800)
#define NOMOMENTUM_THRESHOLD    (0.0001)

#define IS_SECTOR_LINKED(mo)    ((mo)->sPrev != NULL)
#define IS_BLOCK_LINKED(mo)     ((mo)->bNext != NULL)

void P_InitUnusedMobjList(void);

mobj_t* P_MobjCreate(think_t function, coord_t const post[3], angle_t angle,
    coord_t radius, coord_t height, int ddflags);
mobj_t* P_MobjCreateXYZ(think_t function, coord_t x, coord_t y, coord_t z, angle_t angle,
    coord_t radius, coord_t height, int ddflags);

void P_MobjDestroy(mobj_t* mobj);
void P_MobjRecycle(mobj_t* mobj);
void P_MobjSetState(mobj_t* mobj, int statenum);

/**
 * Sets a mobj's position.
 *
 * @return  @c true if successful, @c false otherwise. The object's position is
 *          not changed if the move fails.
 *
 * @note Internal to the engine.
 */
boolean P_MobjSetOrigin(mobj_t* mobj, coord_t x, coord_t y, coord_t z);

/**
 * Calculate the visible @a origin of @a mobj in world space, including
 * any short range offset.
 */
void Mobj_OriginSmoothed(mobj_t* mobj, coord_t origin[3]);

angle_t Mobj_AngleSmoothed(mobj_t* mobj);

coord_t Mobj_ApproxPointDistance(mobj_t* start, coord_t const* point);

#endif /// LIBDENG_MAP_MOBJ
