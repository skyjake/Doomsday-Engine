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
// Revision 1.10  2004/06/16 18:28:47  skyjake
// Updated style (typenames)
//
// Revision 1.9  2004/06/16 18:25:09  skyjake
// Added a separate killmsg for telestomp
//
// Revision 1.8  2004/05/30 08:42:41  skyjake
// Tweaked indentation style
//
// Revision 1.7  2004/05/29 09:53:29  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.6  2004/05/28 19:52:58  skyjake
// Finished switch from branch-1-7 to trunk, hopefully everything is fine
//
// Revision 1.3.2.3  2004/05/16 10:01:36  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.3.2.2.2.2  2003/11/22 18:09:10  skyjake
// Cleanup
//
// Revision 1.3.2.2.2.1  2003/11/19 17:07:13  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.3.2.2  2003/09/19 19:29:52  skyjake
// Fixed hang when lineattack dz==zero
//
// Revision 1.3.2.1  2003/09/07 22:22:30  skyjake
// Fixed bullet puff bugs: handling empty sectors and large deltas
//
// Revision 1.3  2003/03/14 21:22:54  skyjake
// Lineattack finds approx. plane hitpoint
//
// Revision 1.2  2003/02/27 23:14:32  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:21:55  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

static const char
        rcsid[] = "$Id$";

#include <stdlib.h>
#include <math.h>

#include "m_random.h"

#include "doomdef.h"
#include "d_config.h"
#include "p_local.h"
#include "g_common.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

fixed_t tmbbox[4];
mobj_t *tmthing;
int     tmflags;
fixed_t tmx, tmy, tmz, tmheight;
line_t *tmhitline;

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
fixed_t tmdropoffz;

// killough $dropoff_fix
boolean felldown;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t *ceilingline;

// Used to prevent player getting stuck in monster
// Based on solution derived by Lee Killough
// See also floorline and static int untouched
static int tmunstuck;			// $unstuck: used to check unsticking
line_t *floorline;				// $unstuck: Highest touched floor
line_t *blockline;				// $unstuck: blocking linedef

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
line_t *spechit[MAXSPECIALCROSS];
int     numspechit;

//
// TELEPORT MOVE
// 

