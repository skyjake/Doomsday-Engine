
//**************************************************************************
//**
//** P_MOBJ.C
//**
//** Contains various routines for moving mobjs.
//** Collision and Z checking.
//** Original code from Doom.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// Max. distance to move in one call to P_XYMovement.
#define MAXMOVE			(30*FRACUNIT)
#define MAXRADIUS		(32*FRACUNIT)

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

/*fixed_t		tmbbox[4];
mobj_t*		tmthing;
int			tmflags;
fixed_t		tmx, tmy, tmz, tmheight;*/
// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean		floatok;

fixed_t		tmfloorz;
fixed_t		tmceilingz;
fixed_t		tmdropoffz;

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
	boolean spawning = (mobj->state == 0), sponly;

#if _DEBUG
	if(statenum < 0 || statenum >= defs.count.states.num)
		Con_Error("P_SetState: statenum %i out of bounds.\n", statenum);
#endif

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

	// Check for a ptcgen trigger.
	if(statenum && st->ptrigger)
	{
		sponly = (((ded_ptcgen_t*)st->ptrigger)->flags & PGF_SPAWN_ONLY) != 0;
		if(!sponly || spawning)
		{
			// We are allowed to spawn the generator.
			P_SpawnParticleGen(st->ptrigger, mobj);
		}
	}
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

//===========================================================================
// PIT_CheckLine
//	Adjusts tmfloorz and tmceilingz as lines are contacted.
//===========================================================================
boolean PIT_CheckLine (line_t* ld, checkpos_data_t *tm)
{
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
boolean PIT_CheckThing (mobj_t* thing, checkpos_data_t *tm)
{
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
		if(thing->z > tm->z+tm->height 
			|| thing->z+thing->height < tm->z)	// under or over it
			return true; 
		overlap = true;
	}
	if(abs(thing->x - tm->x) >= blockdist 
		|| abs(thing->y - tm->y) >= blockdist)
    {
		// Didn't hit it.
		return true;	
    }
	if(overlap /*&& thing->ddflags & DDMF_SOLID*/)
	{
		// How are we positioned?
		if(tm->z > thing->z + thing->height - 24*FRACUNIT)
		{
			tm->thing->onmobj = thing;
			tm->floorz = thing->z + thing->height;
			return true;
		}
	}
    return false; //!(thing->ddflags & DDMF_SOLID);
}


