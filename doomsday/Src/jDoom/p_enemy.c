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
// Revision 1.6  2004/05/28 17:16:40  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.4.2.1  2004/05/16 10:01:36  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.4.4.3  2004/01/07 13:17:28  skyjake
// Refresh header cleanup
//
// Revision 1.4.4.2  2003/11/22 18:09:10  skyjake
// Cleanup
//
// Revision 1.4.4.1  2003/11/19 17:07:12  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.4  2003/07/01 12:42:26  skyjake
// Allow massacre only when GS_LEVEL
//
// Revision 1.3  2003/04/29 13:11:52  skyjake
// Missile puff ptcgen issue fixed
//
// Revision 1.2  2003/02/27 23:14:32  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:21:53  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	Enemy thinking, AI.
//	Action Pointer Functions
//	that are associated with states/frames. 
//
//-----------------------------------------------------------------------------

#ifdef WIN32
#pragma optimize("g", off)
#endif

static const char
rcsid[] = "$Id$";

#include <stdlib.h>

#include "m_random.h"

#include "doomdef.h"
#include "d_config.h"
#include "p_local.h"

#include "s_sound.h"

#include "g_game.h"

// State.
#include "doomstat.h"
#include "r_state.h"

typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
    
} dirtype_t;

/*
//
// P_NewChaseDir related LUT.
//
dirtype_t opposite[] =
{
  DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
  DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

dirtype_t diags[] =
{
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};
*/




void C_DECL A_Fall (mobj_t *actor);


//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//

mobj_t*		soundtarget;

void
P_RecursiveSound
( sector_t*	sec,
  int		soundblocks )
{
    int		i;
    line_t*	check;
    sector_t*	other;
	
    // wake up all monsters in this sector
    if (sec->Validcount == validCount && sec->soundtraversed <= soundblocks+1)
    {
		return;		// already flooded
    }
    
    sec->Validcount = validCount;
    sec->soundtraversed = soundblocks+1;
    sec->soundtarget = soundtarget;
	
    for (i=0 ;i<sec->linecount ; i++)
    {
	check = sec->Lines[i];
	if (! (check->flags & ML_TWOSIDED) )
	    continue;
	
	P_LineOpening (check);

	if (openrange <= 0)
	    continue;	// closed door
	
	if ( sides[ check->sidenum[0] ].sector == sec)
	    other = sides[ check->sidenum[1] ] .sector;
	else
	    other = sides[ check->sidenum[0] ].sector;
	
	if (check->flags & ML_SOUNDBLOCK)
	{
	    if (!soundblocks)
		P_RecursiveSound (other, 1);
	}
	else
	    P_RecursiveSound (other, soundblocks);
    }
}



//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void
P_NoiseAlert
( mobj_t*	target,
  mobj_t*	emmiter )
{
    soundtarget = target;
    validCount++;
    P_RecursiveSound (emmiter->subsector->sector, 0);
}




//
// P_CheckMeleeRange
//
boolean P_CheckMeleeRange (mobj_t*	actor)
{
    mobj_t*	pl;
    fixed_t	dist;
	
    if (!actor->target)
		return false;
	
    pl = actor->target;
    dist = P_ApproxDistance (pl->x-actor->x, pl->y-actor->y);

	//if(INCOMPAT_OK && cfg.moveCheckZ)
	//{
	dist = P_ApproxDistance(dist, (pl->z+(pl->height>>1))-(actor->z+(actor->height>>1)));
	//}
	
    if (dist >= MELEERANGE-20*FRACUNIT+pl->info->radius)
		return false;
	
    if (! P_CheckSight (actor, actor->target) )
		return false;
	
    return true;		
}

//
// P_CheckMissileRange
//
boolean P_CheckMissileRange (mobj_t* actor)
{
    fixed_t	dist;
	
    if (! P_CheckSight (actor, actor->target) )
	return false;
	
    if ( actor->flags & MF_JUSTHIT )
    {
	// the target just hit the enemy,
	// so fight back!
	actor->flags &= ~MF_JUSTHIT;
	return true;
    }
	
    if (actor->reactiontime)
	return false;	// do not attack yet
		
    // OPTIMIZE: get this from a global checksight
    dist = P_ApproxDistance ( actor->x-actor->target->x,
			     actor->y-actor->target->y) - 64*FRACUNIT;
    
    if (!actor->info->meleestate)
	dist -= 128*FRACUNIT;	// no melee attack, so fire more

    dist >>= 16;

    if (actor->type == MT_VILE)
    {
	if (dist > 14*64)	
	    return false;	// too far away
    }
	

    if (actor->type == MT_UNDEAD)
    {
	if (dist < 196)	
	    return false;	// close for fist attack
	dist >>= 1;
    }
	

    if (actor->type == MT_CYBORG
	|| actor->type == MT_SPIDER
	|| actor->type == MT_SKULL)
    {
	dist >>= 1;
    }
    
    if (dist > 200)
	dist = 200;
		
    if (actor->type == MT_CYBORG && dist > 160)
	dist = 160;
		
    if (P_Random () < dist)
	return false;
		
    return true;
}


//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
fixed_t	xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

extern	line_t*	spechit[MAXSPECIALCROSS];
extern	int	numspechit;
extern	boolean felldown;	//$dropoff_fix: used to flag pushed off ledge
extern	line_t*	blockline;	// $unstuck: blocking linedef