//
// PIT_StompThing
//
boolean PIT_StompThing(mobj_t *thing, void *data)
{
	fixed_t blockdist;

	if(!(thing->flags & MF_SHOOTABLE))
		return true;

	blockdist = thing->radius + tmthing->radius;

	if(abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
	{
		// didn't hit it
		return true;
	}

	// don't clip against self
	if(thing == tmthing)
		return true;

	// monsters don't stomp things except on boss level
	if(!tmthing->player && gamemap != 30)
		return false;

	// Do stomp damage.
	P_DamageMobj2(thing, tmthing, tmthing, 10000, true);

	return true;
}

//===========================================================================
// P_TeleportMove
//  Also stomps on things.
//===========================================================================
boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y)
{
	int     xl;
	int     xh;
	int     yl;
	int     yh;
	int     bx;
	int     by;

	subsector_t *newsubsec;

	// kill anything occupying the position
	tmthing = thing;
	tmflags = thing->flags;

	tmx = x;
	tmy = y;

	tmbbox[BOXTOP] = y + tmthing->radius;
	tmbbox[BOXBOTTOM] = y - tmthing->radius;
	tmbbox[BOXRIGHT] = x + tmthing->radius;
	tmbbox[BOXLEFT] = x - tmthing->radius;

	newsubsec = R_PointInSubsector(x, y);

	blockline = floorline = ceilingline = NULL;	// $unstuck: floorline used with tmunstuck

	// $unstuck
	tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

	// The base floor/ceiling is from the subsector
	// that contains the point.
	// Any contacted lines the step closer together
	// will adjust them.
	tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
	tmceilingz = newsubsec->sector->ceilingheight;

	validCount++;
	numspechit = 0;

	// stomp on any things contacted
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

	for(bx = xl; bx <= xh; bx++)
		for(by = yl; by <= yh; by++)
			if(!P_BlockThingsIterator(bx, by, PIT_StompThing, 0))
				return false;

	// the move is ok,
	// so link the thing into its new position
	P_UnsetThingPosition(thing);

	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	thing->dropoffz = tmdropoffz;	// killough $unstuck
	thing->x = x;
	thing->y = y;

	P_SetThingPosition(thing);
	P_ClearThingSRVO(thing);

	return true;
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

// $unstuck: used to test intersection between thing and line
// assuming NO movement occurs -- used to avoid sticky situations.

static int untouched(line_t *ld)
{
	fixed_t x, y, tmbbox[4];

	return (tmbbox[BOXRIGHT] =
			(x = tmthing->x) + tmthing->radius) <= ld->bbox[BOXLEFT] ||
		(tmbbox[BOXLEFT] = x - tmthing->radius) >= ld->bbox[BOXRIGHT] ||
		(tmbbox[BOXTOP] =
		 (y = tmthing->y) + tmthing->radius) <= ld->bbox[BOXBOTTOM] ||
		(tmbbox[BOXBOTTOM] = y - tmthing->radius) >= ld->bbox[BOXTOP] ||
		P_BoxOnLineSide(tmbbox, ld) != -1;
}

//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
boolean PIT_CheckLine(line_t *ld, void *data)
{
	if(tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT] ||
	   tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT] ||
	   tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM] ||
	   tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
		return true;

	if(P_BoxOnLineSide(tmbbox, ld) != -1)
		return true;

	// A line has been hit
	tmthing->wallhit = true;

	// A Hit event will be sent to special lines.
	if(ld->special)
		tmhitline = ld;

	// The moving thing's destination position will cross
	// the given line.
	// If this should not be allowed, return false.
	// If the line is special, keep track of it
	// to process later if the move is proven ok.
	// NOTE: specials are NOT sorted by order,
	// so two special lines that are only 8 pixels apart
	// could be crossed in either order.

	// $unstuck: allow player to move out of 1s wall, to prevent sticking
	if(!ld->backsector)			// one sided line
	{
		blockline = ld;
		return tmunstuck && !untouched(ld) &&
			FixedMul(tmx - tmthing->x, ld->dy) > FixedMul(tmy - tmthing->y,
														  ld->dx);
	}

	if(!(tmthing->flags & MF_MISSILE))
	{
		if(ld->flags & ML_BLOCKING)	// explicitly blocking everything
			return tmunstuck && !untouched(ld);	// killough $unstuck: allow escape
		/* return false; */

		if(!tmthing->player && ld->flags & ML_BLOCKMONSTERS)
			return false;		// block monsters only
	}

	// set openrange, opentop, openbottom
	P_LineOpening(ld);

	// adjust floor / ceiling heights
	if(opentop < tmceilingz)
	{
		tmceilingz = opentop;
		ceilingline = ld;
		blockline = ld;
	}

	if(openbottom > tmfloorz)
	{
		tmfloorz = openbottom;
		floorline = ld;			// killough $unstuck: remember floor linedef
		blockline = ld;
	}
	/*
	   if (openbottom > tmfloorz)
	   tmfloorz = openbottom;   */

	if(lowfloor < tmdropoffz)
		tmdropoffz = lowfloor;

	// if contacted a special line, add it to the list
	if(ld->special)
	{
		spechit[numspechit] = ld;
		numspechit++;
	}
	tmthing->wallhit = false;
	return true;
}

//===========================================================================
// PIT_CheckThing
//===========================================================================
boolean PIT_CheckThing(mobj_t *thing, void *data)
{
	fixed_t blockdist;
	boolean solid, overlap = false;
	int     damage;

	// don't clip against self
	if(thing == tmthing)
		return true;

	if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) || P_IsCamera(thing) || P_IsCamera(tmthing))	// $democam
		return true;

	blockdist = thing->radius + tmthing->radius;

	if(tmthing->player && tmz != DDMAXINT && cfg.moveCheckZ)
	{
		if(thing->z > tmz + tmheight || thing->z + thing->height < tmz)
			return true;		// under or over it
		overlap = true;
	}
	if(abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
	{
		// didn't hit it
		return true;
	}

	// check for skulls slamming into things
	if(tmthing->flags & MF_SKULLFLY)
	{
		damage = ((P_Random() % 8) + 1) * tmthing->info->damage;

		P_DamageMobj(thing, tmthing, tmthing, damage);

		tmthing->flags &= ~MF_SKULLFLY;
		tmthing->momx = tmthing->momy = tmthing->momz = 0;

		P_SetMobjState(tmthing, tmthing->info->spawnstate);

		return false;			// stop moving
	}

	// missiles can hit other things
	if(tmthing->flags & MF_MISSILE)
	{
		// see if it went over / under
		if(tmthing->z > thing->z + thing->height)
			return true;		// overhead
		if(tmthing->z + tmthing->height < thing->z)
			return true;		// underneath

		// Don't hit same species as originator.
		if(tmthing->target &&
		   (tmthing->target->type == thing->type ||
			(tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER)
			|| (tmthing->target->type == MT_BRUISER &&
				thing->type == MT_KNIGHT)))
		{
			if(thing == tmthing->target)
				return true;

			if(!monsterinfight && thing->type != MT_PLAYER)	// $infight
			{
				// Explode, but do no damage.
				// Let players missile other players.
				return false;
			}
		}

		if(!(thing->flags & MF_SHOOTABLE))
		{
			// didn't do any damage
			return !(thing->flags & MF_SOLID);
		}

		// damage / explode
		damage = ((P_Random() % 8) + 1) * tmthing->info->damage;
		P_DamageMobj(thing, tmthing, tmthing->target, damage);

		// don't traverse any more
		return false;
	}

	// check for special pickup
	if(thing->flags & MF_SPECIAL)
	{
		solid = thing->flags & MF_SOLID;
		if(tmflags & MF_PICKUP)
		{
			// can remove thing
			P_TouchSpecialThing(thing, tmthing);
		}
		return !solid;
	}

	if(overlap && thing->flags & MF_SOLID)
	{
		// How are we positioned?
		if(tmz > thing->z + thing->height - 24 * FRACUNIT)
		{
			tmthing->onmobj = thing;
			if(thing->z + thing->height > tmfloorz)
				tmfloorz = thing->z + thing->height;
			return true;
		}
	}

	return !(thing->flags & MF_SOLID);
}

