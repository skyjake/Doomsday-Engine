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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * p_mobj.h: Map Objects
 */

#ifndef __DOOMSDAY_MOBJ_H__
#define __DOOMSDAY_MOBJ_H__

#define DEFAULT_FRICTION	0xe800

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
	// If your compiler doesn't support this, you could convert 
	// ddmobj_base_s into a #define.
	struct ddmobj_base_s;
} mobj_t;

extern int tmfloorz, tmceilingz;
extern mobj_t *blockingMobj;
extern boolean dontHitMobjs;

void P_SetState(mobj_t *mo, int statenum);
void P_XYMovement(mobj_t* mo);
void P_XYMovement2(mobj_t* mo, struct playerstate_s *playmove);
void P_ZMovement(mobj_t* mo);
boolean P_TryMove(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_StepMove(mobj_t *thing, fixed_t dx, fixed_t dy, fixed_t dz);
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
boolean P_CheckPosition2(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_ChangeSector(sector_t *sector);

#endif