// killough $dropoff_fix
boolean P_Move (mobj_t*	actor, boolean dropoff)
{
    fixed_t	tryx, stepx;
    fixed_t	tryy, stepy;
    
    line_t*	ld;
    
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
    boolean	good;
	
    if (actor->movedir == DI_NODIR)
		return false;
	
    if ((unsigned)actor->movedir >= 8)
		Con_Error ("Weird actor->movedir!");
	
	stepx = actor->info->speed/FRACUNIT * xspeed[actor->movedir];
	stepy = actor->info->speed/FRACUNIT * yspeed[actor->movedir];
    tryx = actor->x + stepx;
    tryy = actor->y + stepy;

	// killough $dropoff_fix
    if (!P_TryMove (actor, tryx, tryy, dropoff))
    {
		// open any specials
		if (actor->flags & MF_FLOAT && floatok)
		{
			// must adjust height
			if (actor->z < tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;
			
			actor->flags |= MF_INFLOAT;
			return true;
		}
		
		if (numspechit <= 0)
			return false;
		
		actor->movedir = DI_NODIR;
		good = false;
		while (numspechit--)
		{
			ld = spechit[numspechit];

			// if the special is not a door that can be opened, return false
			//
			// killough $unstuck: this is what caused monsters to get stuck in
			// doortracks, because it thought that the monster freed itself
			// by opening a door, even if it was moving towards the doortrack,
			// and not the door itself.
			//
			// If a line blocking the monster is activated, return true 90%
			// of the time. If a line blocking the monster is not activated,
			// but some other line is, return false 90% of the time.
			// A bit of randomness is needed to ensure it's free from
			// lockups, but for most cases, it returns the correct result.
			//
			// Do NOT simply return false 1/4th of the time (causes monsters to
			// back out when they shouldn't, and creates secondary stickiness).

			if (P_UseSpecialLine (actor, ld,0))
				good |= ld == blockline ? 1 : 2;
//				good = true;
		}
		return good && ((P_Random() >= 230) ^ (good & 1));
//		return good;
    }
    else
    {
		P_SetThingSRVO(actor, stepx, stepy); // "servo": movement smoothing
		actor->flags &= ~MF_INFLOAT;
    }
	
	// $dropoff_fix: fall more slowly, under gravity, if felldown==true
	if (!(actor->flags & MF_FLOAT) && !felldown)
		actor->z = actor->floorz;
    return true; 
}


//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
boolean P_TryWalk (mobj_t* actor)
{	
	// killough $dropoff_fix
    if (!P_Move (actor, false))
    {
		return false;
    }

    actor->movecount = P_Random()&15;
    return true;
}

/*void P_NewChaseDir (mobj_t*	actor)
{
	fixed_t	deltax;
	fixed_t	deltay;
    
	dirtype_t	d[3];
    
    int		tdir;
    dirtype_t	olddir;
    
    dirtype_t	turnaround;

    if (!actor->target)
	Con_Error ("P_NewChaseDir: called with no target");
		
    olddir = actor->movedir;
    turnaround=opposite[olddir];

    deltax = actor->target->x - actor->x;
    deltay = actor->target->y - actor->y;

    if (deltax>10*FRACUNIT)
	d[1]= DI_EAST;
    else if (deltax<-10*FRACUNIT)
	d[1]= DI_WEST;
    else
	d[1]=DI_NODIR;

    if (deltay<-10*FRACUNIT)
	d[2]= DI_SOUTH;
    else if (deltay>10*FRACUNIT)
	d[2]= DI_NORTH;
    else
	d[2]=DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR
	&& d[2] != DI_NODIR)
    {
	actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
	if (actor->movedir != turnaround && P_TryWalk(actor))
	    return;
    }

    // try other directions
    if (P_Random() > 200
	||  abs(deltay)>abs(deltax))
    {
	tdir=d[1];
	d[1]=d[2];
	d[2]=tdir;
    }

    if (d[1]==turnaround)
	d[1]=DI_NODIR;
    if (d[2]==turnaround)
	d[2]=DI_NODIR;
	
    if (d[1]!=DI_NODIR)
    {
	actor->movedir = d[1];
	if (P_TryWalk(actor))
	{
	    // either moved forward or attacked
	    return;
	}
    }

    if (d[2]!=DI_NODIR)
    {
	actor->movedir =d[2];

	if (P_TryWalk(actor))
	    return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
	actor->movedir =olddir;

	if (P_TryWalk(actor))
	    return;
    }

    // randomly determine direction of search
    if (P_Random()&1) 	
    {
	for ( tdir=DI_EAST;
	      tdir<=DI_SOUTHEAST;
	      tdir++ )
	{
	    if (tdir!=turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }
    else
    {
	for ( tdir=DI_SOUTHEAST;
	      tdir != (DI_EAST-1);
	      tdir-- )
	{
	    if (tdir!=turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }

    if (turnaround !=  DI_NODIR)
    {
	actor->movedir =turnaround;
	if ( P_TryWalk(actor) )
	    return;
    }

    actor->movedir = DI_NODIR;	// can not move
}
*/

//*****************************************************************************
//  killough $dropoff_fix Replace above P_DoNewChaseDir with following 4 functions
//
// P_DoNewChaseDir
//
// Most of P_NewChaseDir(), except for what
// determines the new direction to take
//

static void P_DoNewChaseDir(mobj_t *actor, fixed_t deltax, fixed_t deltay)
{
	dirtype_t xdir, ydir, tdir;
	dirtype_t olddir = actor->movedir;
	dirtype_t turnaround = olddir;

	if (turnaround != DI_NODIR)         // find reverse direction
		turnaround ^= 4;

	xdir = 
		deltax >  10*FRACUNIT ? DI_EAST :
		deltax < -10*FRACUNIT ? DI_WEST : DI_NODIR;

	ydir = 
		deltay < -10*FRACUNIT ? DI_SOUTH :
		deltay >  10*FRACUNIT ? DI_NORTH : DI_NODIR;

	// try direct route
	if (xdir != DI_NODIR && ydir != DI_NODIR && turnaround != 
		(actor->movedir = deltay < 0 ? deltax > 0 ? DI_SOUTHEAST : DI_SOUTHWEST :
		deltax > 0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
	return;

	// try other directions
	if (P_Random() > 200 || abs(deltay)>abs(deltax))
		tdir = xdir, xdir = ydir, ydir = tdir;

	if ((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
		(actor->movedir = xdir, P_TryWalk(actor)))
		return;         // either moved forward or attacked

	if ((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
		(actor->movedir = ydir, P_TryWalk(actor)))
		return;

	// there is no direct path to the player, so pick another direction.
	if (olddir != DI_NODIR && (actor->movedir = olddir, P_TryWalk(actor)))
		return;

	// randomly determine direction of search
	if (P_Random() & 1)
	{
		for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
			if (tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
				return;
	}
	else
	for (tdir = DI_SOUTHEAST; tdir != DI_EAST-1; tdir--)
		if (tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
			return;
  
	if ((actor->movedir = turnaround) != DI_NODIR && !P_TryWalk(actor))
		actor->movedir = DI_NODIR;
}

//
// Monsters try to move away from tall dropoffs.
//
// In Doom, they were never allowed to hang over dropoffs,
// and would remain stuck if involuntarily forced over one.
// This logic, combined with p_map.c (P_TryMove), allows
// monsters to free themselves without making them tend to
// hang over dropoffs.

static fixed_t dropoff_deltax, dropoff_deltay, floorz;
fixed_t tmbbox[4];

//===========================================================================
// PIT_AvoidDropoff
//	Modified by jk @ 5/29/02
//===========================================================================
static boolean PIT_AvoidDropoff(line_t *line, void *data)
{
/*	if (line->backsector                          && // Ignore one-sided linedefs
		tmbbox[BOXRIGHT]  > line->bbox[BOXLEFT]   &&
		tmbbox[BOXLEFT]   < line->bbox[BOXRIGHT]  &&
		tmbbox[BOXTOP]    > line->bbox[BOXBOTTOM] && // Linedef must be contacted
		tmbbox[BOXBOTTOM] < line->bbox[BOXTOP]    &&
		P_BoxOnLineSide(tmbbox, line) == -1)
	{*/
	
	fixed_t front = line->frontsector->floorheight;
	fixed_t back  = line->backsector->floorheight;
	angle_t angle;

	// The monster must contact one of the two floors,
	// and the other must be a tall drop off (more than 24).

	if (back == floorz && front < floorz - FRACUNIT*24)
		angle = R_PointToAngle2(0,0,line->dx,line->dy);   // front side drop off
	else if (front == floorz && back < floorz - FRACUNIT*24)
		angle = R_PointToAngle2(line->dx,line->dy,0,0); // back side drop off
	else
		return true;

	// Move away from drop off at a standard speed.
	// Multiple contacted linedefs are cumulative (e.g. hanging over corner)
	dropoff_deltax -= finesine[angle >> ANGLETOFINESHIFT]*32;
	dropoff_deltay += finecosine[angle >> ANGLETOFINESHIFT]*32;
	
	return true;
}

//
// Driver for above
//

//===========================================================================
// P_AvoidDropoff
//	Modified by jk @ 5/29/02
//===========================================================================
static fixed_t P_AvoidDropoff(mobj_t *actor)
{
/*	int yh=((tmbbox[BOXTOP]   = actor->y+actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
	int yl=((tmbbox[BOXBOTTOM]= actor->y-actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
	int xh=((tmbbox[BOXRIGHT] = actor->x+actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
	int xl=((tmbbox[BOXLEFT]  = actor->x-actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
	int bx, by;
*/
	floorz = actor->z;            // remember floor height

	dropoff_deltax = dropoff_deltay = 0;

	// check lines

	validCount++;
	/*for (bx=xl ; bx<=xh ; bx++)
		for (by=yl ; by<=yh ; by++)
			P_BlockLinesIterator(bx, by, PIT_AvoidDropoff, 0);  // all contacted lines*/

	P_ThingLinesIterator(actor, PIT_AvoidDropoff, 0);

	return dropoff_deltax | dropoff_deltay;   // Non-zero if movement prescribed
}


void P_NewChaseDir (mobj_t*	actor)
{
	mobj_t *target = actor->target;
	fixed_t deltax = target->x - actor->x;
	fixed_t deltay = target->y - actor->y;

	if (actor->floorz - actor->dropoffz > FRACUNIT*24 &&
	actor->z <= actor->floorz && !(actor->flags & (MF_DROPOFF|MF_FLOAT)) &&
	P_AvoidDropoff(actor)) // Move away from dropoff
	{
		P_DoNewChaseDir(actor, dropoff_deltax, dropoff_deltay);

		// $dropoff_fix
		// If moving away from drop off, set movecount to 1 so that 
		// small steps are taken to get monster away from drop off.

		actor->movecount = 1;
		return;
	}

	P_DoNewChaseDir(actor, deltax, deltay);
}

//*****************************************************************************


//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
boolean
P_LookForPlayers
( mobj_t*	actor,
 boolean	allaround )
{
    int			c;
    int			stop;
    player_t*	player;
    sector_t*	sector;
    angle_t		an;
    fixed_t		dist;
	int			playerCount;

	for(c = playerCount = 0; c < MAXPLAYERS; c++)
		if(players[c].plr->ingame) playerCount++; 

	// Are there any players?
	if(!playerCount) return false;
	
    sector = actor->subsector->sector;
	
    c = 0;
    stop = (actor->lastlook-1)&3;
	
    for ( ; ; actor->lastlook = (actor->lastlook+1)&3 )
    {
		if (!players[actor->lastlook].plr->ingame)
			continue;
		
		if (c++ == 2 || actor->lastlook == stop)
		{
			// done looking
			return false;	
		}
		
		player = &players[actor->lastlook];
		
		if (player->health <= 0)
			continue;		// dead
		
		if (!P_CheckSight (actor, player->plr->mo))
			continue;		// out of sight
		
		if (!allaround)
		{
			an = R_PointToAngle2 (actor->x,
				actor->y, 
				player->plr->mo->x,
				player->plr->mo->y)
				- actor->angle;
			
			if (an > ANG90 && an < ANG270)
			{
				dist = P_ApproxDistance (player->plr->mo->x - actor->x,
					player->plr->mo->y - actor->y);
				// if real close, react anyway
				if (dist > MELEERANGE)
					continue;	// behind back
			}
		}
		
		actor->target = player->plr->mo;
		return true;
    }
	
    return false;
}

int P_Massacre(void)
{
	int count = 0;
	mobj_t *mo;
	thinker_t *think;

	// Only massacre when in a level.
	if(gamestate != GS_LEVEL) return 0;

	for(think = thinkercap.next; think != &thinkercap;
		think = think->next)
	{
		if(think->function != P_MobjThinker)
		{ // Not a mobj thinker
			continue;
		}
		mo = (mobj_t *)think;
		if(mo->type == MT_SKULL ||
		   (mo->flags & MF_COUNTKILL && mo->health > 0))
		{
			P_DamageMobj(mo, NULL, NULL, 10000);
			count++;
		}
	}
	return count;
}


//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void C_DECL A_KeenDie (mobj_t* mo)
{
    thinker_t*	th;
    mobj_t*	mo2;
    line_t	junk;

    A_Fall (mo);
    
    // scan the remaining thinkers
    // to see if all Keens are dead
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function != P_MobjThinker)
	    continue;

	mo2 = (mobj_t *)th;
	if (mo2 != mo
	    && mo2->type == mo->type
	    && mo2->health > 0)
	{
	    // other Keen not dead
	    return;		
	}
    }

    junk.tag = 666;
    EV_DoDoor(&junk,open);
}


//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//
void C_DECL A_Look (mobj_t* actor)
{
    mobj_t*	targ;
	
    actor->threshold = 0;	// any shot will wake up
    targ = actor->subsector->sector->soundtarget;
	
    if(targ && (targ->flags & MF_SHOOTABLE))
    {
		actor->target = targ;
		
		if ( actor->flags & MF_AMBUSH )
		{
			if (P_CheckSight (actor, actor->target))
				goto seeyou;
		}
		else
			goto seeyou;
    }
	
	
    if (!P_LookForPlayers (actor, false) )
		return;
	
    // go into chase state
seeyou:
    if (actor->info->seesound)
    {
		int		sound;
		
		switch (actor->info->seesound)
		{
		case sfx_posit1:
		case sfx_posit2:
		case sfx_posit3:
			sound = sfx_posit1+P_Random()%3;
			break;
			
		case sfx_bgsit1:
		case sfx_bgsit2:
			sound = sfx_bgsit1+P_Random()%2;
			break;
			
		default:
			sound = actor->info->seesound;
			break;
		}
		
		if (actor->type==MT_SPIDER
			|| actor->type == MT_CYBORG)
		{
			// full volume
			S_StartSound(sound | DDSF_NO_ATTENUATION, actor);
		}
		else
			S_StartSound(sound, actor);
    }
	
    P_SetMobjState (actor, actor->info->seestate);
}


//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void C_DECL A_Chase (mobj_t*	actor)
{
    int		delta;
	
    if (actor->reactiontime)
		actor->reactiontime--;
				
    // modify target threshold
    if  (actor->threshold)
    {
		if (!actor->target
			|| actor->target->health <= 0)
		{
			actor->threshold = 0;
		}
		else
			actor->threshold--;
    }
    
    // turn towards movement direction if not there yet
    if (actor->movedir < 8)
    {
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);
		
		if (delta > 0)
			actor->angle -= ANG90/2;
		else if (delta < 0)
			actor->angle += ANG90/2;
    }
	
    if (!actor->target
		|| !(actor->target->flags&MF_SHOOTABLE))
    {
		// look for a new target
		if (P_LookForPlayers(actor,true))
			return; 	// got a new target
		
		P_SetMobjState (actor, actor->info->spawnstate);
		return;
    }
    
    // do not attack twice in a row
    if (actor->flags & MF_JUSTATTACKED)
    {
		actor->flags &= ~MF_JUSTATTACKED;
		if (gameskill != sk_nightmare && !fastparm)
			P_NewChaseDir (actor);
		return;
    }
    
    // check for melee attack
    if (actor->info->meleestate
		&& P_CheckMeleeRange (actor))
    {
		if (actor->info->attacksound)
			S_StartSound (actor->info->attacksound, actor);
		
		P_SetMobjState (actor, actor->info->meleestate);
		return;
    }
    
    // check for missile attack
    if (actor->info->missilestate)
    {
		if (gameskill < sk_nightmare
			&& !fastparm && actor->movecount)
		{
			goto nomissile;
		}
		
		if (!P_CheckMissileRange (actor))
			goto nomissile;
		
		P_SetMobjState (actor, actor->info->missilestate);
		actor->flags |= MF_JUSTATTACKED;
		return;
    }
	
    // ?
nomissile:
    // possibly choose another target
    if (IS_NETGAME
		&& !actor->threshold
		&& !P_CheckSight (actor, actor->target) )
    {
		if (P_LookForPlayers(actor,true))
			return;	// got a new target
    }
    
    // chase towards player
	// killough $dropoff_fix
    if (--actor->movecount<0 || !P_Move (actor, false))
    {
		P_NewChaseDir (actor);
    }
    
    // make active sound
    if (actor->info->activesound
		&& P_Random () < 3)
    {
		S_StartSound (actor->info->activesound, actor);
    }
}


//
// A_FaceTarget
//
void C_DECL A_FaceTarget (mobj_t* actor)
{	
    if (!actor->target) return;
    
	actor->turntime = true; // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;
    actor->angle = R_PointToAngle2 (actor->x,
				    actor->y,
				    actor->target->x,
				    actor->target->y);
    
    if (actor->target->flags & MF_SHADOW)
		actor->angle += (P_Random()-P_Random())<<21;
}


//
// A_PosAttack
//
void C_DECL A_PosAttack (mobj_t* actor)
{
    int		angle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    angle = actor->angle;
    slope = P_AimLineAttack (actor, angle, MISSILERANGE);

    S_StartSound(sfx_pistol, actor);
    angle += (P_Random()-P_Random())<<20;
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_SPosAttack (mobj_t* actor)
{
    int		i;
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
		return;
	
    S_StartSound(sfx_shotgn, actor);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);
	
    for (i=0 ; i<3 ; i++)
    {
		angle = bangle + ((P_Random()-P_Random())<<20);
		damage = ((P_Random()%5)+1)*3;
		P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
    }
}

void C_DECL A_CPosAttack (mobj_t* actor)
{
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;

    S_StartSound(sfx_shotgn, actor);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random()-P_Random())<<20);
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_CPosRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 40)
	return;

    if (!actor->target
	|| actor->target->health <= 0
	|| !P_CheckSight (actor, actor->target) )
    {
	P_SetMobjState (actor, actor->info->seestate);
    }
}


void C_DECL A_SpidRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 10)
	return;

    if (!actor->target
	|| actor->target->health <= 0
	|| !P_CheckSight (actor, actor->target) )
    {
	P_SetMobjState (actor, actor->info->seestate);
    }
}