//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
// 
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a drop off)
//  speciallines[]
//  numspeciallines
//

boolean P_CheckPosition2(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z)
{
	int     xl;
	int     xh;
	int     yl;
	int     yh;
	int     bx;
	int     by;
	subsector_t *newsubsec;

	tmthing = thing;
	tmflags = thing->flags;

	thing->onmobj = NULL;
	thing->wallhit = false;

	tmhitline = NULL;

	tmx = x;
	tmy = y;
	tmz = z;
	tmheight = thing->height;

	tmbbox[BOXTOP] = y + tmthing->radius;
	tmbbox[BOXBOTTOM] = y - tmthing->radius;
	tmbbox[BOXRIGHT] = x + tmthing->radius;
	tmbbox[BOXLEFT] = x - tmthing->radius;

	newsubsec = R_PointInSubsector(x, y);
	blockline = floorline = ceilingline = NULL;	// $unstuck: floorline used with tmunstuck

	// $unstuck
	tmunstuck = thing->dplayer && thing->dplayer->mo == thing;
	// The base floor / ceiling is from the subsector
	// that contains the point. Any contacted lines the
	// step closer together will adjust them.
	tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
	tmceilingz = newsubsec->sector->ceilingheight;

	validCount++;
	numspechit = 0;

	if(tmflags & MF_NOCLIP)
		return true;

	// Check things first, possibly picking things up.
	// The bounding box is extended by MAXRADIUS
	// because mobj_ts are grouped into mapblocks
	// based on their origin point, and can overlap
	// into adjacent blocks by up to MAXRADIUS units.
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

	for(bx = xl; bx <= xh; bx++)
		for(by = yl; by <= yh; by++)
			if(!P_BlockThingsIterator(bx, by, PIT_CheckThing, 0))
				return false;

	// check lines
	xl = (tmbbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

	for(bx = xl; bx <= xh; bx++)
		for(by = yl; by <= yh; by++)
			if(!P_BlockLinesIterator(bx, by, PIT_CheckLine, 0))
				return false;

	return true;
}

boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y)
{
	return P_CheckPosition2(thing, x, y, DDMAXINT);
}