//
// MOVEMENT CLIPPING
//
//===========================================================================
// P_CheckPosition2
//===========================================================================
boolean P_CheckPosition2(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z)
{
    int xl, xh;
    int	yl, yh;
    int bx, by;
    subsector_t* newsubsec;
	checkpos_data_t data;
	boolean result = true;

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
	
    for (bx=xl ; bx<=xh ; bx++)
		for (by=yl ; by<=yh ; by++)
			if (!P_BlockThingsIterator(bx, by, PIT_CheckThing, &data))
			{
				result = false;
				goto checkpos_done;
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

//===========================================================================
// P_CheckPosition
//===========================================================================
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y)
{
	return P_CheckPosition2(thing, x, y, DDMAXINT);
}

//===========================================================================
// P_TryMove
//	Attempt to move to a new position.
//===========================================================================
boolean P_TryMove(mobj_t* thing, fixed_t x, fixed_t y)
{	
    floatok = false;
    if(!P_CheckPosition2(thing, x, y, thing->z))
	{
		if(!thing->onmobj || thing->wallhit)
			return false;		// Solid wall or thing.
	}
    
	if(tmceilingz - tmfloorz < thing->height)
		return false;	// doesn't fit
		
	floatok = true;
		
	if(tmceilingz - thing->z < thing->height)
		return false;	// mobj must lower itself to fit
		
	if(tmfloorz - thing->z > 24*FRACUNIT)
		return false;	// too big a step up
    
    // The move is OK. First unlink.
	// NOTE: This routine is used only for the players, so both sector and
	// blocklinks will be processed.
	P_UnlinkThing(thing); 
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;	
    thing->x = x;
    thing->y = y;
	// Link to sector and possibly blockmap.
	P_LinkThing(thing, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    return true;
}

//===========================================================================
// P_ThingHeightClip
//	Takes a valid thing and adjusts the thing->floorz,
//	thing->ceilingz, and possibly thing->z.
//	This is called for all nearby monsters
//	whenever a sector changes height.
//	If the thing doesn't fit,
//	the z will be set to the lowest value
//	and false will be returned.
//===========================================================================
boolean P_ThingHeightClip (mobj_t* thing)
{
    boolean		onfloor;
	
	// During demo playback the player gets preferential
	// treatment.
	if(thing->dplayer == &players[consoleplayer]
		&& playback) return true;

    onfloor = (thing->z <= thing->floorz);
	
    P_CheckPosition2(thing, thing->x, thing->y, thing->z);
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
    if(thing->ceilingz - thing->floorz < thing->height) return false;
    return true;
}



//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
void P_HitSlideLine (line_t* ld)
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
boolean PTR_SlideTraverse (intercept_t* in)
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
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess. (No kidding?)
//
void P_SlideMove (mobj_t* mo)
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
		if (!P_TryMove (mo, mo->x, mo->y + mo->momy))
			P_TryMove (mo, mo->x + mo->momx, mo->y);
		return;
    }

    // Fudge a bit to make sure it doesn't hit.
    bestslidefrac -= 0x800;	
    if(bestslidefrac > 0)
    {
		newx = FixedMul (mo->momx, bestslidefrac);
		newy = FixedMul (mo->momy, bestslidefrac);
		if (!P_TryMove (mo, mo->x+newx, mo->y+newy)) goto stairstep;
    }
    
    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT-(bestslidefrac+0x800);
    
    if (bestslidefrac > FRACUNIT)
		bestslidefrac = FRACUNIT;
    
    if (bestslidefrac <= 0) return;
    
    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    P_HitSlideLine (bestslideline);	// clip the moves
	
    mo->momx = tmxmove;
    mo->momy = tmymove;
	
    if(!P_TryMove (mo, mo->x+tmxmove, mo->y+tmymove))
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

//===========================================================================
// PIT_ChangeSector
//===========================================================================
boolean PIT_ChangeSector(mobj_t *thing, void *data)
{
	// Always keep checking.
    if(P_ThingHeightClip(thing)) return true;
    nofit = true;
    return true;	
}

//===========================================================================
// P_ChangeSector
//===========================================================================
boolean P_ChangeSector(sector_t *sector)
{
/*    int		x;
    int		y;
	mobj_t *iter;*/
	
    nofit = false;

	// We'll use validcount to make sure things are only checked once.
	validcount++;
	P_SectorTouchingThingsIterator(sector, PIT_ChangeSector, 0);

	// First check what's sectorlinked.
	/*for(iter = sector->thinglist; iter; iter = iter->snext)
	{
		PIT_ChangeSector(iter, 0);
		iter->valid = validcount;
	}

    // re-check heights for all things near the moving sector
    for(x=sector->blockbox[BOXLEFT]; x<=sector->blockbox[BOXRIGHT]; x++)
	{
		for(y=sector->blockbox[BOXBOTTOM]; y<=sector->blockbox[BOXTOP]; y++)
			P_BlockThingsIterator(x, y, PIT_ChangeSector, 0);
	}*/
	return nofit;
}

//
// P_XYMovement  
//
#define STOPSPEED			0x1000

void P_XYMovement(mobj_t* mo) 
{
	P_XYMovement2(mo, 0);
}

// Playmove can be NULL. It's only used with player mobjs.
void P_XYMovement2(mobj_t* mo, struct playerstate_s *playstate) 
{ 	
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
		
		if(!P_TryMove(mo, ptryx, ptryy))
		{
			// Blocked move.
			if(player)
			{	
				// Try to slide along it.
				P_SlideMove (mo);
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
		&& (!playstate || (!playstate->forwardmove && !playstate->sidemove)))
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
void P_ZMovement(mobj_t* mo)
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