void C_DECL A_BspiAttack (mobj_t *actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);

    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_ARACHPLAZ);
}


//
// A_TroopAttack
//
void C_DECL A_TroopAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
		return;
	
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
		S_StartSound(sfx_claw, actor);
		damage = (P_Random()%8+1)*3;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
    }
	
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_TROOPSHOT);
}


void C_DECL A_SargAttack (mobj_t* actor)
{
    int		damage;

    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
		damage = ((P_Random()%10)+1)*4;
		P_DamageMobj (actor->target, actor, actor, damage);
    }
}

void C_DECL A_HeadAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target) return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
		damage = (P_Random()%6+1)*10;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_HEADSHOT);
}

void C_DECL A_CyberAttack (mobj_t* actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    P_SpawnMissile (actor, actor->target, MT_ROCKET);
}


void C_DECL A_BruisAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    if (P_CheckMeleeRange (actor))
    {
		S_StartSound(sfx_claw, actor);
		damage = (P_Random()%8+1)*10;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_BRUISERSHOT);
}


//
// A_SkelMissile
//
void C_DECL A_SkelMissile (mobj_t* actor)
{	
    mobj_t*	mo;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    actor->z += 16*FRACUNIT;	// so missile spawns higher
    mo = P_SpawnMissile (actor, actor->target, MT_TRACER);
    actor->z -= 16*FRACUNIT;	// back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = actor->target;
}

