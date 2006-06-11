/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * p_mobj.h: Map Objects
 */

#ifndef __DOOMSDAY_MOBJ_H__
#define __DOOMSDAY_MOBJ_H__

#include "p_data.h"

// This macro can be used to calculate a thing-specific 'random' number.
#define THING_TO_ID(mo) ( (mo)->thinker.id * 48 + ((unsigned)(mo)/1000) )

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
DD_BASE_MOBJ_ELEMENTS()} mobj_t;

#define DEFAULT_FRICTION    0xe800

extern int      tmfloorz, tmceilingz;
extern mobj_t  *blockingMobj;
extern boolean  dontHitMobjs;

#include "cl_def.h"                // for playerstate_s

void            P_SetState(mobj_t *mo, int statenum);
void            P_ThingMovement(mobj_t *mo);
void            P_ThingMovement2(mobj_t *mo, void *pstate);
void            P_ThingZMovement(mobj_t *mo);
boolean         P_TryMoveXYZ(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z);
boolean         P_StepMove(mobj_t *thing, fixed_t dx, fixed_t dy, fixed_t dz);
boolean         P_CheckPosXY(mobj_t *thing, fixed_t x, fixed_t y);
boolean         P_CheckPosXYZ(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z);
boolean         P_SectorPlanesChanged(sector_t *sector);

boolean         P_IsInVoid(ddplayer_t* p);
#endif
