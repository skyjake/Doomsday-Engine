// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log$
// Revision 1.1  2003/02/26 19:21:57  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	Movement/collision utility functions,
//	as used by function in p_map.c. 
//	BLOCKMAP Iterator functions,
//	and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id$";


#include <stdlib.h>


#include "m_bbox.h"

#include "doomdef.h"
#include "p_local.h"


// State.
#include "r_state.h"


//
// THING POSITION SETTING
//

//===========================================================================
// P_UnsetThingPosition
//	Unlinks a thing from block map and sectors.
//	On each position change, BLOCKMAP and other
//	lookups maintaining lists ot things inside
//	these structures need to be updated.
//===========================================================================
void P_UnsetThingPosition (mobj_t* thing)
{
	P_UnlinkThing(thing);
}

//===========================================================================
// P_SetThingPosition
//	Links a thing into both a block and a subsector based on it's x,y.
//	Sets thing->subsector properly.
//===========================================================================
void P_SetThingPosition(mobj_t* thing)
{
	P_LinkThing(thing, 
		(!(thing->flags & MF_NOSECTOR)? DDLINK_SECTOR : 0) |
		(!(thing->flags & MF_NOBLOCKMAP)? DDLINK_BLOCKMAP : 0));
}

//
// killough $dropoff_fix
//
// Apply "torque" to objects hanging off of ledges, so that they
// fall off. It's not really torque, since Doom has no concept of
// rotation, but it's a convincing effect which avoids anomalies
// such as lifeless objects hanging more than halfway off of ledges,
// and allows objects to roll off of the edges of moving lifts, or
// to slide up and then back down stairs, or to fall into a ditch.
// If more than one linedef is contacted, the effects are cumulative,
// so balancing is possible.
//

static mobj_t    *tmthing;
//extern fixed_t tmbbox[4];

//===========================================================================
// PIT_ApplyTorque
//	Modified by jk @ 5/29/02
//===========================================================================
static boolean PIT_ApplyTorque(line_t *ld, void *data)
{
	mobj_t *mo = tmthing;
	fixed_t dist;

	if(tmthing->player) return true; // skip players!
	
	/*if (ld->backsector &&       // If thing touches two-sided pivot linedef
		tmbbox[BOXRIGHT]  > ld->bbox[BOXLEFT]  &&
		tmbbox[BOXLEFT]   < ld->bbox[BOXRIGHT] &&
		tmbbox[BOXTOP]    > ld->bbox[BOXBOTTOM] &&
		tmbbox[BOXBOTTOM] < ld->bbox[BOXTOP] &&
		P_BoxOnLineSide(tmbbox, ld) == -1)
    {*/

	dist =						// lever arm
		+ (ld->dx >> FRACBITS) * (mo->y >> FRACBITS)
		- (ld->dy >> FRACBITS) * (mo->x >> FRACBITS) 
		- (ld->dx >> FRACBITS) * (ld->v1->y >> FRACBITS)
		+ (ld->dy >> FRACBITS) * (ld->v1->x >> FRACBITS);
		
	if (dist < 0 ?                               // drop off direction
		ld->frontsector->floorheight < mo->z &&
		ld->backsector->floorheight >= mo->z :
		ld->backsector->floorheight < mo->z &&
		ld->frontsector->floorheight >= mo->z)
	{
		// At this point, we know that the object straddles a two-sided
		// linedef, and that the object's center of mass is above-ground.
			
		fixed_t x = abs(ld->dx), y = abs(ld->dy);
		
		if (y > x)
		{
			fixed_t t = x;
			x = y;
			y = t;
		}
		
		y = finesine[(tantoangle[FixedDiv(y,x)>>DBITS] +
			ANG90) >> ANGLETOFINESHIFT];
		
		// Momentum is proportional to distance between the
		// object's center of mass and the pivot linedef.
		//
		// It is scaled by 2^(OVERDRIVE - gear). When gear is
		// increased, the momentum gradually decreases to 0 for
		// the same amount of pseudotorque, so that oscillations
		// are prevented, yet it has a chance to reach equilibrium.
		
		dist = FixedDiv(FixedMul(dist, (mo->gear < OVERDRIVE) ?
			y << -(mo->gear - OVERDRIVE) :
		y >> +(mo->gear - OVERDRIVE)), x);
		
		// Apply momentum away from the pivot linedef.
		
		x = FixedMul(ld->dy, dist);
		y = FixedMul(ld->dx, dist);
		
		// Avoid moving too fast all of a sudden (step into "overdrive")
		
		dist = FixedMul(x,x) + FixedMul(y,y);
		
		while (dist > FRACUNIT*4 && mo->gear < MAXGEAR)
			++mo->gear, x >>= 1, y >>= 1, dist >>= 1;
		
		mo->momx -= x;
		mo->momy += y;
	}
	return true;
}

//===========================================================================
// P_ApplyTorque
//	killough $dropoff_fix
//	Applies "torque" to objects, based on all contacted linedefs.
//	Modified by jk @ 5/29/02
//===========================================================================
void P_ApplyTorque(mobj_t *mo)
{
	int flags = mo->intflags; //Remember the current state, for gear-change
	
	tmthing = mo;
	validcount++; // prevents checking same line twice
	
	P_ThingLinesIterator(mo, PIT_ApplyTorque, 0);
		
	// If any momentum, mark object as 'falling' using engine-internal flags
	if (mo->momx | mo->momy)
		mo->intflags |= MIF_FALLING;
	else  // Clear the engine-internal flag indicating falling object.
		mo->intflags &= ~MIF_FALLING;
		
	// If the object has been moving, step up the gear.
	// This helps reach equilibrium and avoid oscillations.
	//
	// Doom has no concept of potential energy, much less
	// of rotation, so we have to creatively simulate these 
	// systems somehow :)
	
	if (!((mo->intflags | flags) & MIF_FALLING))	// If not falling for a while,
		mo->gear = 0;								// Reset it to full strength
	else
		if (mo->gear < MAXGEAR)						// Else if not at max gear,
			mo->gear++;								// move up a gear
}