int	TRACEANGLE = 0xc000000;

void C_DECL A_Tracer (mobj_t* actor)
{
    angle_t	exact;
    fixed_t	dist;
    fixed_t	slope;
    mobj_t*	dest;
    mobj_t*	th;
	
    if (gametic & 3) return;
    
    // spawn a puff of smoke behind the rocket		
    P_SpawnCustomPuff(actor->x, actor->y, actor->z, MT_ROCKETPUFF);
	
    th = P_SpawnMobj (actor->x-actor->momx,
		      actor->y-actor->momy,
			  actor->z, MT_SMOKE);
    
    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;
    if (th->tics < 1)
		th->tics = 1;
    
    // adjust direction
    dest = actor->tracer;
	
    if (!dest || dest->health <= 0)
		return;
    
    // change angle	
    exact = R_PointToAngle2 (actor->x,
			     actor->y,
				 dest->x,
				 dest->y);
	
    if (exact != actor->angle)
    {
		if (exact - actor->angle > 0x80000000)
		{
			actor->angle -= TRACEANGLE;
			if (exact - actor->angle < 0x80000000)
				actor->angle = exact;
		}
		else
		{
			actor->angle += TRACEANGLE;
			if (exact - actor->angle > 0x80000000)
				actor->angle = exact;
		}
    }
	
    exact = actor->angle>>ANGLETOFINESHIFT;
    actor->momx = FixedMul (actor->info->speed, finecosine[exact]);
    actor->momy = FixedMul (actor->info->speed, finesine[exact]);
    
    // change slope
    dist = P_ApproxDistance (dest->x - actor->x,
		dest->y - actor->y);
    
    dist = dist / actor->info->speed;
	
    if (dist < 1) dist = 1;
    slope = (dest->z+40*FRACUNIT - actor->z) / dist;
	
    if (slope < actor->momz)
		actor->momz -= FRACUNIT/8;
    else
		actor->momz += FRACUNIT/8;
}


