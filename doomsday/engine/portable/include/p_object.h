/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_object.h: Map Objects
 */

#ifndef __DOOMSDAY_MOBJ_H__
#define __DOOMSDAY_MOBJ_H__

#include "p_mapdata.h"

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday p_object.h from a game"
#endif

// This macro can be used to calculate a mobj-specific 'random' number.
#define MOBJ_TO_ID(mo) ( (long)(mo)->thinker.id * 48 + ((unsigned long)(mo)/1000) )

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
DD_BASE_MOBJ_ELEMENTS()} mobj_t;

#define MOBJ_SIZE           gx.mobjSize

#define DEFAULT_FRICTION    FIX2FLT(0xe800)
#define NOMOMENTUM_THRESHOLD    (0.000001f)

//extern float    tmpFloorZ, tmpCeilingZ;
//extern mobj_t  *blockingMobj;
//extern boolean  dontHitMobjs;

#include "cl_def.h"                // for clplayerstate_s

void            P_InitUnusedMobjList(void);

mobj_t         *P_MobjCreate(think_t function, float x, float y, float z,
                             angle_t angle, float radius, float height,
                             int ddflags);
void            P_MobjDestroy(mobj_t *mo);
void            P_MobjRecycle(mobj_t *mo);
void            P_MobjSetState(mobj_t *mo, int statenum);

boolean         P_MobjSetPos(struct mobj_s* mo, float x, float y, float z);

#endif