//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
// killough $dropoff_fix
boolean P_TryMove2(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
{
	fixed_t oldx;
	fixed_t oldy;
	int     side;
	int     oldside;
	line_t *ld;

	floatok = felldown = false;	// $dropoff_fix: felldown

	//if (!P_CheckPosition (thing, x, y))
	if(!P_CheckPosition2(thing, x, y, thing->z))
	{
		if(!thing->onmobj || thing->wallhit)
			return false;		// solid wall or thing
	}

	// GMJ 02/02/02
	if(!(thing->flags & MF_NOCLIP))
	{
		// killough 7/26/98: reformatted slightly
		// killough 8/1/98: Possibly allow escape if otherwise stuck

		if(tmceilingz - tmfloorz < thing->height ||	// doesn't fit
		   // mobj must lower to fit
		   (floatok = true, !(thing->flags & MF_TELEPORT) &&
			tmceilingz - thing->z < thing->height) ||
		   // too big a step up
		   (!(thing->flags & MF_TELEPORT) &&
			tmfloorz - thing->z > 24 * FRACUNIT))
		{
			return tmunstuck && !(ceilingline && untouched(ceilingline)) &&
				!(floorline && untouched(floorline));
		}

		// killough 3/15/98: Allow certain objects to drop off
		// killough 7/24/98, 8/1/98: 
		// Prevent monsters from getting stuck hanging off ledges
		// killough 10/98: Allow dropoffs in controlled circumstances
		// killough 11/98: Improve symmetry of clipping on stairs

		if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
		{
			// Dropoff height limit
			if(!dropoff && tmfloorz - tmdropoffz > 24 * FRACUNIT)
				return false;
			else
			{
				// set felldown if drop > 24
				felldown = !(thing->flags & MF_NOGRAVITY) &&
					thing->z - tmfloorz > 24 * FRACUNIT;
			}
		}

		// killough $dropoff: prevent falling objects from going up too many steps
		if(!thing->player && thing->intflags & MIF_FALLING &&
		   tmfloorz - thing->z > FixedMul(thing->momx,
										  thing->momx) + FixedMul(thing->momy,
																  thing->momy))
		{
			return false;
		}
	}

/*---OLD CODE---
	if ( !(thing->flags & MF_NOCLIP) )
	{
		// $unstuck: Possibly allow escape if otherwise stuck
		if (tmceilingz - tmfloorz < thing->height)
			goto ret_stuck_test;	// doesn't fit

		floatok = true;
		
		if ( !(thing->flags & MF_TELEPORT) 
			&& tmceilingz - thing->z < thing->height)
			goto ret_stuck_test; // mobj must lower to fit
		  
		if ( !(thing->flags & MF_TELEPORT)
			&& tmfloorz - thing->z > 24*FRACUNIT )
			// too big a step up
		{
ret_stuck_test:
			return tmunstuck 
				&& !(ceilingline && untouched(ceilingline))
				&& !(  floorline && untouched(	floorline));
		}

		if (!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
		{
			// Dropoff height limit
			if (!dropoff)
			{
				if (thing->floorz  - tmfloorz > 24*FRACUNIT
					|| thing->dropoffz - tmdropoffz > 24*FRACUNIT)
					return false;
			}
			else
				// set felldown if drop > 24
				felldown = !(thing->flags & MF_NOGRAVITY)
							&& thing->z - tmfloorz > 24*FRACUNIT;

			// killough $dropoff: prevent falling objects from going up too many steps
			if (thing->intflags & MIF_FALLING && tmfloorz - thing->z >
				FixedMul(thing->momx,thing->momx)+FixedMul(thing->momy,thing->momy))
				return false;
		}
	}
---OLD CODE---*/

	// the move is ok, so link the thing into its new position
	P_UnsetThingPosition(thing);

	oldx = thing->x;
	oldy = thing->y;
	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	thing->dropoffz = tmdropoffz;	// killough $dropoff_fix: keep track of dropoffs
	thing->x = x;
	thing->y = y;

	P_SetThingPosition(thing);

	// if any special lines were hit, do the effect
	if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
	{
		while(numspechit-- > 0)
		{
			// see if the line was crossed
			ld = spechit[numspechit];

			side = P_PointOnLineSide(thing->x, thing->y, ld);
			oldside = P_PointOnLineSide(oldx, oldy, ld);
			if(side != oldside)
			{
				if(ld->special)
					P_CrossSpecialLine(ld - lines, oldside, thing);
			}
		}
	}

	return true;
}

boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
{
	// killough $dropoff_fix
	boolean res = P_TryMove2(thing, x, y, dropoff);

	if(!res && tmhitline)
	{
		// Move not possible, see if the thing hit a line and send a Hit
		// event to it.
		XL_HitLine(tmhitline, P_PointOnLineSide(thing->x, thing->y, tmhitline),
				   thing);
	}
	return res;
}

//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//
boolean P_ThingHeightClip(mobj_t *thing)
{
	boolean onfloor;

	onfloor = (thing->z == thing->floorz);
	P_CheckPosition2(thing, thing->x, thing->y, thing->z);

	// what about stranding a monster partially off an edge?

	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	thing->dropoffz = tmdropoffz;	// killough $dropoff_fix: remember dropoffs

	if(onfloor)
	{
		// walking monsters rise and fall with the floor
		thing->z = thing->floorz;
		// killough $dropoff_fix:
		// Possibly upset balance of objects hanging off ledges
		if(thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
			thing->gear = 0;
	}
	else
	{
		// don't adjust a floating monster unless forced to
		if(thing->z + thing->height > thing->ceilingz)
			thing->z = thing->ceilingz - thing->height;
	}

	return (thing->ceilingz - thing->floorz >= thing->height);
}

//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
fixed_t bestslidefrac;
fixed_t secondslidefrac;

line_t *bestslideline;
line_t *secondslideline;

mobj_t *slidemo;

fixed_t tmxmove;
fixed_t tmymove;

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
void P_HitSlideLine(line_t *ld)
{
	int     side;

	angle_t lineangle;
	angle_t moveangle;
	angle_t deltaangle;

	fixed_t movelen;
	fixed_t newlen;

	if(ld->slopetype == ST_HORIZONTAL)
	{
		tmymove = 0;
		return;
	}

	if(ld->slopetype == ST_VERTICAL)
	{
		tmxmove = 0;
		return;
	}

	side = P_PointOnLineSide(slidemo->x, slidemo->y, ld);

	lineangle = R_PointToAngle2(0, 0, ld->dx, ld->dy);

	if(side == 1)
		lineangle += ANG180;

	moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
	deltaangle = moveangle - lineangle;

	if(deltaangle > ANG180)
		deltaangle += ANG180;
	//  Con_Error ("SlideLine: ang>ANG180");

	lineangle >>= ANGLETOFINESHIFT;
	deltaangle >>= ANGLETOFINESHIFT;

	movelen = P_ApproxDistance(tmxmove, tmymove);
	newlen = FixedMul(movelen, finecosine[deltaangle]);

	tmxmove = FixedMul(newlen, finecosine[lineangle]);
	tmymove = FixedMul(newlen, finesine[lineangle]);
}

//
// PTR_SlideTraverse
//
boolean PTR_SlideTraverse(intercept_t * in)
{
	line_t *li;

	if(!in->isaline)
		Con_Error("PTR_SlideTraverse: not a line?");

	li = in->d.line;

	if(!(li->flags & ML_TWOSIDED))
	{
		if(P_PointOnLineSide(slidemo->x, slidemo->y, li))
		{
			// don't hit the back side
			return true;
		}
		goto isblocking;
	}

	// set openrange, opentop, openbottom
	P_LineOpening(li);

	if(openrange < slidemo->height)
		goto isblocking;		// doesn't fit

	if(opentop - slidemo->z < slidemo->height)
		goto isblocking;		// mobj is too high

	if(openbottom - slidemo->z > 24 * FRACUNIT)
		goto isblocking;		// too big a step up

	// this line doesn't block movement
	return true;

	// the line does block movement,
	// see if it is closer than best so far
  isblocking:
	if(in->frac < bestslidefrac)
	{
		secondslidefrac = bestslidefrac;
		secondslideline = bestslideline;
		bestslidefrac = in->frac;
		bestslideline = li;
	}

	return false;				// stop
}

//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
void P_SlideMove(mobj_t *mo)
{
	fixed_t leadx;
	fixed_t leady;
	fixed_t trailx;
	fixed_t traily;
	fixed_t newx;
	fixed_t newy;
	int     hitcount;

	slidemo = mo;
	hitcount = 0;

  retry:
	if(++hitcount == 3)
		goto stairstep;			// don't loop forever

	// trace along the three leading corners
	if(mo->momx > 0)
	{
		leadx = mo->x + mo->radius;
		trailx = mo->x - mo->radius;
	}
	else
	{
		leadx = mo->x - mo->radius;
		trailx = mo->x + mo->radius;
	}

	if(mo->momy > 0)
	{
		leady = mo->y + mo->radius;
		traily = mo->y - mo->radius;
	}
	else
	{
		leady = mo->y - mo->radius;
		traily = mo->y + mo->radius;
	}

	bestslidefrac = FRACUNIT + 1;

	P_PathTraverse(leadx, leady, leadx + mo->momx, leady + mo->momy,
				   PT_ADDLINES, PTR_SlideTraverse);
	P_PathTraverse(trailx, leady, trailx + mo->momx, leady + mo->momy,
				   PT_ADDLINES, PTR_SlideTraverse);
	P_PathTraverse(leadx, traily, leadx + mo->momx, traily + mo->momy,
				   PT_ADDLINES, PTR_SlideTraverse);

	// move up to the wall
	if(bestslidefrac == FRACUNIT + 1)
	{
		// the move most have hit the middle, so stairstep
	  stairstep:
		// killough $dropoff_fix
		if(!P_TryMove(mo, mo->x, mo->y + mo->momy, true))
			P_TryMove(mo, mo->x + mo->momx, mo->y, true);
		return;
	}

	// fudge a bit to make sure it doesn't hit
	bestslidefrac -= 0x800;
	if(bestslidefrac > 0)
	{
		newx = FixedMul(mo->momx, bestslidefrac);
		newy = FixedMul(mo->momy, bestslidefrac);

		// killough $dropoff_fix
		if(!P_TryMove(mo, mo->x + newx, mo->y + newy, true))
			goto stairstep;
	}

	// Now continue along the wall.
	// First calculate remainder.
	bestslidefrac = FRACUNIT - (bestslidefrac + 0x800);

	if(bestslidefrac > FRACUNIT)
		bestslidefrac = FRACUNIT;

	if(bestslidefrac <= 0)
		return;

	tmxmove = FixedMul(mo->momx, bestslidefrac);
	tmymove = FixedMul(mo->momy, bestslidefrac);

	P_HitSlideLine(bestslideline);	// clip the moves

	mo->momx = tmxmove;
	mo->momy = tmymove;

	// killough $dropoff_fix
	if(!P_TryMove(mo, mo->x + tmxmove, mo->y + tmymove, true))
	{
		goto retry;
	}
}

//
// P_LineAttack
//
mobj_t *linetarget;				// who got hit (or NULL)
mobj_t *shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
fixed_t shootz;

int     la_damage;
fixed_t attackrange;

fixed_t aimslope;

// slopes to top and bottom of target
fixed_t topslope;
fixed_t bottomslope;

//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
//
boolean PTR_AimTraverse(intercept_t * in)
{
	line_t *li;
	mobj_t *th;
	fixed_t slope;
	fixed_t thingtopslope;
	fixed_t thingbottomslope;
	fixed_t dist;

	if(in->isaline)
	{
		li = in->d.line;

		if(!(li->flags & ML_TWOSIDED))
			return false;		// stop

		// Crosses a two sided line.
		// A two sided line will restrict
		// the possible target ranges.
		P_LineOpening(li);

		if(openbottom >= opentop)
			return false;		// stop

		dist = FixedMul(attackrange, in->frac);

		if(li->frontsector->floorheight != li->backsector->floorheight)
		{
			slope = FixedDiv(openbottom - shootz, dist);
			if(slope > bottomslope)
				bottomslope = slope;
		}

		if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
		{
			slope = FixedDiv(opentop - shootz, dist);
			if(slope < topslope)
				topslope = slope;
		}

		if(topslope <= bottomslope)
			return false;		// stop

		return true;			// shot continues
	}

	// shoot a thing
	th = in->d.thing;
	if(th == shootthing)
		return true;			// can't shoot self

	if(!(th->flags & MF_SHOOTABLE))
		return true;			// corpse or something

	// check angles to see if the thing can be aimed at
	dist = FixedMul(attackrange, in->frac);
	thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

	if(thingtopslope < bottomslope)
		return true;			// shot over the thing

	thingbottomslope = FixedDiv(th->z - shootz, dist);

	if(thingbottomslope > topslope)
		return true;			// shot under the thing

	// this thing can be hit!
	if(thingtopslope > topslope)
		thingtopslope = topslope;

	if(thingbottomslope < bottomslope)
		thingbottomslope = bottomslope;

	aimslope = (thingtopslope + thingbottomslope) / 2;
	linetarget = th;

	return false;				// don't go any farther
}

//===========================================================================
// PTR_ShootTraverse
//===========================================================================
boolean PTR_ShootTraverse(intercept_t * in)
{
	fixed_t x;
	fixed_t y;
	fixed_t z;
	fixed_t frac;

	line_t *li;

	mobj_t *th;

	fixed_t slope;
	fixed_t dist;
	fixed_t thingtopslope;
	fixed_t thingbottomslope;

	divline_t *trace = (divline_t *) Get(DD_TRACE_ADDRESS);

	// These were added for the plane-hitpoint algo.
	// FIXME: This routine is getting rather bloated.
	boolean lineWasHit;
	subsector_t *contact, *originSub;
	fixed_t ctop, cbottom, dx, dy, dz, step, stepx, stepy, stepz;
	int     divisor;

	if(in->isaline)
	{
		li = in->d.line;

		if(li->special)
			P_ShootSpecialLine(shootthing, li);

		if(!(li->flags & ML_TWOSIDED))
			goto hitline;

		// crosses a two sided line
		P_LineOpening(li);

		dist = FixedMul(attackrange, in->frac);

		if(li->frontsector->floorheight != li->backsector->floorheight)
		{
			slope = FixedDiv(openbottom - shootz, dist);
			if(slope > aimslope)
				goto hitline;
		}

		if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
		{
			slope = FixedDiv(opentop - shootz, dist);
			if(slope < aimslope)
				goto hitline;
		}

		// shot continues
		return true;

		// hit line
	  hitline:
		lineWasHit = true;

		// Position a bit closer.
		frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
		x = trace->x + FixedMul(trace->dx, frac);
		y = trace->y + FixedMul(trace->dy, frac);
		z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

		// Is it a sky hack wall? If the hitpoint is above the visible 
		// line, no puff must be shown.
		if(li->backsector && li->frontsector->ceilingpic == skyflatnum &&
		   li->backsector->ceilingpic == skyflatnum &&
		   (z > li->frontsector->ceilingheight ||
			z > li->backsector->ceilingheight))
			return false;

		// This is subsector where the trace originates.
		originSub = R_PointInSubsector(trace->x, trace->y);

		dx = x - trace->x;
		dy = y - trace->y;
		dz = z - shootz;

		if(dz != 0)
		{
			contact = R_PointInSubsector(x, y);
			step = P_ApproxDistance3(dx, dy, dz);
			stepx = FixedDiv(dx, step);
			stepy = FixedDiv(dy, step);
			stepz = FixedDiv(dz, step);

			// Backtrack until we find a non-empty sector.
			while(contact->sector->ceilingheight <=
				  contact->sector->floorheight && contact != originSub)
			{
				dx -= 8 * stepx;
				dy -= 8 * stepy;
				dz -= 8 * stepz;
				x = trace->x + dx;
				y = trace->y + dy;
				z = shootz + dz;
				contact = R_PointInSubsector(x, y);
			}

			// Should we backtrack to hit a plane instead?
			ctop = contact->sector->ceilingheight - 4 * FRACUNIT;
			cbottom = contact->sector->floorheight + 4 * FRACUNIT;
			divisor = 2;

			// We must not hit a sky plane.
			if((z > ctop && contact->sector->ceilingpic == skyflatnum) ||
			   (z < cbottom && contact->sector->floorpic == skyflatnum))
				return false;

			// Find the approximate hitpoint by stepping back and
			// forth using smaller and smaller steps. 
			while((z > ctop || z < cbottom) && divisor <= 128)
			{
				// We aren't going to hit a line any more.
				lineWasHit = false;

				// Take a step backwards.
				x -= dx / divisor;
				y -= dy / divisor;
				z -= dz / divisor;

				// Divisor grows.
				divisor <<= 1;

				// Move forward until limits breached.
				while((dz > 0 && z <= ctop) || (dz < 0 && z >= cbottom))
				{
					x += dx / divisor;
					y += dy / divisor;
					z += dz / divisor;
				}
			}
		}

		// Spawn bullet puffs.
		P_SpawnPuff(x, y, z);

		if(lineWasHit && li->special)
		{
			// Extended shoot events only happen when the bullet actually
			// hits the line.
			XL_ShootLine(li, 0, shootthing);
		}

		// don't go any farther
		return false;
	}

	// shoot a thing
	th = in->d.thing;
	if(th == shootthing)
		return true;			// can't shoot self

	if(!(th->flags & MF_SHOOTABLE))
		return true;			// corpse or something

	// check angles to see if the thing can be aimed at
	dist = FixedMul(attackrange, in->frac);
	thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

	if(thingtopslope < aimslope)
		return true;			// shot over the thing

	thingbottomslope = FixedDiv(th->z - shootz, dist);

	if(thingbottomslope > aimslope)
		return true;			// shot under the thing

	// hit thing
	// position a bit closer
	frac = in->frac - FixedDiv(10 * FRACUNIT, attackrange);

	x = trace->x + FixedMul(trace->dx, frac);
	y = trace->y + FixedMul(trace->dy, frac);
	z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

	// Spawn bullet puffs or blod spots,
	// depending on target type.
	if(in->d.thing->flags & MF_NOBLOOD)
		P_SpawnPuff(x, y, z);
	else
		P_SpawnBlood(x, y, z, la_damage);

	if(la_damage)
		P_DamageMobj(th, shootthing, shootthing, la_damage);

	// don't go any farther
	return false;

}

//
// P_AimLineAttack
//
fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance)
{
	fixed_t x2;
	fixed_t y2;

	angle >>= ANGLETOFINESHIFT;
	shootthing = t1;

	x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
	y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
	shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;

	/*    if(INCOMPAT_OK)
	   { */
	topslope = 60 * FRACUNIT;
	bottomslope = -topslope;
	/*  }
	   else
	   {
	   // can't shoot outside view angles 
	   topslope = 100*FRACUNIT/160; 
	   bottomslope = -100*FRACUNIT/160;
	   } */

	attackrange = distance;
	linetarget = NULL;

	P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS,
				   PTR_AimTraverse);

	if(linetarget)
	{
		if(!t1->player || !cfg.noAutoAim)	// || !INCOMPAT_OK)
			return aimslope;
	}

	if(t1->player)
	{
		// The slope is determined by lookdir.
		return FRACUNIT * (tan(LOOKDIR2RAD(t1->dplayer->lookdir)) / 1.2);
	}

	return 0;
}