void C_DECL A_SkelWhoosh (mobj_t*	actor)
{
    if (!actor->target)
		return;
    A_FaceTarget (actor);
    S_StartSound(sfx_skeswg, actor);
}

void C_DECL A_SkelFist (mobj_t*	actor)
{
    int		damage;
	
    if (!actor->target)
		return;
	
    A_FaceTarget (actor);
	
    if (P_CheckMeleeRange (actor))
    {
		damage = ((P_Random()%10)+1)*6;
		S_StartSound(sfx_skepch, actor);
		P_DamageMobj (actor->target, actor, actor, damage);
    }
}



//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
mobj_t*		corpsehit;
mobj_t*		vileobj;
fixed_t		viletryx;
fixed_t		viletryy;

boolean PIT_VileCheck (mobj_t*	thing, void *data)
{
    int		maxdist;
    boolean	check;
	
    if (!(thing->flags & MF_CORPSE) )
	return true;	// not a monster
    
    if (thing->tics != -1)
	return true;	// not lying still yet
    
    if (thing->info->raisestate == S_NULL)
	return true;	// monster doesn't have a raise state
    
    maxdist = thing->info->radius + mobjinfo[MT_VILE].radius;
	
    if ( abs(thing->x - viletryx) > maxdist
	 || abs(thing->y - viletryy) > maxdist )
	return true;		// not actually touching
		
    corpsehit = thing;
    corpsehit->momx = corpsehit->momy = 0;
    corpsehit->height <<= 2;
    check = P_CheckPosition (corpsehit, corpsehit->x, corpsehit->y);
    corpsehit->height >>= 2;

    if (!check)
	return true;		// doesn't fit here
		
    return false;		// got one, so stop checking
}



