/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * p_mobj.c: Map Objects
 *
 * Contains various routines for moving mobjs.
 * Collision and Z checking.
 * Original code from Doom (by id Software).
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

// Max. distance to move in one call to P_XYMovement.
#define MAXRADIUS		(32*FRACUNIT)
#define MAXMOVE			(30*FRACUNIT)	

// Shortest possible movement step.
#define MINMOVE			(FRACUNIT >> 7)

#define MIN_STEP(d)		((d) <= MINMOVE && (d) >= -MINMOVE)

// TYPES -------------------------------------------------------------------

typedef struct
{
	mobj_t		*thing;
	fixed_t		box[4];
	int			flags;
	fixed_t		x, y, z, height;
	fixed_t		floorz, ceilingz, dropoffz;
} 
checkpos_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t mapgravity;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// If set to true, P_CheckPosition will skip the mobj hit test.
boolean		dontHitMobjs = false;

fixed_t		tmfloorz;
fixed_t		tmceilingz;
fixed_t		tmdropoffz;

// When a mobj is contacted in PIT_CheckThing, this pointer is set.
// It's reset to NULL in the beginning of P_CheckPosition.
mobj_t		*blockingMobj;

// Slide variables.
fixed_t		bestslidefrac;
fixed_t		secondslidefrac;
line_t*		bestslideline;
line_t*		secondslideline;
mobj_t*		slidemo;
fixed_t		tmxmove;
fixed_t		tmymove;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// P_SetState
//	'statenum' must be a valid state (not null!).
//===========================================================================
void P_SetState(mobj_t *mobj, int statenum)
{
	state_t *st = states + statenum;
	boolean spawning = (mobj->state == 0);
	ded_ptcgen_t *pg;

#if _DEBUG
	if(statenum < 0 || statenum >= defs.count.states.num)
		Con_Error("P_SetState: statenum %i out of bounds.\n", statenum);
#endif

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

	// Check for a ptcgen trigger.
	for(pg = st->ptrigger; statenum && pg; pg = pg->state_next)
	{
		if(!(pg->flags & PGF_SPAWN_ONLY) || spawning)
		{
			// We are allowed to spawn the generator.
			P_SpawnParticleGen(pg, mobj);
		}
	}

	if(defs.states[statenum].execute)
		Con_Execute(defs.states[statenum].execute, true);
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

//===========================================================================
// PIT_CheckLine
//	Adjusts tmfloorz and tmceilingz as lines are contacted.
//===========================================================================
static boolean PIT_CheckLine (line_t* ld, void *parm)
{
	checkpos_data_t *tm = parm;
	fixed_t bbox[4];

	// Setup the bounding box for the line.
	ORDER(ld->v1->x, ld->v2->x, bbox[BOXLEFT], bbox[BOXRIGHT]);
	ORDER(ld->v1->y, ld->v2->y, bbox[BOXBOTTOM], bbox[BOXTOP]);

    if (tm->box[BOXRIGHT] <= bbox[BOXLEFT]
		|| tm->box[BOXLEFT] >= bbox[BOXRIGHT]
		|| tm->box[BOXTOP] <= bbox[BOXBOTTOM]
		|| tm->box[BOXBOTTOM] >= bbox[BOXTOP] )
		return true;
	
    if (P_BoxOnLineSide (tm->box, ld) != -1)
		return true;
	
    // A line has been hit.
	tm->thing->wallhit = true;
  
    if(!ld->backsector) return false;	// One sided line, can't go through.
	
    if(!(tm->thing->ddflags & DDMF_MISSILE))
    {
		if(ld->flags & ML_BLOCKING)
			return false;	// explicitly blocking everything
	}
    
	// set openrange, opentop, openbottom.
    P_LineOpening(ld);	
	
    // adjust floor / ceiling heights.
    if(opentop < tm->ceilingz) tm->ceilingz = opentop;
    if(openbottom > tm->floorz) tm->floorz = openbottom;	
    if(lowfloor < tm->dropoffz) tm->dropoffz = lowfloor;
	
	tm->thing->wallhit = false;
    return true;
}

//===========================================================================
// PIT_CheckThing
//===========================================================================
static boolean PIT_CheckThing (mobj_t* thing, void *parm)
{
	checkpos_data_t *tm = parm;
    fixed_t		blockdist;
    boolean		overlap = false;

    // don't clip against self
    if(thing == tm->thing) return true;
    if(!(thing->ddflags & DDMF_SOLID)) return true;
    
    blockdist = thing->radius + tm->thing->radius;

	// Only players can move under or over other things.
	if(tm->z != DDMAXINT 
		&& (tm->thing->dplayer /* || thing->dplayer */
			|| thing->ddflags & DDMF_NOGRAVITY))
	{
		if(thing->z > tm->z + tm->height)
		{
			// We're under it.
			return true;
		}
		else if(thing->z + thing->height < tm->z)
		{
			// We're over it.
			return true; 
		}
		overlap = true;
	}
	if(abs(thing->x - tm->x) >= blockdist 
		|| abs(thing->y - tm->y) >= blockdist)
    {
		// Didn't hit it.
		return true;	
    }
	if(overlap)
	{
		// How are we positioned?
		if(tm->z >= thing->z + thing->height - 24*FRACUNIT)
		{
			// Above, allowing stepup.
			tm->thing->onmobj = thing;
			tm->floorz = thing->z + thing->height;
			return true;
		}

		// To prevent getting stuck, don't block if moving away from
		// the object.
		if(tm->thing->dplayer
			&& P_ApproxDistance(tm->thing->x - thing->x, 
				tm->thing->y - thing->y)
				< P_ApproxDistance(tm->x - thing->x, tm->y - thing->y)
			&& tm->thing->momz > -12*FRACUNIT)
		{
			// The current distance is smaller than the new one would be.
			// No blocking needs to occur.
			// The Z movement test is done to prevent a 'falling through'
			// case when a thing is moving at a high speed.
			return true;
		}

		// We're hitting this mobj.
		blockingMobj = thing;
	}
    return false;
}


//
// MOVEMENT CLIPPING
//

/*
 * Returns true if it the thing can be positioned in the coordinates.
 */
boolean P_CheckPosXYZ(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z)
{
    int xl, xh;
    int	yl, yh;
    int bx, by;
    subsector_t* newsubsec;
	checkpos_data_t data;
	boolean result = true;

	blockingMobj = NULL;
	thing->onmobj = NULL;
	thing->wallhit = false;

	// Prepare the data struct.
    data.thing = thing;
    data.flags = thing->ddflags;
    data.x = x;
    data.y = y;
	data.z = z;
	data.height = thing->height;
    data.box[BOXTOP] = y + thing->radius;
    data.box[BOXBOTTOM] = y - thing->radius;
    data.box[BOXRIGHT] = x + thing->radius;
    data.box[BOXLEFT] = x - thing->radius;
	
    newsubsec = R_PointInSubsector(x,y);
       
    // The base floor / ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    data.floorz = data.dropoffz = newsubsec->sector->floorheight;
    data.ceilingz = newsubsec->sector->ceilingheight;
	
    validcount++;
  
    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    xl = (data.box[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
    xh = (data.box[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
    yl = (data.box[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
    yh = (data.box[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;
	
	if(!dontHitMobjs)
	{
		for (bx=xl ; bx<=xh ; bx++)
			for (by=yl ; by<=yh ; by++)
				if (!P_BlockThingsIterator(bx, by, PIT_CheckThing, &data))
				{
					result = false;
					goto checkpos_done;
				}
	}
			
	// check lines
	xl = (data.box[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
	xh = (data.box[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
	yl = (data.box[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
	yh = (data.box[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;
			
	for (bx=xl ; bx<=xh ; bx++)
		for (by=yl ; by<=yh ; by++)
			if (!P_BlockLinesIterator (bx, by, PIT_CheckLine, &data))
			{
				result = false;
				goto checkpos_done;
			}
			
checkpos_done:
	tmceilingz = data.ceilingz;
	tmfloorz = data.floorz;
	tmdropoffz = data.dropoffz;
	return result;
}

/*
 * Returns true if the thing can be positioned in the coordinates
 * (x,y), assuming traditional 2D Doom item placement rules.
 */ 
boolean P_CheckPosXY(mobj_t *thing, fixed_t x, fixed_t y)
{
	return P_CheckPosXYZ(thing, x, y, DDMAXINT);
}

/*
 * Attempt to move to a new (x,y,z) position.  Returns true if the
 * move was successful. Both lines and things are checked for
 * collisions.
 */
boolean P_TryMoveXYZ(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z)
{	
	int links = 0;
	boolean goodPos;

	blockingMobj = NULL;

	// Is this a real move?
	if(thing->x == x && thing->y == y && thing->z == z)
	{
		// No move. Of course it's successful.
		return true;
	}

	goodPos = P_CheckPosXYZ(thing, x, y, z);

	// Is movement clipping in effect?
	if(!thing->dplayer || !(thing->dplayer->flags & DDPF_NOCLIP))
	{
		if(!goodPos)
		{
			if(!thing->onmobj || thing->wallhit)
				return false;		// Solid wall or thing.
		}
    
		// Does it fit between contacted ceiling and floor?
		if(tmceilingz - tmfloorz < thing->height)
			return false;	
		
		if(tmceilingz - z < thing->height)
			return false;	// mobj must lower itself to fit
			
		if(thing->dplayer)
		{
			// Players are allowed a stepup.
			if(tmfloorz - z > 24*FRACUNIT)
				return false;	// too big a step up
		}
		else
		{
			// Normals mobjs are not...
			if(tmfloorz > z) return false; // below the floor
		}
	}

    // The move is OK. First unlink.
	if(IS_SECTOR_LINKED(thing)) links |= DDLINK_SECTOR;
	if(IS_BLOCK_LINKED(thing)) links |= DDLINK_BLOCKMAP;
	P_UnlinkThing(thing); 

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;	
    thing->x = x;
    thing->y = y;
	thing->z = z;

	// Put back to the same links.
	P_LinkThing(thing, links);
    return true;
}

/*
 * Try to do the given move. Returns true if nothing was hit.
 */
boolean P_StepMove(mobj_t *thing, fixed_t dx, fixed_t dy, fixed_t dz)
{
/*	fixed_t x = thing->x, y = thing->y, z = thing->z; */
	fixed_t stepX, stepY, stepZ;
	boolean notHit = true;

	while(dx || dy || dz)
	{
		stepX = dx;
		stepY = dy;
		stepZ = dz;

		// Is the step too long?
		while(stepX > MAXMOVE || stepX < -MAXMOVE
			|| stepY > MAXMOVE || stepY < -MAXMOVE
			|| stepZ > MAXMOVE || stepZ < -MAXMOVE)
		{
			// Only half that, then.
			stepX /= 2;
			stepY /= 2;
			stepZ /= 2;
		}

		// If there is no step, we're already there!
		if(!(stepX | stepY | stepZ)) return notHit;

		// Can we do this step?
		while(!P_TryMoveXYZ(thing, thing->x + stepX, thing->y + stepY, 
			thing->z + stepZ))
		{
			// We hit something!
			notHit = false;

			// This means even the current step is unreachable.
			// Let's make it our intended destination.
			dx = stepX;
			dy = stepY;
			dz = stepZ;
			
			// Try a smaller step.
			stepX /= 2;
			stepY /= 2;
			stepZ /= 2;

			// If we run out of step, we must give up.
			//if(!(stepX | stepY | stepZ)) return false;
			if(MIN_STEP(stepX) && MIN_STEP(stepY) && MIN_STEP(stepZ))
			{
				return false;
			}
		}

		// Subtract from the 'to go' distance.
		dx -= stepX;
		dy -= stepY;
		dz -= stepZ;
	}
	return notHit;
}

/*
 * Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
 * and possibly thing->z.  This is called for all nearby monsters
 * whenever a sector changes height.  If the thing doesn't fit, the z
 * will be set to the lowest value and false will be returned.
 */
static boolean P_HeightClip (mobj_t* thing)
{
    boolean	onfloor;
	
	// During demo playback the player gets preferential
	// treatment.
	if(thing->dplayer == &players[consoleplayer]
		&& playback) return true;

    onfloor = (thing->z <= thing->floorz);
	
    P_CheckPosXYZ(thing, thing->x, thing->y, thing->z);
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
	
    if(onfloor)
    {
		thing->z = thing->floorz;
    }
    else
    {
		// don't adjust a floating monster unless forced to
		if(thing->z+thing->height > thing->ceilingz)
			thing->z = thing->ceilingz - thing->height;
    }

	// On clientside, players are represented by two mobjs: the real mobj, 
	// created by the Game, is the one that is visible and modified in this
	// function. We'll need to sync the hidden client mobj (that receives
	// all the changes from the server) to match the changes.
	if(isClient && thing->dplayer)
	{
		Cl_UpdatePlayerPos(thing->dplayer);
	}

    if(thing->ceilingz - thing->floorz < thing->height) return false;
    return true;
}



//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

//
// P_WallMomSlide
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
static void P_WallMomSlide (line_t* ld)
{
    int			side;
    angle_t		lineangle;
    angle_t		moveangle;
    angle_t		deltaangle;
    fixed_t		movelen;
    fixed_t		newlen;

	// First check the simple cases.
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
    lineangle = R_PointToAngle2 (0,0, ld->dx, ld->dy);

    if(side == 1) lineangle += ANG180;

    moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);
    deltaangle = moveangle-lineangle;

    if (deltaangle > ANG180) deltaangle += ANG180;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
	
    movelen = P_ApproxDistance (tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmxmove = FixedMul(newlen, finecosine[lineangle]);	
    tmymove = FixedMul(newlen, finesine[lineangle]);	
}


//
// PTR_SlideTraverse
//
static boolean PTR_SlideTraverse (intercept_t* in)
{
    line_t*	li;
	
    if (!in->isaline)
		Con_Error ("PTR_SlideTraverse: not a line?");
		
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
	
    if(openbottom - slidemo->z > 24*FRACUNIT )
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
    return false;	// stop
}



//
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess. (No kidding?)
//
static void P_ThingSlidingMove (mobj_t* mo)
{
    fixed_t		leadx;
    fixed_t		leady;
    fixed_t		trailx;
    fixed_t		traily;
    fixed_t		newx;
    fixed_t		newy;
    int			hitcount;
		
    slidemo = mo;
    hitcount = 0;
    
retry:
    if (++hitcount == 3)
		goto stairstep;		// don't loop forever
    
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
		
    bestslidefrac = FRACUNIT+1;
	
    P_PathTraverse ( leadx, leady, leadx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( trailx, leady, trailx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( leadx, traily, leadx+mo->momx, traily+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    
    // Move up to the wall.
    if(bestslidefrac == FRACUNIT+1)
    {
		// The move most have hit the middle, so stairstep.
stairstep:
		if (!P_TryMoveXYZ(mo, mo->x, mo->y + mo->momy, mo->z))
			P_TryMoveXYZ(mo, mo->x + mo->momx, mo->y, mo->z);
		return;
    }

    // Fudge a bit to make sure it doesn't hit.
    bestslidefrac -= 0x800;	
    if(bestslidefrac > 0)
    {
		newx = FixedMul (mo->momx, bestslidefrac);
		newy = FixedMul (mo->momy, bestslidefrac);
		if(!P_TryMoveXYZ(mo, mo->x + newx, mo->y + newy, mo->z)) 
			goto stairstep;
    }
    
    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT-(bestslidefrac+0x800);
    
    if (bestslidefrac > FRACUNIT)
		bestslidefrac = FRACUNIT;
    
    if (bestslidefrac <= 0) return;
    
    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    P_WallMomSlide (bestslideline);	// clip the moves
	
    mo->momx = tmxmove;
    mo->momy = tmymove;
	
    if(!P_TryMoveXYZ(mo, mo->x + tmxmove, mo->y + tmymove, mo->z))
    {
		goto retry;
    }
}

//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
//
boolean		nofit;

static boolean PIT_SectorPlanesChanged(mobj_t *thing, void *data)
{
	// Always keep checking.
    if(P_HeightClip(thing)) return true;
    nofit = true;
    return true;	
}

/*
 * Called whenever a sector's planes are moved.  This will update the
 * things inside the sector and do crushing.
 */
boolean P_SectorPlanesChanged(sector_t *sector)
{
    nofit = false;

	// We'll use validcount to make sure things are only checked once.
	validcount++;
	P_SectorTouchingThingsIterator(sector, PIT_SectorPlanesChanged, 0);

	return nofit;
}

//
// P_XYMovement  
//
#define STOPSPEED			0x1000

void P_ThingMovement(mobj_t* mo) 
{
	P_ThingMovement2(mo, NULL);
}

// Playmove can be NULL. It's only used with player mobjs.
void P_ThingMovement2(mobj_t* mo, void *pstate)
{
	playerstate_t *playstate = pstate;
    fixed_t		ptryx, ptryy;
    fixed_t		xmove, ymove;
	ddplayer_t*	player;

    if(!mo->momx && !mo->momy) return;	// This isn't moving anywhere.
	
    player = mo->dplayer;
	
	// Make sure we're not trying to move too much.
    if(mo->momx > MAXMOVE)
		mo->momx = MAXMOVE;
    else if(mo->momx < -MAXMOVE)
		mo->momx = -MAXMOVE;
	
    if(mo->momy > MAXMOVE)
		mo->momy = MAXMOVE;
    else if(mo->momy < -MAXMOVE)
		mo->momy = -MAXMOVE;

	// Do the move in progressive steps.
    xmove = mo->momx;
    ymove = mo->momy;
    do
    {
		if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
		{
			ptryx = mo->x + xmove/2;
			ptryy = mo->y + ymove/2;
			xmove >>= 1;
			ymove >>= 1;
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}
		
		if(!P_TryMoveXYZ(mo, ptryx, ptryy, mo->z))
		{
			// Blocked move.
			if(player)
			{	
				if(blockingMobj)
				{
					// Slide along the side of the mobj.
					if(P_TryMoveXYZ(mo, mo->x, ptryy, mo->z))
					{
						mo->momx = 0;
					}
					else if(P_TryMoveXYZ(mo, ptryx, mo->y, mo->z))
					{
						mo->momy = 0;
					}
					else
					{
						// All movement stops here.
						mo->momx = mo->momy = 0;
					}
				}
				else
				{
					// Try to slide along it.
					P_ThingSlidingMove(mo);
				}
			}
			else
			{
				// Stop moving.
				mo->momx = mo->momy = 0;
			}
		}
    } while (xmove || ymove);
    
    // Apply friction.
    if(mo->ddflags & DDMF_MISSILE)
		return; 	// No friction for missiles, ever.
	
    if(mo->z > mo->floorz && !mo->onmobj && !(mo->ddflags & DDMF_FLY))
		return;		// No friction when airborne.
	
    if(mo->momx > -STOPSPEED
		&& mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED
		&& mo->momy < STOPSPEED
		&& (!playstate || (!playstate->forwardMove && !playstate->sideMove)))
    {
		mo->momx = 0;
		mo->momy = 0;
    }
    else
    {
		fixed_t friction = DEFAULT_FRICTION;
		if(playstate) friction = playstate->friction;
		mo->momx = FixedMul(mo->momx, friction);
		mo->momy = FixedMul(mo->momy, friction);
    }
}

//
// P_ZMovement
//
void P_ThingZMovement(mobj_t* mo)
{
    // check for smooth step up
    if(mo->dplayer && mo->z < mo->floorz)
    {
		mo->dplayer->viewheight -= mo->floorz-mo->z;
		mo->dplayer->deltaviewheight
			= ((41<<FRACBITS) - mo->dplayer->viewheight)>>3;
    }

	// Adjust height.
    mo->z += mo->momz;

	// Clip movement. Another thing?
	if(mo->onmobj && mo->z <= mo->onmobj->z + mo->onmobj->height)
	{
		if(mo->momz < 0)
		{
			if(mo->dplayer && mo->momz < -mapgravity*8)	
			{
				// Squat down.
				// Decrease viewheight for a moment
				// after hitting the ground (hard),
				// and utter appropriate sound.
				mo->dplayer->deltaviewheight = mo->momz >> 3;
			}
			mo->momz = 0;
		}
		if(!mo->momz) mo->z = mo->onmobj->z+mo->onmobj->height;
	}
	    
	// The floor.
    if(mo->z <= mo->floorz)
    {
		// Hit the floor.
		if(mo->momz < 0)
		{
			if(mo->dplayer && mo->momz < -mapgravity*8)	
			{
				// Squat down.
				// Decrease viewheight for a moment
				// after hitting the ground (hard),
				// and utter appropriate sound.
				mo->dplayer->deltaviewheight = mo->momz >> 3;
			}
			mo->momz = 0;
		}
		mo->z = mo->floorz;
    }
	else if(mo->ddflags & DDMF_LOWGRAVITY)
	{
		if (mo->momz == 0)
			mo->momz = -(mapgravity>>3)*2;
		else
			mo->momz -= mapgravity>>3;
	}
    else if(!(mo->ddflags & DDMF_NOGRAVITY))
    {
		if (mo->momz == 0)
			mo->momz = -mapgravity*2;
		else
			mo->momz -= mapgravity;
    }
    if(mo->z + mo->height > mo->ceilingz)
    {
		// hit the ceiling
		if(mo->momz > 0) mo->momz = 0;
		mo->z = mo->ceilingz - mo->height;
    }
} 