//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//
void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
				  int damage)
{
	fixed_t x2;
	fixed_t y2;

	angle >>= ANGLETOFINESHIFT;
	shootthing = t1;
	la_damage = damage;
	x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
	y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
	shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
	if(t1->player)
	{
		// Players shoot at eye height.
		shootz = t1->z + (cfg.plrViewHeight - 5) * FRACUNIT;
	}
	attackrange = distance;
	aimslope = slope;

	P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS,
				   PTR_ShootTraverse);
}

//
// USE LINES
//
mobj_t *usething;

boolean PTR_UseTraverse(intercept_t * in)
{
	int     side;

	if(!in->d.line->special)
	{
		P_LineOpening(in->d.line);
		if(openrange <= 0)
		{
			S_StartSound(sfx_noway, usething);

			// can't use through a wall
			return false;
		}
		// not a special line, but keep checking
		return true;
	}

	side = 0;
	if(P_PointOnLineSide(usething->x, usething->y, in->d.line) == 1)
		side = 1;

	//  return false;       // don't use back side

	P_UseSpecialLine(usething, in->d.line, side);

	// can't use for than one special line in a row
	return false;
}

//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
void P_UseLines(player_t *player)
{
	int     angle;
	fixed_t x1;
	fixed_t y1;
	fixed_t x2;
	fixed_t y2;

	usething = player->plr->mo;

	angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

	x1 = player->plr->mo->x;
	y1 = player->plr->mo->y;
	x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
	y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];

	P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse);
}