//
// A_VileChase
// Check for ressurecting a body
//
void C_DECL A_VileChase (mobj_t* actor)
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    
    int			bx;
    int			by;

    mobjinfo_t*		info;
    mobj_t*		temp;
	
    if (actor->movedir != DI_NODIR)
    {
	// check for corpses to raise
	viletryx =
	    actor->x + actor->info->speed/FRACUNIT*xspeed[actor->movedir];
	viletryy =
	    actor->y + actor->info->speed/FRACUNIT*yspeed[actor->movedir];

	xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
	xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
	yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
	yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;
	
	vileobj = actor;
	for (bx=xl ; bx<=xh ; bx++)
	{
		for (by=yl ; by<=yh ; by++)
		{
			// Call PIT_VileCheck to check
			// whether object is a corpse
			// that canbe raised.
			if (!P_BlockThingsIterator(bx,by,PIT_VileCheck,0))
			{
				// got one!
				temp = actor->target;
				actor->target = corpsehit;
				A_FaceTarget (actor);
				actor->target = temp;
				
				P_SetMobjState (actor, S_VILE_HEAL1);
				S_StartSound(sfx_slop, corpsehit);
				info = corpsehit->info;
				
				P_SetMobjState (corpsehit,info->raisestate);
				corpsehit->height <<= 2;
				corpsehit->flags = info->flags;
				corpsehit->health = info->spawnhealth;
				corpsehit->target = NULL;
				
				return;
			}
		}
	}
    }

    // Return to normal attack.
    A_Chase (actor);
}


//
// A_VileStart
//
void C_DECL A_VileStart (mobj_t* actor)
{
    S_StartSound(sfx_vilatk, actor);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void C_DECL A_Fire (mobj_t* actor);

void C_DECL A_StartFire (mobj_t* actor)
{
    S_StartSound(sfx_flamst, actor);
    A_Fire(actor);
}

void C_DECL A_FireCrackle (mobj_t* actor)
{
    S_StartSound(sfx_flame, actor);
    A_Fire(actor);
}

void C_DECL A_Fire (mobj_t* actor)
{
    mobj_t*	dest;
    unsigned	an;
	
    dest = actor->tracer;
    if (!dest)
		return;
	
    // don't move it if the vile lost sight
    if (!P_CheckSight (actor->target, dest) )
		return;
	
    an = dest->angle >> ANGLETOFINESHIFT;
	
    P_UnsetThingPosition (actor);
    actor->x = dest->x + FixedMul (24*FRACUNIT, finecosine[an]);
    actor->y = dest->y + FixedMul (24*FRACUNIT, finesine[an]);
    actor->z = dest->z;
    P_SetThingPosition (actor);
}



//
// A_VileTarget
// Spawn the hellfire
//
void C_DECL A_VileTarget (mobj_t*	actor)
{
    mobj_t*	fog;
	
    if (!actor->target)
	return;

    A_FaceTarget (actor);

    fog = P_SpawnMobj (actor->target->x,
		       actor->target->x,
		       actor->target->z, MT_FIRE);
    
    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    A_Fire (fog);
}




//
// A_VileAttack
//
void C_DECL A_VileAttack (mobj_t* actor)
{	
    mobj_t*	fire;
    int		an;
	
    if (!actor->target)
	return;
    
    A_FaceTarget (actor);

    if (!P_CheckSight (actor, actor->target) )
	return;

    S_StartSound(sfx_barexp, actor);
    P_DamageMobj (actor->target, actor, actor, 20);
    actor->target->momz = 1000*FRACUNIT/actor->target->info->mass;
	
    an = actor->angle >> ANGLETOFINESHIFT;

    fire = actor->tracer;

    if (!fire)
	return;
		
    // move the fire between the vile and the player
    fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
    fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);	
    P_RadiusAttack (fire, actor, 70 );
}




//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it. 
//
#define	FATSPREAD	(ANG90/8)

void C_DECL A_FatRaise (mobj_t *actor)
{
    A_FaceTarget (actor);
    S_StartSound(sfx_manatk, actor);
}