//
// RADIUS ATTACK
//
mobj_t *bombsource;
mobj_t *bombspot;
int     bombdamage;

//
// PIT_RadiusAttack
// "bombsource" is the creature
// that caused the explosion at "bombspot".
//
boolean PIT_RadiusAttack(mobj_t *thing, void *data)
{
	fixed_t dx;
	fixed_t dy;
	fixed_t dz;
	fixed_t dist;

	if(!(thing->flags & MF_SHOOTABLE))
		return true;

	// Boss spider and cyborg
	// take no damage from concussion.
	if(thing->type == MT_CYBORG || thing->type == MT_SPIDER)
		return true;

	dx = abs(thing->x - bombspot->x);
	dy = abs(thing->y - bombspot->y);
	dz = abs(thing->z - bombspot->z);

	dist = dx > dy ? dx : dy;
	dist = dz > dist ? dz : dist;
	dist = (dist - thing->radius) >> FRACBITS;

	if(dist < 0)
		dist = 0;

	if(dist >= bombdamage)
		return true;			// out of range

	if(P_CheckSight(thing, bombspot))
	{
		// must be in direct path
		P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist);
	}

	return true;
}

//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage)
{
	int     x;
	int     y;

	int     xl;
	int     xh;
	int     yl;
	int     yh;

	fixed_t dist;

	dist = (damage + MAXRADIUS) << FRACBITS;
	yh = (spot->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
	yl = (spot->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
	xh = (spot->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
	xl = (spot->x - dist - bmaporgx) >> MAPBLOCKSHIFT;
	bombspot = spot;
	bombsource = source;
	bombdamage = damage;

	for(y = yl; y <= yh; y++)
		for(x = xl; x <= xh; x++)
			P_BlockThingsIterator(x, y, PIT_RadiusAttack, 0);
}

//===========================================================================
// SECTOR HEIGHT CHANGING
//===========================================================================
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//
boolean crushchange;
boolean nofit;

//===========================================================================
// PIT_ChangeSector
//===========================================================================
boolean PIT_ChangeSector(mobj_t *thing, void *data)
{
	mobj_t *mo;

	// Don't check things that aren't blocklinked (supposedly immaterial).
	if(thing->flags & MF_NOBLOCKMAP)
		return true;

	if(P_ThingHeightClip(thing))
	{
		// keep checking
		return true;
	}

	// crunch bodies to giblets
	if(thing->health <= 0)
	{
		P_SetMobjState(thing, S_GIBS);

		thing->flags &= ~MF_SOLID;
		thing->height = 0;
		thing->radius = 0;

		// keep checking
		return true;
	}

	// crunch dropped items
	if(thing->flags & MF_DROPPED)
	{
		P_RemoveMobj(thing);

		// keep checking
		return true;
	}

	if(!(thing->flags & MF_SHOOTABLE))
	{
		// assume it is bloody gibs or something
		return true;
	}

	nofit = true;

	if(crushchange && !(leveltime & 3))
	{
		P_DamageMobj(thing, NULL, NULL, 10);

		// spray blood in a random direction
		mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2,
						 MT_BLOOD);

		mo->momx = (P_Random() - P_Random()) << 12;
		mo->momy = (P_Random() - P_Random()) << 12;
	}

	// keep checking (crush other things)   
	return true;
}

//===========================================================================
// P_ChangeSector
//===========================================================================
boolean P_ChangeSector(sector_t *sector, boolean crunch)
{
	/*int       x;
	   int      y; */

	nofit = false;
	crushchange = crunch;

	// re-check heights for all things near the moving sector
	/*for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
	   for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
	   P_BlockThingsIterator (x, y, PIT_ChangeSector, 0); */

	validCount++;
	P_SectorTouchingThingsIterator(sector, PIT_ChangeSector, 0);

	return nofit;
}