void C_DECL A_FatAttack1 (mobj_t* actor)
{
    mobj_t*	mo;
    int		an;
	
    A_FaceTarget (actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile (actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
}

void C_DECL A_FatAttack2 (mobj_t* actor)
{
    mobj_t*	mo;
    int		an;

    A_FaceTarget (actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile (actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD*2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
}

void C_DECL A_FatAttack3 (mobj_t*	actor)
{
    mobj_t*	mo;
    int		an;

    A_FaceTarget (actor);
    
    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD/2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD/2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
}


//
// SkullAttack
// Fly at the player like a missile.
//
#define	SKULLSPEED		(20*FRACUNIT)

void C_DECL A_SkullAttack (mobj_t* actor)
{
    mobj_t*		dest;
    angle_t		an;
    int			dist;

    if (!actor->target)
	return;
		
    dest = actor->target;	
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attacksound, actor);
    A_FaceTarget (actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul (SKULLSPEED, finecosine[an]);
    actor->momy = FixedMul (SKULLSPEED, finesine[an]);
    dist = P_ApproxDistance (dest->x - actor->x, dest->y - actor->y);
    dist = dist / SKULLSPEED;
    
    if (dist < 1)
	dist = 1;
    actor->momz = (dest->z+(dest->height>>1) - actor->z) / dist;
}


//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void
A_PainShootSkull
( mobj_t*	actor,
  angle_t	angle )
{
    fixed_t	x;
    fixed_t	y;
    fixed_t	z;
    
    mobj_t*	newmobj;
    angle_t	an;
    int		prestep;
    int		count;
    thinker_t*	currentthinker;

    // count total number of skull currently on the level
    count = 0;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	if (   (currentthinker->function == P_MobjThinker)
	    && ((mobj_t *)currentthinker)->type == MT_SKULL)
	    count++;
	currentthinker = currentthinker->next;
    }

    // if there are allready 20 skulls on the level,
    // don't spit another one
    if (count > 20)
	return;


    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;
    
    prestep =
	4*FRACUNIT
	+ 3*(actor->info->radius + mobjinfo[MT_SKULL].radius)/2;
    
    x = actor->x + FixedMul (prestep, finecosine[an]);
    y = actor->y + FixedMul (prestep, finesine[an]);
    z = actor->z + 8*FRACUNIT;
		
    newmobj = P_SpawnMobj (x , y, z, MT_SKULL);

    // Check for movements.
	// killough $dropoff_fix
    if (!P_TryMove (newmobj, newmobj->x, newmobj->y, false))
    {
	// kill it immediately
	P_DamageMobj (newmobj,actor,actor,10000);	
	return;
    }
		
    newmobj->target = actor->target;
    A_SkullAttack (newmobj);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
void C_DECL A_PainAttack (mobj_t* actor)
{
    if (!actor->target)
	return;

    A_FaceTarget (actor);
    A_PainShootSkull (actor, actor->angle);
}


void C_DECL A_PainDie (mobj_t* actor)
{
    A_Fall (actor);
    A_PainShootSkull (actor, actor->angle+ANG90);
    A_PainShootSkull (actor, actor->angle+ANG180);
    A_PainShootSkull (actor, actor->angle+ANG270);
}






void C_DECL A_Scream (mobj_t* actor)
{
    int		sound;
	
    switch (actor->info->deathsound)
    {
      case 0:
	return;
		
      case sfx_podth1:
      case sfx_podth2:
      case sfx_podth3:
	sound = sfx_podth1 + P_Random ()%3;
	break;
		
      case sfx_bgdth1:
      case sfx_bgdth2:
	sound = sfx_bgdth1 + P_Random ()%2;
	break;
	
      default:
	sound = actor->info->deathsound;
	break;
    }

    // Check for bosses.
    if (actor->type==MT_SPIDER
	|| actor->type == MT_CYBORG)
    {
		// full volume
		S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
    }
    else
		S_StartSound(sound, actor);
}


void C_DECL A_XScream (mobj_t* actor)
{
    S_StartSound(sfx_slop, actor);	
}

void C_DECL A_Pain (mobj_t* actor)
{
    if (actor->info->painsound)
		S_StartSound(actor->info->painsound, actor);	
}

void C_DECL A_Fall (mobj_t *actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}


//
// A_Explode
//
void C_DECL A_Explode (mobj_t* thingy)
{
    P_RadiusAttack ( thingy, thingy->target, 128 );
}


//
// A_BossDeath
// Possibly trigger special effects
// if on first boss level
//
void C_DECL A_BossDeath (mobj_t* mo)
{
    thinker_t*	th;
    mobj_t*	mo2;
    line_t	junk;
    int		i;
	
    if ( gamemode == commercial)
    {
		if (gamemap != 7)
			return;
		if ((mo->type != MT_FATSO) && (mo->type != MT_BABY))
			return;
	}
    else
    {
		switch(gameepisode)
		{
		case 1:
			if (gamemap != 8)
				return;
			
			if (mo->type != MT_BRUISER)
				return;
			break;
			
		case 2:
			if (gamemap != 8)
				return;
			
			if (mo->type != MT_CYBORG)
				return;
			break;
			
		case 3:
			if (gamemap != 8)
				return;
			
			if (mo->type != MT_SPIDER)
				return;
			
			break;
			
		case 4:
			switch(gamemap)
			{
			case 6:
				if (mo->type != MT_CYBORG)
					return;
				break;
				
			case 8: 
				if (mo->type != MT_SPIDER)
					return;
				break;
				
			default:
				return;
				break;
			}
			break;
			
			default:
				if (gamemap != 8)
					return;
				break;
		}
		
    }
	
    
    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
		if (players[i].plr->ingame && players[i].health > 0)
			break;
		
	if (i==MAXPLAYERS)
		return;	// no one left alive, so do not end game
	
	// scan the remaining thinkers to see
	// if all bosses are dead
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function != P_MobjThinker)
			continue;
		
		mo2 = (mobj_t *)th;
		if (mo2 != mo
			&& mo2->type == mo->type
			&& mo2->health > 0)
		{
			// other boss not dead
			return;
		}
	}

	// victory!
	if ( gamemode == commercial)
	{
		if (gamemap == 7)
		{
			if (mo->type == MT_FATSO)
			{
				junk.tag = 666;
				EV_DoFloor(&junk,lowerFloorToLowest);
				return;
			}
			
			if (mo->type == MT_BABY)
			{
				junk.tag = 667;
				EV_DoFloor(&junk,raiseToTexture);
				return;
			}
		}
	}
	else
	{
		switch(gameepisode)
		{
		case 1:
			junk.tag = 666;
			EV_DoFloor (&junk, lowerFloorToLowest);
			return;
			break;
			
		case 4:
			switch(gamemap)
			{
			case 6:
				junk.tag = 666;
				EV_DoDoor (&junk, blazeOpen);
				return;
				break;
				
			case 8:
				junk.tag = 666;
				EV_DoFloor (&junk, lowerFloorToLowest);
				return;
				break;
			}
		}
	}
	
	G_ExitLevel ();
}


void C_DECL A_Hoof (mobj_t* mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    S_StartSound(sfx_hoof | (gamemode != commercial && gamemap == 8
		? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase (mo);
}

void C_DECL A_Metal (mobj_t* mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    S_StartSound(sfx_metal | (gamemode != commercial && gamemap == 8
		? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase (mo);
}

void C_DECL A_BabyMetal (mobj_t* mo)
{
    S_StartSound(sfx_bspwlk, mo);
    A_Chase (mo);
}

void C_DECL
A_OpenShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound(sfx_dbopn, player->plr->mo);
}

void C_DECL
A_LoadShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound(sfx_dbload, player->plr->mo);
}

void C_DECL A_ReFire( player_t*	player,  pspdef_t*	psp );

void C_DECL
A_CloseShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound(sfx_dbcls, player->plr->mo);
    A_ReFire(player,psp);
}



mobj_t*	braintargets[64];
int		numbraintargets;
int		braintargeton;

void C_DECL A_BrainAwake (mobj_t* mo)
{
    thinker_t*	thinker;
    mobj_t*	m;
	
    // find all the target spots
    numbraintargets = 0;
    braintargeton = 0;
	
    for (thinker = thinkercap.next ; thinker != &thinkercap ;
		thinker = thinker->next)
    {
		if (thinker->function != P_MobjThinker)
			continue;	// not a mobj
		
		m = (mobj_t *)thinker;
		
		if (m->type == MT_BOSSTARGET )
		{
			braintargets[numbraintargets] = m;
			numbraintargets++;
		}
    }
	
    S_StartSound(sfx_bossit, NULL);
}


void C_DECL A_BrainPain (mobj_t*	mo)
{
    S_StartSound(sfx_bospn, NULL);
}


void C_DECL A_BrainScream (mobj_t*	mo)
{
    int		x;
    int		y;
    int		z;
    mobj_t*	th;
	
    for (x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
    {
		y = mo->y - 320*FRACUNIT;
		z = 128 + P_Random()*2*FRACUNIT;
		th = P_SpawnMobj (x,y,z, MT_ROCKET);
		th->momz = P_Random()*512;
		
		P_SetMobjState (th, S_BRAINEXPLODE1);
		
		th->tics -= P_Random()&7;
		if (th->tics < 1)
			th->tics = 1;
    }
	
    S_StartSound(sfx_bosdth, NULL);
}



void C_DECL A_BrainExplode (mobj_t* mo)
{
    int		x;
    int		y;
    int		z;
    mobj_t*	th;
	
    x = mo->x + (P_Random () - P_Random ())*2048;
    y = mo->y;
    z = 128 + P_Random()*2*FRACUNIT;
    th = P_SpawnMobj (x,y,z, MT_ROCKET);
    th->momz = P_Random()*512;

    P_SetMobjState (th, S_BRAINEXPLODE1);

    th->tics -= P_Random()&7;
    if (th->tics < 1)
	th->tics = 1;
}


void C_DECL A_BrainDie (mobj_t*	mo)
{
    G_ExitLevel ();
}

void C_DECL A_BrainSpit (mobj_t*	mo)
{
    mobj_t*	targ;
    mobj_t*	newmobj;
    static int easy = 0;
	
    easy ^= 1;
    if (gameskill <= sk_easy && (!easy)) return;
	
    // shoot a cube at current target
    targ = braintargets[braintargeton];
    braintargeton = (braintargeton+1) % numbraintargets;
	
    // spawn brain missile
    newmobj = P_SpawnMissile (mo, targ, MT_SPAWNSHOT);
    newmobj->target = targ;
    newmobj->reactiontime =
		((targ->y - mo->y)/newmobj->momy) / newmobj->state->tics;
	
    S_StartSound(sfx_bospit, NULL);
}


void C_DECL A_SpawnFly (mobj_t* mo);

// travelling cube sound
void C_DECL A_SpawnSound (mobj_t* mo)	
{
    S_StartSound(sfx_boscub, mo);
    A_SpawnFly(mo);
}

void C_DECL A_SpawnFly (mobj_t* mo)
{
    mobj_t*	newmobj;
    mobj_t*	fog;
    mobj_t*	targ;
    int		r;
    mobjtype_t	type;
	
    if (--mo->reactiontime) return;	// still flying
	
    targ = mo->target;
	
    // First spawn teleport fog.
    fog = P_SpawnMobj (targ->x, targ->y, targ->z, MT_SPAWNFIRE);
    S_StartSound(sfx_telept, fog);
	
    // Randomly select monster to spawn.
    r = P_Random ();
	
    // Probability distribution (kind of :),
    // decreasing likelihood.
    if ( r<50 )
		type = MT_TROOP;
    else if (r<90)
		type = MT_SERGEANT;
    else if (r<120)
		type = MT_SHADOWS;
    else if (r<130)
		type = MT_PAIN;
    else if (r<160)
		type = MT_HEAD;
    else if (r<162)
		type = MT_VILE;
    else if (r<172)
		type = MT_UNDEAD;
    else if (r<192)
		type = MT_BABY;
    else if (r<222)
		type = MT_FATSO;
    else if (r<246)
		type = MT_KNIGHT;
    else
		type = MT_BRUISER;		
	
    newmobj	= P_SpawnMobj (targ->x, targ->y, targ->z, type);
    if (P_LookForPlayers (newmobj, true) )
		P_SetMobjState (newmobj, newmobj->info->seestate);
	
    // telefrag anything in this spot
    P_TeleportMove (newmobj, newmobj->x, newmobj->y);
	
    // remove self (i.e., cube).
    P_RemoveMobj (mo);
}



void C_DECL A_PlayerScream (mobj_t* mo)
{
    // Default death sound.
    int		sound = sfx_pldeth;
	
    if ( (gamemode == commercial)
	&& 	(mo->health < -50))
    {
	// IF THE PLAYER DIES
	// LESS THAN -50% WITHOUT GIBBING
	sound = sfx_pdiehi;
    }
    
    S_StartSound(sound, mo);
}



