
//**************************************************************************
//**
//** p_mobj.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#ifdef WIN32
#pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include "h2def.h"
#include "p_local.h"
#include "sounds.h"
#include "soundst.h"
#include "settings.h"
#include "m_bams.h"

// MACROS ------------------------------------------------------------------

#define MAX_TID_COUNT 200

#define MAX_BOB_OFFSET	0x80000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void G_PlayerReborn(int player);
void P_MarkAsLeaving(mobj_t *corpse);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void P_SpawnMapThing(mapthing_t *mthing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PlayerLandedOnThing(mobj_t *mo, mobj_t *onmobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern mobj_t LavaInflictor;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobjtype_t PuffType;
mobj_t *MissileMobj;

fixed_t FloatBobOffsets[64] =
{
	0, 51389, 102283, 152192,
	200636, 247147, 291278, 332604,
	370727, 405280, 435929, 462380,
	484378, 501712, 514213, 521763,
	524287, 521763, 514213, 501712,
	484378, 462380, 435929, 405280,
	370727, 332604, 291278, 247147,
	200636, 152192, 102283, 51389,
	-1, -51390, -102284, -152193,
	-200637, -247148, -291279, -332605,
	-370728, -405281, -435930, -462381,
	-484380, -501713, -514215, -521764,
	-524288, -521764, -514214, -501713,
	-484379, -462381, -435930, -405280,
	-370728, -332605, -291279, -247148,
	-200637, -152193, -102284, -51389
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int TIDList[MAX_TID_COUNT+1]; // +1 for termination marker
static mobj_t *TIDMobj[MAX_TID_COUNT];

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_SetMobjState
//
// Returns true if the mobj is still present.
//
//==========================================================================

boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
	state_t *st;

	if(state == S_NULL)
	{ // Remove mobj
		mobj->state = (state_t*) S_NULL;
		P_RemoveMobj(mobj);
		return(false);
	}
	st = &states[state];
/*	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;*/
	P_SetState(mobj, state);
	mobj->turntime = false; // $visangle-facetarget
	if(st->action)
	{ // Call action function
		st->action(mobj);
	}
	// Return false if the action function removed the mobj.
	return mobj->thinker.function != (think_t) -1;
}

//==========================================================================
//
// P_SetMobjStateNF
//
// Same as P_SetMobjState, but does not call the state function.
//
//==========================================================================

boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
//	state_t *st;

	if(state == S_NULL)
	{ // Remove mobj
		mobj->state = (state_t*) S_NULL;
		P_RemoveMobj(mobj);
		return(false);
	}
/*	st = &states[state];
	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;*/
	mobj->turntime = false; // $visangle-facetarget
	P_SetState(mobj, state);
	return(true);
}

//----------------------------------------------------------------------------
//
// PROC P_ExplodeMissile
//
//----------------------------------------------------------------------------

void P_ExplodeMissile(mobj_t *mo)
{
	mo->momx = mo->momy = mo->momz = 0;
	P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
	//mo->tics -= P_Random()&3;
	if(mo->flags & MF_MISSILE)
	{
		mo->flags &= ~MF_MISSILE;
		mo->flags |= MF_VIEWALIGN;
		if(mo->flags & MF_BRIGHTEXPLODE) mo->flags |= MF_BRIGHTSHADOW;
	}

	switch(mo->type)
	{
		case MT_SORCBALL1:
		case MT_SORCBALL2:
		case MT_SORCBALL3:
			S_StartSound(SFX_SORCERER_BIGBALLEXPLODE, NULL);
			break;
		case MT_SORCFX1:
			S_StartSound(SFX_SORCERER_HEADSCREAM, NULL);
			break;
		default:
			if(mo->info->deathsound)
			{
				S_StartSound(mo->info->deathsound, mo);
			}
			break;
	}
}

//----------------------------------------------------------------------------
//
// PROC P_FloorBounceMissile
//
//----------------------------------------------------------------------------

void P_FloorBounceMissile(mobj_t *mo)
{
	if(P_HitFloor(mo) >= FLOOR_LIQUID)
	{
		switch(mo->type)
		{
			case MT_SORCFX1:
			case MT_SORCBALL1:
			case MT_SORCBALL2:
			case MT_SORCBALL3:
				break;
			default:
				P_RemoveMobj(mo);
				return;
		}
	}
	switch(mo->type)
	{
		case MT_SORCFX1:
			mo->momz = -mo->momz;		// no energy absorbed
			break;
		case MT_SGSHARD1:
		case MT_SGSHARD2:
		case MT_SGSHARD3:
		case MT_SGSHARD4:
		case MT_SGSHARD5:
		case MT_SGSHARD6:
		case MT_SGSHARD7:
		case MT_SGSHARD8:
		case MT_SGSHARD9:
		case MT_SGSHARD0:
			mo->momz = FixedMul(mo->momz, -0.3*FRACUNIT);
			if(abs(mo->momz) < (FRACUNIT/2))
			{
				P_SetMobjState(mo, S_NULL);
				return;
			}
			break;
		default:
			mo->momz = FixedMul(mo->momz, -0.7*FRACUNIT);
			break;
	}
	mo->momx = 2*mo->momx/3;
	mo->momy = 2*mo->momy/3;
	if(mo->info->seesound)
	{
		switch(mo->type)
		{
			case MT_SORCBALL1:
			case MT_SORCBALL2:
			case MT_SORCBALL3:
				if (!mo->args[0]) S_StartSound(mo->info->seesound, mo);
				break;
			default:
				S_StartSound(mo->info->seesound, mo);
				break;
		}
		S_StartSound(mo->info->seesound, mo);
	}
//	P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}

//----------------------------------------------------------------------------
//
// PROC P_ThrustMobj
//
//----------------------------------------------------------------------------

void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;
	mo->momx += FixedMul(move, finecosine[angle]);
	mo->momy += FixedMul(move, finesine[angle]);
}

//----------------------------------------------------------------------------
//
// FUNC P_FaceMobj
//
// Returns 1 if 'source' needs to turn clockwise, or 0 if 'source' needs
// to turn counter clockwise.  'delta' is set to the amount 'source'
// needs to turn.
//
//----------------------------------------------------------------------------

int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta)
{
	angle_t diff;
	angle_t angle1;
	angle_t angle2;

	angle1 = source->angle;
	angle2 = R_PointToAngle2(source->x, source->y, target->x, target->y);
	if(angle2 > angle1)
	{
		diff = angle2-angle1;
		if(diff > ANGLE_180)
		{
			*delta = ANGLE_MAX-diff;
			return(0);
		}
		else
		{
			*delta = diff;
			return(1);
		}
	}
	else
	{
		diff = angle1-angle2;
		if(diff > ANGLE_180)
		{
			*delta = ANGLE_MAX-diff;
			return(1);
		}
		else
		{
			*delta = diff;
			return(0);
		}
	}
}

//----------------------------------------------------------------------------
//
//
// The missile special1 field must be mobj_t *target.  Returns true if
// target was tracked, false if not.
//
//----------------------------------------------------------------------------

boolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	mobj_t *target;

	target = (mobj_t *)actor->special1;
	if(target == NULL)
	{
		return(false);
	}
	if(!(target->flags&MF_SHOOTABLE))
	{ // Target died
		actor->special1 = 0;
		return(false);
	}
	dir = P_FaceMobj(actor, target, &delta);
	if(delta > thresh)
	{
		delta >>= 1;
		if(delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if(dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul(actor->info->speed, finecosine[angle]);
	actor->momy = FixedMul(actor->info->speed, finesine[angle]);
	if(actor->z+actor->height < target->z 
		|| target->z+target->height < actor->z)
	{ // Need to seek vertically
		dist = P_ApproxDistance(target->x-actor->x, target->y-actor->y);
		dist = dist/actor->info->speed;
		if(dist < 1)
		{
			dist = 1;
		}
		actor->momz = (target->z+(target->height>>1)
			-(actor->z+(actor->height>>1)))/dist;
	}
	return(true);
}

#define FRICTION_NORMAL		0xe800
#define FRICTION_LOW		0xf900
#define FRICTION_FLY		0xeb00

//--------------------------------------------------------------------------
// P_GetMobjFriction
//--------------------------------------------------------------------------
int P_GetMobjFriction(mobj_t *mo)
{
	if(mo->flags2 & MF2_FLY && !(mo->z <= mo->floorz)
		&& !(mo->flags2 & MF2_ONMOBJ))
	{
		return FRICTION_FLY;
	}
	else if(P_GetThingFloorType(mo) == FLOOR_ICE)
	{
		return FRICTION_LOW;
	}
	return FRICTION_NORMAL;
}

//----------------------------------------------------------------------------
//
// PROC P_XYMovement
//
//----------------------------------------------------------------------------

#define STOPSPEED			0x1000

void P_XYMovement(mobj_t *mo)
{
	fixed_t ptryx, ptryy;
	player_t *player;
	fixed_t xmove, ymove;
	int special;
	angle_t angle;
	static int windTab[3] = {2048*5, 2048*10, 2048*25};

	// $democam: cameramen have their own movement code
	if(P_CameraXYMovement(mo)) return;

	if(!mo->momx && !mo->momy)
	{
		if(mo->flags&MF_SKULLFLY)
		{ // A flying mobj slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->momx = mo->momy = mo->momz = 0;
			P_SetMobjState(mo, mo->info->seestate);
		}
		return;
	}
	special = mo->subsector->sector->special;
	if(mo->flags2&MF2_WINDTHRUST)
	{
		switch(special)
		{
			case 40: case 41: case 42: // Wind_East
				P_ThrustMobj(mo, 0, windTab[special-40]);
				break;
			case 43: case 44: case 45: // Wind_North
				P_ThrustMobj(mo, ANG90, windTab[special-43]);
				break;
			case 46: case 47: case 48: // Wind_South
				P_ThrustMobj(mo, ANG270, windTab[special-46]);
				break;
			case 49: case 50: case 51: // Wind_West
				P_ThrustMobj(mo, ANG180, windTab[special-49]);
				break;
		}
	}
	player = mo->player;
	
	if(mo->momx > MAXMOVE)
	{
		mo->momx = MAXMOVE;
	}
	else if(mo->momx < -MAXMOVE)
	{
		mo->momx = -MAXMOVE;
	}

	if(mo->momy > MAXMOVE)
	{
		mo->momy = MAXMOVE;
	}
	else if(mo->momy < -MAXMOVE)
	{
		mo->momy = -MAXMOVE;
	}

	xmove = mo->momx;
	ymove = mo->momy;
	do
	{
		if(xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
		{
#if 1
			ptryx = mo->x+xmove/2;
			ptryy = mo->y+ymove/2;
			xmove >>= 1;
			ymove >>= 1;
#else
			xmove >>= 1;
			ymove >>= 1;
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
#endif
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}
		if(!P_TryMove(mo, ptryx, ptryy))
		{ // Blocked move
			if(mo->flags2&MF2_SLIDE)
			{ // Try to slide along it
				if(BlockingMobj == NULL)
				{ // Slide against wall
					P_SlideMove(mo);
				}
				else
				{ // Slide against mobj
					//if(P_TryMove(mo, mo->x, mo->y+mo->momy))
					if(P_TryMove(mo, mo->x, ptryy))
					{
						mo->momx = 0;
					}
					//else if(P_TryMove(mo, mo->x+mo->momx, mo->y))
					else if(P_TryMove(mo, ptryx, mo->y))
					{
						mo->momy = 0;
					}
					else
					{
						mo->momx = mo->momy = 0;
					}
				}
			}
			else if(mo->flags&MF_MISSILE)
			{ 
				if(mo->flags2&MF2_FLOORBOUNCE)
				{
					if(BlockingMobj)
					{
						if ((BlockingMobj->flags2&MF2_REFLECTIVE) ||
							((!BlockingMobj->player) &&
							(!(BlockingMobj->flags&MF_COUNTKILL))))
						{
							fixed_t speed;
	
							angle = R_PointToAngle2(BlockingMobj->x,
								BlockingMobj->y, mo->x, mo->y)
								+ANGLE_1*((P_Random()%16)-8);
							speed = P_ApproxDistance(mo->momx, mo->momy);
							speed = FixedMul(speed, 0.75*FRACUNIT);
							mo->angle = angle;
							angle >>= ANGLETOFINESHIFT;
							mo->momx = FixedMul(speed, finecosine[angle]);
							mo->momy = FixedMul(speed, finesine[angle]);
							if(mo->info->seesound)
							{
								S_StartSound(mo->info->seesound, mo);
							}
							return;
						}
						else
						{ // Struck a player/creature
 							P_ExplodeMissile(mo);
						}
					}
					else
					{ // Struck a wall
						P_BounceWall(mo);
						switch(mo->type)
						{
							case MT_SORCBALL1:
							case MT_SORCBALL2:
							case MT_SORCBALL3:
							case MT_SORCFX1:
								break;
							default:
								if(mo->info->seesound)
								{
									S_StartSound(mo->info->seesound, mo);
								}
								break;
						}
						return;
					}	
				}
				if(BlockingMobj &&
					(BlockingMobj->flags2 & MF2_REFLECTIVE))
				{
					angle = R_PointToAngle2(BlockingMobj->x,
													BlockingMobj->y,
													mo->x, mo->y);

					// Change angle for delflection/reflection
					switch(BlockingMobj->type)
					{
						case MT_CENTAUR:
						case MT_CENTAURLEADER:
							if ( abs(angle-BlockingMobj->angle)>>24 > 45)
								goto explode;
							if (mo->type == MT_HOLY_FX)
								goto explode;
								// Drop through to sorcerer full reflection
						case MT_SORCBOSS:
							// Deflection
							if (P_Random()<128)
								angle += ANGLE_45;
							else
								angle -= ANGLE_45;
							break;
						default:
							// Reflection
							angle += ANGLE_1 * ((P_Random()%16)-8);
							break;
					}

					// Reflect the missile along angle
					mo->angle = angle;
					angle >>= ANGLETOFINESHIFT;
					mo->momx = FixedMul(mo->info->speed>>1, finecosine[angle]);
					mo->momy = FixedMul(mo->info->speed>>1, finesine[angle]);
//					mo->momz = -mo->momz;
					if (mo->flags2 & MF2_SEEKERMISSILE)
					{
						mo->special1 = (int)(mo->target);
					}
					mo->target = BlockingMobj;
					return;
				}
explode:
				// Explode a missile
				if(ceilingline && ceilingline->backsector
					&& ceilingline->backsector->ceilingpic == skyflatnum)
				{ // Hack to prevent missiles exploding against the sky
					if(mo->type == MT_BLOODYSKULL)
					{
						mo->momx = mo->momy = 0;
						mo->momz = -FRACUNIT;
					}
					else if(mo->type == MT_HOLY_FX)
					{
						P_ExplodeMissile(mo);
					}
					else
					{
						P_RemoveMobj(mo);
					}
					return;
				}
				P_ExplodeMissile(mo);
			}
			//else if(mo->info->crashstate)
			//{
			//	mo->momx = mo->momy = 0;
			//	P_SetMobjState(mo, mo->info->crashstate);
			//	return;
			//}
			else
			{
				mo->momx = mo->momy = 0;
			}
		}
	} while(xmove || ymove);

	// Friction

	if(player && player->cheats&CF_NOMOMENTUM)
	{ // Debug option for no sliding at all
		mo->momx = mo->momy = 0;
		return;
	}
	if(mo->flags&(MF_MISSILE|MF_SKULLFLY))
	{ // No friction for missiles
		return;
	}
	if(mo->z > mo->floorz && !(mo->flags2&MF2_FLY) && !(mo->flags2&MF2_ONMOBJ))
	{ // No friction when falling
		if (mo->type != MT_BLASTEFFECT)
		return;
	}
	if(mo->flags&MF_CORPSE)
	{ // Don't stop sliding if halfway off a step with some momentum
		if(mo->momx > FRACUNIT/4 || mo->momx < -FRACUNIT/4
			|| mo->momy > FRACUNIT/4 || mo->momy < -FRACUNIT/4)
		{
			if(mo->floorz != mo->subsector->sector->floorheight)
			{
				return;
			}
		}
	}
	if(mo->momx > -STOPSPEED && mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED && mo->momy < STOPSPEED
		&& (!player || (player->cmd.forwardmove == 0
		&& player->cmd.sidemove == 0)))
	{ // If in a walking frame, stop moving
		if(player)
		{
			if((unsigned)((player->plr->mo->state-states)
				-PStateRun[player->class]) < 4)
			{
				P_SetMobjState(player->plr->mo, PStateNormal[player->class]);
			}
		}
		mo->momx = 0;
		mo->momy = 0;
	}
	else
	{
		int friction = P_GetMobjFriction(mo);
		mo->momx = FixedMul(mo->momx, friction);
		mo->momy = FixedMul(mo->momy, friction);
	}
}


// Move this to p_inter ***
void P_MonsterFallingDamage(mobj_t *mo)
{
	int damage;
	int mom;

	mom = abs(mo->momz);
	if(mom > 35*FRACUNIT)
	{ // automatic death
		damage=10000;
	}
	else
	{
		damage = ((mom - (23*FRACUNIT) )*6)>>FRACBITS;
	}
	damage=10000;	// always kill 'em
	P_DamageMobj(mo, NULL, NULL, damage);
}



/*
===============
=
= P_ZMovement
=
===============
*/

void P_ZMovement(mobj_t *mo)
{
	int dist;
	int delta;
//	extern boolean demorecording;

	// $democam: cameramen get special z movement
	if(P_CameraZMovement(mo)) return;		

//
// check for smooth step up
//
	if (mo->player && mo->z < mo->floorz)
	{
		mo->player->plr->viewheight -= mo->floorz-mo->z;
		mo->player->plr->deltaviewheight = (VIEWHEIGHT - mo->player->plr->viewheight)>>3;
	}	
//
// adjust height
//
	mo->z += mo->momz;
	if(mo->flags&MF_FLOAT && mo->target)
	{	// float down towards target if too close
		if(!(mo->flags&MF_SKULLFLY) && !(mo->flags&MF_INFLOAT))
		{
			dist = P_ApproxDistance(mo->x-mo->target->x, mo->y-mo->target->y);
			delta =( mo->target->z+(mo->height>>1))-mo->z;
			if (delta < 0 && dist < -(delta*3))
			{
				mo->z -= FLOATSPEED;
				P_SetThingSRVOZ(mo, -FLOATSPEED);
			}
			else if (delta > 0 && dist < (delta*3))
			{
				mo->z += FLOATSPEED;			
				P_SetThingSRVOZ(mo, FLOATSPEED);
			}
		}
	}
	if(mo->player && mo->flags2&MF2_FLY && !(mo->z <= mo->floorz)
		&& leveltime&2)
	{
		mo->z += finesine[(FINEANGLES/20*leveltime>>2)&FINEMASK];
	}

//
// clip movement
//
	if(mo->z <= mo->floorz)
	{	// Hit the floor
		if(mo->flags&MF_MISSILE)
		{
			mo->z = mo->floorz;
			if(mo->flags2&MF2_FLOORBOUNCE)
			{
				P_FloorBounceMissile(mo);
				return;
			}
			else if(mo->type == MT_HOLY_FX)
			{ // The spirit struck the ground
				mo->momz = 0;
				P_HitFloor(mo);
				return;
			}
			else if(mo->type == MT_MNTRFX2 || mo->type == MT_LIGHTNING_FLOOR)
			{ // Minotaur floor fire can go up steps
				return;
			}
			else
			{
				P_HitFloor(mo);
				P_ExplodeMissile(mo);
				return;
			}
		}
		if(mo->flags&MF_COUNTKILL)		// Blasted mobj falling
		{
			if(mo->momz < -(23*FRACUNIT))
			{
				P_MonsterFallingDamage(mo);
			}
		}
		if(mo->z-mo->momz > mo->floorz)
		{ // Spawn splashes, etc.
			P_HitFloor(mo);
		}
		mo->z = mo->floorz;
		if(mo->momz < 0)
		{
			if(mo->flags2&MF2_ICEDAMAGE && mo->momz < -GRAVITY*8)
			{
				mo->tics = 1;
				mo->momx = 0;
				mo->momy = 0;
				mo->momz = 0;
				return;
			}	
			if(mo->player)
			{
				mo->player->jumpTics = 7;// delay any jumping for a short time
				if(mo->momz < -GRAVITY*8 && !(mo->flags2&MF2_FLY))
				{ // squat down
					mo->player->plr->deltaviewheight = mo->momz>>3;
					if(mo->momz < -23*FRACUNIT)
					{
						P_FallingDamage(mo->player);
						P_NoiseAlert(mo, mo);
					}
					else if(mo->momz < -GRAVITY*12 && !mo->player->morphTics)
					{
						S_StartSound(SFX_PLAYER_LAND, mo);
						switch(mo->player->class)
						{
							case PCLASS_FIGHTER:
								S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
								break;
							case PCLASS_CLERIC:
								S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
								break;
							case PCLASS_MAGE:
								S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
								break;
							default:
								break;
						}
					}
					else if ((P_GetThingFloorType(mo) < FLOOR_LIQUID) && 
						(!mo->player->morphTics))
					{
						S_StartSound(SFX_PLAYER_LAND, mo);
					}
/*#ifdef __WATCOMC__
					if(!useexterndriver)
					{
						mo->player->centering = true;
					}
#else*/
					// If mouselook is active, we don't want lookspring.
					if(!cfg.usemlook)// || demorecording || demoplayback)
						mo->player->centering = true;
//#endif
				}
			}
			else if(mo->type >= MT_POTTERY1 
				&& mo->type <= MT_POTTERY3)
			{
				P_DamageMobj(mo, NULL, NULL, 25);
			}
			else if(mo->flags&MF_COUNTKILL)
			{
				if(mo->momz < -23*FRACUNIT)
				{
					// Doesn't get here
				}
			}
			mo->momz = 0;
		}
		if(mo->flags&MF_SKULLFLY)
		{ // The skull slammed into something
			mo->momz = -mo->momz;
		}
		if(mo->info->crashstate &&
			(mo->flags&MF_CORPSE) && 
			!(mo->flags2&MF2_ICEDAMAGE))
		{
			P_SetMobjState(mo, mo->info->crashstate);
			return;
		}
	}
	else if(mo->flags2&MF2_LOGRAV)
	{
		if(mo->momz == 0)
			mo->momz = -(GRAVITY>>3)*2;
		else
			mo->momz -= GRAVITY>>3;
	}
	else if (! (mo->flags & MF_NOGRAVITY) )
	{
		if (mo->momz == 0)
			mo->momz = -GRAVITY*2;
		else
			mo->momz -= GRAVITY;
	}
	
	if (mo->z + mo->height > mo->ceilingz)
	{	// hit the ceiling
		if (mo->momz > 0)
			mo->momz = 0;
		mo->z = mo->ceilingz - mo->height;
		if(mo->flags2&MF2_FLOORBOUNCE)
		{
			// Maybe reverse momentum here for ceiling bounce
			// Currently won't happen

			if(mo->info->seesound)
			{
				S_StartSound(mo->info->seesound, mo);
			}
			return;
		}	
		if (mo->flags & MF_SKULLFLY)
		{	// the skull slammed into something
			mo->momz = -mo->momz;
		}
		if(mo->flags&MF_MISSILE)
		{
			if(mo->type == MT_LIGHTNING_CEILING)
			{
				return;
			}
			if(mo->subsector->sector->ceilingpic == skyflatnum)
			{
				if(mo->type == MT_BLOODYSKULL)
				{
					mo->momx = mo->momy = 0;
					mo->momz = -FRACUNIT;
				}
				else if(mo->type == MT_HOLY_FX)
				{
					P_ExplodeMissile(mo);
				}
				else
				{
					P_RemoveMobj(mo);
				}
				return;
			}
			P_ExplodeMissile(mo);
			return;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_BlasterMobjThinker
//
//
//----------------------------------------------------------------------------

void P_BlasterMobjThinker(mobj_t *mobj)
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	fixed_t z;
	boolean changexy;
	mobj_t *mo;

#ifdef TIC_DEBUG
	FUNTAG("P_BlasterMobjThinker");

	if(rndDebugfile) fprintf(rndDebugfile, " -- z: %f, floorz: %f, mom: %f, %f, %f\n",
		FIX2FLT(mobj->z), FIX2FLT(mobj->floorz), FIX2FLT(mobj->momx), FIX2FLT(mobj->momy),
		FIX2FLT(mobj->momz));
#endif

	// Handle movement
	if(mobj->momx || mobj->momy ||
		(mobj->z != mobj->floorz) || mobj->momz)
	{
		xfrac = mobj->momx>>3;
		yfrac = mobj->momy>>3;
		zfrac = mobj->momz>>3;
		changexy = xfrac || yfrac;
		for(i = 0; i < 8; i++)
		{
			if(changexy)
			{
				if(!P_TryMove(mobj, mobj->x+xfrac, mobj->y+yfrac))
				{ // Blocked move
					P_ExplodeMissile(mobj);
#ifdef TIC_DEBUG
					FUNTAG(" + missile exploded");
#endif
					return;
				}
			}
			mobj->z += zfrac;
			if(mobj->z <= mobj->floorz)
			{ // Hit the floor
#ifdef TIC_DEBUG
				FUNTAG(" + floor hit");
#endif

				mobj->z = mobj->floorz;
				P_HitFloor(mobj);
				P_ExplodeMissile(mobj);
				return;
			}
			if(mobj->z+mobj->height > mobj->ceilingz)
			{ // Hit the ceiling
				mobj->z = mobj->ceilingz-mobj->height;
				P_ExplodeMissile(mobj);
#ifdef TIC_DEBUG
				FUNTAG(" + ceiling hit");
#endif
				return;
			}
			if(changexy)
			{
				if(mobj->type == MT_MWAND_MISSILE && (P_Random() < 128))
				{
					z = mobj->z-8*FRACUNIT;
					if(z < mobj->floorz)
					{
						z = mobj->floorz;
					}
					P_SpawnMobj(mobj->x, mobj->y, z, MT_MWANDSMOKE);
				}
				else if(!--mobj->special1)
				{
					mobj->special1 = 4;
					z = mobj->z-12*FRACUNIT;
					if(z < mobj->floorz)
					{
						z = mobj->floorz;
					}
					mo = P_SpawnMobj(mobj->x, mobj->y, z, MT_CFLAMEFLOOR);
					if(mo)
					{
						mo->angle = mobj->angle;
					}
				}
			}
		}
	}
	// Advance the state
	if(mobj->tics != -1)
	{
		mobj->tics--;
		while(!mobj->tics)
		{
			if(!P_SetMobjState(mobj, mobj->state->nextstate))
			{ // mobj was removed
#ifdef TIC_DEBUG
				FUNTAG(" + mobj removed");
#endif
				return;
			}
		}
	}
#ifdef TIC_DEBUG
	FUNTAG(" + end");
#endif
}

//===========================================================================
//
// PlayerLandedOnThing
//
//===========================================================================

static void PlayerLandedOnThing(mobj_t *mo, mobj_t *onmobj)
{
//	extern boolean demorecording;

	mo->player->plr->deltaviewheight = mo->momz>>3;
	if(mo->momz < -23*FRACUNIT)
	{
		P_FallingDamage(mo->player);
		P_NoiseAlert(mo, mo);
	}
	else if(mo->momz < -GRAVITY*12 
		&& !mo->player->morphTics)
	{
		S_StartSound(SFX_PLAYER_LAND, mo);
		switch(mo->player->class)
		{
			case PCLASS_FIGHTER:
				S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
				break;
			case PCLASS_CLERIC:
				S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
				break;
			case PCLASS_MAGE:
				S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
				break;
			default:
				break;
		}
	}
	else if(!mo->player->morphTics)
	{
		S_StartSound(SFX_PLAYER_LAND, mo);
	}
/*#ifdef __WATCOMC__
	if(!useexterndriver)
	{
		mo->player->centering = true;
	}
#else*/
	// Lookspring is stupid when mouselook is on (and not in demo).
	if(!cfg.usemlook)// || demorecording || demoplayback)
		mo->player->centering = true;
//#endif
}

//----------------------------------------------------------------------------
//
// PROC P_MobjThinker
//
//----------------------------------------------------------------------------

void P_MobjThinker(mobj_t *mobj)
{
	mobj_t *onmo;

	if(mobj->ddflags & DDMF_REMOTE)
	{
		// Remote mobjs are handled separately.
		return;
	}

	P_UpdateMobjFlags(mobj);

/*#ifdef _DEBUG
	mobj->translucency = M_Random();
#endif*/

	// The first three bits of the selector special byte contain a
	// relative health level.
	P_UpdateHealthBits(mobj);

	// Handle X and Y momentums
	BlockingMobj = NULL;
	if(mobj->momx || mobj->momy || (mobj->flags&MF_SKULLFLY))
	{
		P_XYMovement(mobj);
		if(mobj->thinker.function == (think_t)-1)
		{ // mobj was removed
			return;
		}
	}
	else if(mobj->flags2 & MF2_BLASTED)
	{ // Reset to not blasted when momentums are gone
		ResetBlasted(mobj);
	}
	if(mobj->flags2 & MF2_FLOATBOB)
	{ 
		// Floating item bobbing motion (special1 is height)
		/*mobj->z = mobj->floorz +
					mobj->special1 +
					FloatBobOffsets[(mobj->health++)&63];*/

		// Keep it on the floor.
		mobj->z = mobj->floorz;

		// Negative floorclip raises the mobj off the floor.
		mobj->floorclip = -mobj->special1;
		if(mobj->floorclip < -MAX_BOB_OFFSET)
		{
			// We don't want it going through the floor.
			mobj->floorclip = -MAX_BOB_OFFSET;
		}

		// Old floatbob used health as index, let's still increase it
		// as before (in case somebody wants to use it).
		mobj->health++;

		// HACK: If we are bobbing into the floor, raise special1 height.
		// Only affects Weapon Pieces.
		/*if(mobj->type >= MT_FW_SWORD1 
			&& mobj->type <= MT_MW_STAFF3
			&& mobj->z < mobj->floorz)
		{
			mobj->special1 += mobj->floorz - mobj->z;
		}*/
	}
	else if((mobj->z != mobj->floorz) || mobj->momz || BlockingMobj)
	{	// Handle Z momentum and gravity
		if(mobj->flags2&MF2_PASSMOBJ)
		{
			if(!(onmo = P_CheckOnmobj(mobj)))
			{
				P_ZMovement(mobj);
				if(mobj->player && mobj->flags&MF2_ONMOBJ)
				{
					mobj->flags2 &= ~MF2_ONMOBJ;
				}
			}
			else
			{
				if(mobj->player)
				{
					if(mobj->momz < -GRAVITY*8 && !(mobj->flags2&MF2_FLY))
					{
						PlayerLandedOnThing(mobj, onmo);
					}
					if(onmo->z+onmo->height-mobj->z <= 24*FRACUNIT)
					{
						mobj->player->plr->viewheight -= onmo->z+onmo->height
							-mobj->z;
						mobj->player->plr->deltaviewheight = 
							(VIEWHEIGHT-mobj->player->plr->viewheight)>>3;
						mobj->z = onmo->z+onmo->height;
						mobj->flags2 |= MF2_ONMOBJ;
						mobj->momz = 0;
					}				
					else
					{ // hit the bottom of the blocking mobj
						mobj->momz = 0;
					}
				}
/* Landing on another player, and mimicking his movements
				if(mobj->player && onmo->player)
				{
					mobj->momx = onmo->momx;
					mobj->momy = onmo->momy;
					if(onmo->z < onmo->floorz)
					{
						mobj->z += onmo->floorz-onmo->z;
						if(onmo->player)
						{
							onmo->player->plr->viewheight -= onmo->floorz-onmo->z;
							onmo->player->plr->deltaviewheight = (VIEWHEIGHT-
								onmo->player->plr->viewheight)>>3;
						}
						onmo->z = onmo->floorz;
					}
				}
*/
			}
		}
		else
		{
			P_ZMovement(mobj);
		}
		if(mobj->thinker.function == (think_t)-1)
		{ // mobj was removed
			return;
		}
	}

	// Cycle through states, calling action functions at transitions
	if(mobj->tics != -1)
	{
		int oldsprite = mobj->state->sprite;
		mobj->tics--;
		P_SRVOAngleTicker(mobj);
		// You can cycle through multiple states in a tic.
		while(!mobj->tics)
		{
			P_ClearThingSRVO(mobj);
			if(!P_SetMobjState(mobj, mobj->state->nextstate))
			{ // mobj was removed
				return;
			}
		}
		// See what the next frame is.
		/*if(mobj->state->nextstate != S_NULL)
		{
			state_t *st = &states[mobj->state->nextstate];
			if(oldsprite == st->sprite)
			{
				mobj->nextframe = st->frame;
			}
			else
			{
				mobj->nextframe = -1;
			}
			mobj->nexttime = mobj->tics;
		}*/
	}
	// Ice corpses aren't going anywhere.
	if(mobj->flags & MF_ICECORPSE) P_SetThingSRVO(mobj, 0, 0);
	/*else
	{
		// There definitely is no next frame in this case (tics==-1).
		mobj->nextframe = -1;
	}*/
}

//==========================================================================
//
// P_SpawnMobj
//
//==========================================================================

mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	mobj_t *mobj;
	//state_t *st;
	mobjinfo_t *info;
	fixed_t space;

	mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
	memset(mobj, 0, sizeof(*mobj));
	info = &mobjinfo[type];
	mobj->type = type;
	mobj->info = info;
	mobj->x = x;
	mobj->y = y;
	mobj->radius = info->radius;
	mobj->height = info->height;
	mobj->flags = info->flags;
	mobj->flags2 = info->flags2;
	mobj->damage = info->damage; // this doesn't appear to actually be used
                                 // see P_DamageMobj in P_inter.c

	// Let the engine know about solid objects.
	if(mobj->flags & MF_SOLID) mobj->ddflags |= DDMF_SOLID;
	if(mobj->flags2 & MF2_DONTDRAW) mobj->ddflags |= DDMF_DONTDRAW;

	mobj->health = info->spawnhealth 
		* (IS_NETGAME? cfg.netMobHealthModifier : 1);
	if(gameskill != sk_nightmare)
	{
		mobj->reactiontime = info->reactiontime;
	}
	mobj->lastlook = P_Random()%MAXPLAYERS;

	// Must link before setting state.
	mobj->thinker.function = P_MobjThinker;
	P_AddThinker(&mobj->thinker);
	
	P_SetState(mobj, info->spawnstate);

	// Set subsector and/or block links.
	P_SetThingPosition(mobj);
	mobj->floorz = mobj->subsector->sector->floorheight;
	mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	if(z == ONFLOORZ)
	{
		mobj->z = mobj->floorz;
	}
	else if(z == ONCEILINGZ)
	{
		mobj->z = mobj->ceilingz-mobj->info->height;
	}	
	else if(z == FLOATRANDZ)
	{
		space = ((mobj->ceilingz)-(mobj->info->height))-mobj->floorz;
		if(space > 48*FRACUNIT)
		{
			space -= 40*FRACUNIT;
			mobj->z = ((space*P_Random())>>8)+mobj->floorz+40*FRACUNIT;
		}
		else
		{
			mobj->z = mobj->floorz;
		}
	}
	else if (mobj->flags2&MF2_FLOATBOB)
	{
		mobj->z = mobj->floorz + z;		// artifact z passed in as height
	}
	else
	{
		mobj->z = z;
	}
	if(mobj->flags2&MF2_FLOORCLIP && P_GetThingFloorType(mobj) >= FLOOR_LIQUID
		&& mobj->z == mobj->subsector->sector->floorheight)
	{
		mobj->floorclip = 10*FRACUNIT;
	}
	else
	{
		mobj->floorclip = 0;
	}
	return(mobj);
}

//==========================================================================
//
// P_RemoveMobj
//
//==========================================================================

void P_RemoveMobj(mobj_t *mobj)
{
	// Remove from creature queue
	if(mobj->flags&MF_COUNTKILL &&
		mobj->flags&MF_CORPSE)
	{
		A_DeQueueCorpse(mobj);
	}

	if(mobj->tid)
	{ // Remove from TID list
		P_RemoveMobjFromTIDList(mobj);
	}

	// Unlink from sector and block lists
	P_UnsetThingPosition(mobj);

	// Stop any playing sound
	S_StopSound(0, mobj);

	// Free block
	P_RemoveThinker((thinker_t *)mobj);
}

//==========================================================================
//
// P_SpawnPlayer
//
// Called when a player is spawned on the level.  Most of the player
// structure stays unchanged between levels.
//
//==========================================================================

void P_SpawnPlayer(mapthing_t *mthing, int playernum)
{
	player_t *p;
	fixed_t x, y, z;
	mobj_t *mobj;

	if(!players[playernum].plr->ingame)
	{ // Not playing
		return;
	}
	p = &players[playernum];
	if(p->playerstate == PST_REBORN)
	{
		G_PlayerReborn(playernum);
	}
	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	z = ONFLOORZ;
	if(randomclass && deathmatch)
	{
		p->class = P_Random()%3;
		if(p->class == cfg.PlayerClass[playernum])
		{
			p->class = (p->class+1)%3;
		}
		cfg.PlayerClass[playernum] = p->class;
		SB_SetClassData();
		NetSv_SendPlayerInfo(playernum, DDSP_ALL_PLAYERS);
	}
	else
	{
		p->class = cfg.PlayerClass[playernum];
	}
	switch(p->class)
	{
		case PCLASS_FIGHTER:
			mobj = P_SpawnMobj(x, y, z, MT_PLAYER_FIGHTER);
			break;
		case PCLASS_CLERIC:
			mobj = P_SpawnMobj(x, y, z, MT_PLAYER_CLERIC);
			break;
		case PCLASS_MAGE:
			mobj = P_SpawnMobj(x, y, z, MT_PLAYER_MAGE);
			break;
		default:
			Con_Error("P_SpawnPlayer: Unknown class type");
			break;
	}
	
	// With clients all player mobjs are remote, even the consoleplayer.
	if(IS_CLIENT) 
	{
		mobj->flags &= ~MF_SOLID;
		mobj->ddflags = DDMF_REMOTE | DDMF_DONTDRAW;
		// The real flags are received from the server later on.
	}

	// Set translation table data.
	if(p->class == PCLASS_FIGHTER && (p->colormap == 0 || p->colormap == 2))
	{ 
		// The first type should be blue, and the third should be the
		// Fighter's original gold color
		//if(mthing->type == 1)
		if(p->colormap == 0)
		{
			mobj->flags |= 2<<MF_TRANSSHIFT;
		}
	}
	//else if(mthing->type > 1)
	else if(p->colormap > 0 && p->colormap < 8)
	{ // Set color translation bits for player sprites
		//mobj->flags |= (mthing->type-1)<<MF_TRANSSHIFT;
		mobj->flags |= p->colormap << MF_TRANSSHIFT;
	}
    p->plr->clAngle = mobj->angle = ANG45 * (mthing->angle/45);
	p->plr->clLookDir = 0;
	p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
	mobj->player = p;
	mobj->dplayer = p->plr;
	mobj->health = p->health;
	p->plr->mo = mobj;
	p->playerstate = PST_LIVE;
	p->refire = 0;
	P_ClearMessage(p);
	p->damagecount = 0;
	p->bonuscount = 0;
	p->poisoncount = 0;
	p->morphTics = 0;
	p->plr->extralight = 0;
	p->plr->fixedcolormap = 0;
	p->plr->viewheight = VIEWHEIGHT;
	p->plr->lookdir = 0;
	P_SetupPsprites(p);
	if(deathmatch)
	{ // Give all keys in death match mode
		p->keys = 2047;
	}
}

//==========================================================================
//
// P_SpawnMapThing
//
// The fields of the mapthing should already be in host byte order.
//
//==========================================================================

void P_SpawnMapThing(mapthing_t *mthing)
{
	int i;
	unsigned int spawnMask;
	mobj_t *mobj;
	fixed_t x, y, z;
	static unsigned int classFlags[] =
	{
		MTF_FIGHTER,
		MTF_CLERIC,
		MTF_MAGE
	};

	// Count deathmatch start positions
	if(mthing->type == 11)
	{
		if(deathmatch_p < &deathmatchstarts[MAXDEATHMATCHSTARTS])
		{
			memcpy(deathmatch_p, mthing, sizeof(*mthing));
			deathmatch_p++;
		}
		return;
	}
	if(mthing->type == PO_ANCHOR_TYPE)
	{ // Polyobj Anchor Pt.
		return;
	}
	else if(mthing->type == PO_SPAWN_TYPE
		|| mthing->type == PO_SPAWNCRUSH_TYPE)
	{ // Polyobj Anchor Pt.
		po_NumPolyobjs++;
		return;
	}

	// Check for player starts 1 to 4
	if(mthing->type <= 4)
	{
		/*
		if(mthing->arg1 < 0 || mthing->arg1 > MAX_PLAYER_STARTS)
		{
			Con_Message("P_SpawnMapThing: mthing->arg1 is really weird!"
				" (%d)\n", mthing->arg1);
		}
		*/
		P_RegisterPlayerStart(mthing);
		/*playerstarts[mthing->arg1][mthing->type-1] = *mthing;
		if(!deathmatch && !mthing->arg1)
		{
			P_SpawnPlayer(mthing, mthing->type-1);
		}*/
		return;
	}
	// Check for player starts 5 to 8
	if(mthing->type >= 9100 && mthing->type <= 9103)
	{
		mthing->type = 5+mthing->type-9100; // Translate to 5 - 8
		P_RegisterPlayerStart(mthing);
		/*playerstarts[mthing->arg1][mthing->type-1] = *mthing;
		if(!deathmatch && !mthing->arg1)
		{
			P_SpawnPlayer(mthing, mthing->type-1);
		}*/
		return;
	}

	if(mthing->type >= 1400 && mthing->type < 1410)
	{
		(R_PointInSubsector(mthing->x<<FRACBITS, 
			mthing->y<<FRACBITS))->sector->seqType = mthing->type-1400;
		return;
	}

	// Check current game type with spawn flags
	if(netgame == false)
	{
		spawnMask = MTF_GSINGLE;
	}
	else if(deathmatch)
	{
		spawnMask = MTF_GDEATHMATCH;
	}
	else
	{
		spawnMask = MTF_GCOOP;
	}
	if(!(mthing->options&spawnMask))
	{
		return;
	}

	// Check current skill with spawn flags
	if(gameskill == sk_baby || gameskill == sk_easy)
	{
		spawnMask = MTF_EASY;
	}
	else if(gameskill == sk_hard || gameskill == sk_nightmare)
	{
		spawnMask = MTF_HARD;
	}
	else
	{
		spawnMask = MTF_NORMAL;
	}
	if(!(mthing->options&spawnMask))
	{
		return;
	}

	// Check current character classes with spawn flags
	if(netgame == false)
	{ // Single player
		if((mthing->options&classFlags[cfg.PlayerClass[0]]) == 0)
		{ // Not for current class
			return;
		}
	}
	else if(deathmatch == false)
	{ // Cooperative
		spawnMask = 0;
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(players[i].plr->ingame)
			{
				spawnMask |= classFlags[cfg.PlayerClass[i]];
			}
		}

		// No players are in the game when a dedicated server is
		// started. In this case, we'll be generous and spawn stuff
		// for all the classes.
		if(!spawnMask) 
		{
			spawnMask |= MTF_FIGHTER | MTF_CLERIC | MTF_MAGE;
		}

		if((mthing->options&spawnMask) == 0)
		{
			return;
		}
	}

	// Find which type to spawn
	for(i = 0; i < Get(DD_NUMMOBJTYPES); i++)
	{
		if(mthing->type == mobjinfo[i].doomednum)
		{
			break;
		}
	}
	
	if(i == Get(DD_NUMMOBJTYPES))
	{ // Can't find thing type
		Con_Error("P_SpawnMapThing: Unknown type %i at (%i, %i)",
			mthing->type, mthing->x, mthing->y);
	}

	// Clients only spawn local objects.
	if(IS_CLIENT) 
	{
		if(!(mobjinfo[i].flags & MF_LOCAL))
			return;
	}

	// Don't spawn keys and players in deathmatch
	if(deathmatch && mobjinfo[i].flags&MF_NOTDMATCH)
	{
		return;
	}
		
	// Don't spawn monsters if -nomonsters
	if(nomonsters && (mobjinfo[i].flags&MF_COUNTKILL))
	{
		return;
	}

	x = mthing->x<<FRACBITS;
	y = mthing->y<<FRACBITS;
	if(mobjinfo[i].flags&MF_SPAWNCEILING)
	{
		z = ONCEILINGZ;
	}
	else if(mobjinfo[i].flags2&MF2_SPAWNFLOAT)
	{
		z = FLOATRANDZ;
	}
	else if(mobjinfo[i].flags2&MF2_FLOATBOB)
	{
		z = mthing->height<<FRACBITS;
	}
	else
	{
		z = ONFLOORZ;
	}
	switch(i)
	{ // Special stuff
		case MT_ZLYNCHED_NOHEART:
			P_SpawnMobj(x, y, ONFLOORZ, MT_BLOODPOOL);
			break;
		default:
			break;
	}
	mobj = P_SpawnMobj(x, y, z, i);
	if(z == ONFLOORZ)
	{
		mobj->z += mthing->height<<FRACBITS;
	}
	else if(z == ONCEILINGZ)
	{
		mobj->z -= mthing->height<<FRACBITS;
	}
	mobj->tid = mthing->tid;
	mobj->special = mthing->special;
	mobj->args[0] = mthing->arg1;
	mobj->args[1] = mthing->arg2;
	mobj->args[2] = mthing->arg3;
	mobj->args[3] = mthing->arg4;
	mobj->args[4] = mthing->arg5;
	if(mobj->flags2&MF2_FLOATBOB)
	{ // Seed random starting index for bobbing motion
		mobj->health = P_Random();
		mobj->special1 = mthing->height<<FRACBITS;
	}
	if(mobj->tics > 0)
	{
		mobj->tics = 1+(P_Random()%mobj->tics);
	}
//	if(mobj->flags&MF_COUNTITEM)
//	{
//		totalitems++;
//	}
	if (mobj->flags&MF_COUNTKILL)
	{
		// Quantize angle to 45 degree increments
		mobj->angle = ANG45*(mthing->angle/45);
	}
	else
	{
		// Scale angle correctly (source is 0..359)
		mobj->angle = ((mthing->angle<<8)/360)<<24;
	}
	mobj->visangle = mobj->angle >> 16; // "angle-servo"; smooth actor turning
	if(mthing->options&MTF_AMBUSH)
	{
		mobj->flags |= MF_AMBUSH;
	}
	if(mthing->options&MTF_DORMANT)
	{
		mobj->flags2 |= MF2_DORMANT;
		if(mobj->type == MT_ICEGUY)
		{
			P_SetMobjState(mobj, S_ICEGUY_DORMANT);
		}
		mobj->tics = -1;
	}
}

//==========================================================================
//
// P_CreateTIDList
//
//==========================================================================

void P_CreateTIDList(void)
{
	int i;
	mobj_t *mobj;
	thinker_t *t;

	i = 0;
	for(t = gi.thinkercap->next; t != gi.thinkercap; t = t->next)
	{ // Search all current thinkers
		if(t->function != P_MobjThinker)
		{ // Not a mobj thinker
			continue;
		}
		mobj = (mobj_t *)t;
		if(mobj->tid != 0)
		{ // Add to list
			if(i == MAX_TID_COUNT)
			{
				Con_Error("P_CreateTIDList: MAX_TID_COUNT (%d) exceeded.",
					MAX_TID_COUNT);
			}
			TIDList[i] = mobj->tid;
			TIDMobj[i++] = mobj;
		}
	}
	// Add termination marker
	TIDList[i] = 0;
}

//==========================================================================
//
// P_InsertMobjIntoTIDList
//
//==========================================================================

void P_InsertMobjIntoTIDList(mobj_t *mobj, int tid)
{
	int i;
	int index;

	index = -1;
	for(i = 0; TIDList[i] != 0; i++)
	{
		if(TIDList[i] == -1)
		{ // Found empty slot
			index = i;
			break;
		}
	}
	if(index == -1)
	{ // Append required
		if(i == MAX_TID_COUNT)
		{
			Con_Error("P_InsertMobjIntoTIDList: MAX_TID_COUNT (%d)"
				"exceeded.", MAX_TID_COUNT);
		}
		index = i;
		TIDList[index+1] = 0;
	}
	mobj->tid = tid;
	TIDList[index] = tid;
	TIDMobj[index] = mobj;
}

//==========================================================================
//
// P_RemoveMobjFromTIDList
//
//==========================================================================

void P_RemoveMobjFromTIDList(mobj_t *mobj)
{
	int i;

	for(i = 0; TIDList[i] != 0; i++)
	{
		if(TIDMobj[i] == mobj)
		{
			TIDList[i] = -1;
			TIDMobj[i] = NULL;
			mobj->tid = 0;
			return;
		}
	}
	mobj->tid = 0;
}

//==========================================================================
//
// P_FindMobjFromTID
//
//==========================================================================

mobj_t *P_FindMobjFromTID(int tid, int *searchPosition)
{
	int i;

	for(i = *searchPosition+1; TIDList[i] != 0; i++)
	{
		if(TIDList[i] == tid)
		{
			*searchPosition = i;
			return TIDMobj[i];
		}
	}
	*searchPosition = -1;
	return NULL;
}

/*
===============================================================================

						GAME SPAWN FUNCTIONS

===============================================================================
*/

//---------------------------------------------------------------------------
//
// PROC P_SpawnPuff
//
//---------------------------------------------------------------------------

extern fixed_t attackrange;

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
	mobj_t *puff;

	z += ((P_Random()-P_Random())<<10);
	puff = P_SpawnMobj(x, y, z, PuffType);
	if(linetarget && puff->info->seesound)
	{ // Hit thing sound
		S_StartSound(puff->info->seesound, puff);
	}
	else if(puff->info->attacksound)
	{
		S_StartSound(puff->info->attacksound, puff);
	}
	switch(PuffType)
	{
		case MT_PUNCHPUFF:
			puff->momz = FRACUNIT;
			break;
		case MT_HAMMERPUFF:
			puff->momz = .8*FRACUNIT;
			break;
		default:
			break;
	}
	PuffSpawned = puff;
}

/*
================
=
= P_SpawnBlood
=
================
*/

/*
void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage)
{
	mobj_t	*th;
	
	z += ((P_Random()-P_Random())<<10);
	th = P_SpawnMobj (x,y,z, MT_BLOOD);
	th->momz = FRACUNIT*2;
	th->tics -= P_Random()&3;

	if (damage <= 12 && damage >= 9)
		P_SetMobjState (th,S_BLOOD2);
	else if (damage < 9)
		P_SetMobjState (th,S_BLOOD3);
}
*/

//---------------------------------------------------------------------------
//
// PROC P_BloodSplatter
//
//---------------------------------------------------------------------------

void P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator)
{
	mobj_t *mo;

	mo = P_SpawnMobj(x, y, z, MT_BLOODSPLATTER);
	mo->target = originator;
	mo->momx = (P_Random()-P_Random())<<10;
	mo->momy = (P_Random()-P_Random())<<10;
	mo->momz = 3*FRACUNIT;
}

//===========================================================================
//
//  P_BloodSplatter2
//
//===========================================================================

void P_BloodSplatter2(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator)
{
	mobj_t *mo;

	mo = P_SpawnMobj(x+((P_Random()-128)<<11), y+((P_Random()-128)<<11), z, 
		MT_AXEBLOOD);
	mo->target = originator;
}

//---------------------------------------------------------------------------
//
// PROC P_RipperBlood
//
//---------------------------------------------------------------------------

void P_RipperBlood(mobj_t *mo)
{
	mobj_t *th;
	fixed_t x, y, z;

	x = mo->x+((P_Random()-P_Random())<<12);
	y = mo->y+((P_Random()-P_Random())<<12);
	z = mo->z+((P_Random()-P_Random())<<12);
	th = P_SpawnMobj(x, y, z, MT_BLOOD);
//	th->flags |= MF_NOGRAVITY;
	th->momx = mo->momx>>1;
	th->momy = mo->momy>>1;
	th->tics += P_Random()&3;
}

//---------------------------------------------------------------------------
//
// FUNC P_GetThingFloorType
//
//---------------------------------------------------------------------------

int P_GetThingFloorType(mobj_t *thing)
{
	if(thing->floorpic && !IS_CLIENT)
	{		
		return(TerrainTypes[thing->floorpic]);
	}
	else
	{
		return(TerrainTypes[thing->subsector->sector->floorpic]);
	}
/*
	if(thing->subsector->sector->floorpic
		== W_GetNumForName("FLTWAWA1")-firstflat)
	{
		return(FLOOR_WATER);
	}
	else
	{
		return(FLOOR_SOLID);
	}
*/
}

//---------------------------------------------------------------------------
//
// FUNC P_HitFloor
//
//---------------------------------------------------------------------------
#define SMALLSPLASHCLIP 12<<FRACBITS;

int P_HitFloor(mobj_t *thing)
{
	mobj_t *mo;
	int smallsplash=false;

	if(thing->floorz != thing->subsector->sector->floorheight)
	{ // don't splash if landing on the edge above water/lava/etc....
		return(FLOOR_SOLID);
	}

	// Things that don't splash go here
	switch(thing->type)
	{
		case MT_LEAF1:
		case MT_LEAF2:
//		case MT_BLOOD:			// I set these to low mass -- pm
//		case MT_BLOODSPLATTER:
		case MT_SPLASH:
		case MT_SLUDGECHUNK:
			return(FLOOR_SOLID);
		default:
			break;
	}

	// Small splash for small masses
	if (thing->info->mass < 10) smallsplash = true;

	switch(P_GetThingFloorType(thing))
	{
		case FLOOR_WATER:
			if (smallsplash)
			{
				mo=P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_SPLASHBASE);
				if (mo) mo->floorclip += SMALLSPLASHCLIP;
				S_StartSound(SFX_AMBIENT10, mo);	// small drip
			}
			else
			{
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_SPLASH);
				mo->target = thing;
				mo->momx = (P_Random()-P_Random())<<8;
				mo->momy = (P_Random()-P_Random())<<8;
				mo->momz = 2*FRACUNIT+(P_Random()<<8);
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_SPLASHBASE);
				if (thing->player) P_NoiseAlert(thing, thing);
				S_StartSound(SFX_WATER_SPLASH, mo);
			}
			return(FLOOR_WATER);
		case FLOOR_LAVA:
			if (smallsplash)
			{
				mo=P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_LAVASPLASH);
				if (mo) mo->floorclip += SMALLSPLASHCLIP;
			}
			else
			{
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_LAVASMOKE);
				mo->momz = FRACUNIT+(P_Random()<<7);
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_LAVASPLASH);
				if (thing->player) P_NoiseAlert(thing, thing);
			}
			S_StartSound(SFX_LAVA_SIZZLE, mo);
			if(thing->player && leveltime&31)
			{
				P_DamageMobj(thing, &LavaInflictor, NULL, 5);
			}
			return(FLOOR_LAVA);
		case FLOOR_SLUDGE:
			if (smallsplash)
			{
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ,
					MT_SLUDGESPLASH);
				if (mo) mo->floorclip += SMALLSPLASHCLIP;
			}
			else
			{
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, MT_SLUDGECHUNK);
				mo->target = thing;
				mo->momx = (P_Random()-P_Random())<<8;
				mo->momy = (P_Random()-P_Random())<<8;
				mo->momz = FRACUNIT+(P_Random()<<8);
				mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, 
					MT_SLUDGESPLASH);
				if (thing->player) P_NoiseAlert(thing, thing);
			}
			S_StartSound(SFX_SLUDGE_GLOOP, mo);
			return(FLOOR_SLUDGE);
	}
	return(FLOOR_SOLID);
}


//---------------------------------------------------------------------------
//
// FUNC P_CheckMissileSpawn
//
// Returns true if the missile is at a valid spawn point, otherwise
// explodes it and returns false.
//
//---------------------------------------------------------------------------

boolean P_CheckMissileSpawn(mobj_t *missile)
{
	//missile->tics -= P_Random()&3;

	// move a little forward so an angle can be computed if it
	// immediately explodes
	missile->x += (missile->momx>>1);
	missile->y += (missile->momy>>1);
	missile->z += (missile->momz>>1);
	if(!P_TryMove(missile, missile->x, missile->y))
	{
		P_ExplodeMissile(missile);
		return(false);
	}
	return(true);
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissile
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	fixed_t z;
	mobj_t *th;
	angle_t an;
	int dist, origdist;
	float aim;

	switch(type)
	{
		case MT_MNTRFX1: // Minotaur swing attack missile
			z = source->z+40*FRACUNIT;
			break;
		case MT_MNTRFX2: // Minotaur floor fire missile
			z = ONFLOORZ+source->floorclip;
			break;
		case MT_CENTAUR_FX:
			z = source->z+45*FRACUNIT;
			break;
		case MT_ICEGUY_FX:
			z = source->z+40*FRACUNIT;
			break;
		case MT_HOLY_MISSILE:
			z = source->z+40*FRACUNIT;
			break;
		default:
			z = source->z+32*FRACUNIT;
			break;
	}
	z -= source->floorclip;
	th = P_SpawnMobj(source->x, source->y, z, type);
	if(th->info->seesound)
	{
		S_StartSound(th->info->seesound, th);
	}
	th->target = source; // Originator
	an = R_PointToAngle2(source->x, source->y, dest->x, dest->y);
	if(dest->flags&MF_SHADOW)
	{ // Invisible target
		an += (P_Random()-P_Random())<<21;
	}
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(th->info->speed, finecosine[an]);
	th->momy = FixedMul(th->info->speed, finesine[an]);
	origdist = P_ApproxDistance(dest->x - source->x, dest->y - source->y);
	dist = origdist/th->info->speed;
	if(dist < 1)
	{	
		dist = 1;
	}
	th->momz = (dest->z-source->z)/dist;
		
	// Use a more three-dimensional method.
	aim = BANG2RAD(bamsAtan2((dest->z - source->z) >> FRACBITS, 
		origdist >> FRACBITS));
	th->momx *= cos(aim);
	th->momy *= cos(aim);
	th->momz = sin(aim) * th->info->speed;
	
	return(P_CheckMissileSpawn(th) ? th : NULL);
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileXYZ
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

mobj_t *P_SpawnMissileXYZ(fixed_t x, fixed_t y, fixed_t z,
	mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	mobj_t *th;
	angle_t an;
	int dist;

	z -= source->floorclip;
	th = P_SpawnMobj(x, y, z, type);
	if(th->info->seesound)
	{
		S_StartSound(th->info->seesound, th);
	}
	th->target = source; // Originator
	an = R_PointToAngle2(source->x, source->y, dest->x, dest->y);
	if(dest->flags&MF_SHADOW)
	{ // Invisible target
		an += (P_Random()-P_Random())<<21;
	}
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(th->info->speed, finecosine[an]);
	th->momy = FixedMul(th->info->speed, finesine[an]);
	dist = P_ApproxDistance(dest->x - source->x, dest->y - source->y);
	dist = dist/th->info->speed;
	if(dist < 1)
	{
		dist = 1;
	}
	th->momz = (dest->z-source->z)/dist;
	return(P_CheckMissileSpawn(th) ? th : NULL);
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileAngle
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type,
	angle_t angle, fixed_t momz)
{
	fixed_t z;
	mobj_t *mo;

	switch(type)
	{
		case MT_MNTRFX1: // Minotaur swing attack missile
			z = source->z+40*FRACUNIT;
			break;
		case MT_MNTRFX2: // Minotaur floor fire missile
			z = ONFLOORZ+source->floorclip;
			break;
		case MT_ICEGUY_FX2: // Secondary Projectiles of the Ice Guy
			z = source->z+3*FRACUNIT;
			break;
		case MT_MSTAFF_FX2:
			z = source->z+40*FRACUNIT;
			break;
		default:
			z = source->z+32*FRACUNIT;
			break;
	}
	z -= source->floorclip;
	mo = P_SpawnMobj(source->x, source->y, z, type);
	if(mo->info->seesound)
	{
		S_StartSound(mo->info->seesound, mo);
	}
	mo->target = source; // Originator
	mo->angle = angle;
	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[angle]);
	mo->momz = momz;
	return(P_CheckMissileSpawn(mo) ? mo : NULL);
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileAngleSpeed
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

mobj_t *P_SpawnMissileAngleSpeed(mobj_t *source, mobjtype_t type,
	angle_t angle, fixed_t momz, fixed_t speed)
{
	fixed_t z;
	mobj_t *mo;

	z = source->z;
	z -= source->floorclip;
	mo = P_SpawnMobj(source->x, source->y, z, type);
	if(mo->info->seesound)
	{
		//S_StartSound(mo->info->seesound, mo);
	}
	mo->target = source; // Originator
	mo->angle = angle;
	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(speed, finecosine[angle]);
	mo->momy = FixedMul(speed, finesine[angle]);
	mo->momz = momz;
	return(P_CheckMissileSpawn(mo) ? mo : NULL);
}



/*
================
=
= P_SpawnPlayerMissile
=
= Tries to aim at a nearby monster
================
*/

mobj_t *P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
	angle_t an;
	fixed_t x, y, z, slope;
	//float fangle = source->player->plr->lookdir * 85.0/110.0 /180*PI;
	float fangle = LOOKDIR2RAD(source->player->plr->lookdir);
	float movfac = 1;
	boolean dontAim = cfg.noAutoAim /*&& !demorecording && !demoplayback && !netgame*/;

	// Try to find a target
	an = source->angle;
	slope = P_AimLineAttack(source, an, 16*64*FRACUNIT);
	if(!linetarget || dontAim)
	{
		an += 1<<26;
		slope = P_AimLineAttack(source, an, 16*64*FRACUNIT);
		if(!linetarget)
		{
			an -= 2<<26;
			slope = P_AimLineAttack(source, an, 16*64*FRACUNIT);
		}
		if(!linetarget || dontAim)
		{
			an = source->angle;
			/*if(demoplayback || demorecording)
				slope = (((int)source->player->plr->lookdir)<<FRACBITS)/173;
			else
			{*/
			slope = FRACUNIT * sin(fangle) / 1.2;
			movfac = cos(fangle);
			//}
		}
	}
	x = source->x;
	y = source->y;
	if(type == MT_LIGHTNING_FLOOR)
	{	
		z = ONFLOORZ;
		slope = 0;
	}
	else if(type == MT_LIGHTNING_CEILING)
	{
		z = ONCEILINGZ;
		slope = 0;
	}
	else
	{
		z = source->z + 4*8*FRACUNIT+(((int)source->player->plr->lookdir)<<FRACBITS)/173;
		z -= source->floorclip;
	}

#ifdef TIC_DEBUG
	FUNTAG("P_SpawnPlayerMissile");
	if(rndDebugfile) fprintf(rndDebugfile, " -- srcZ:%f, plr:%d, lookdir:%f, z:%f, slope:%f\n", 
		FIX2FLT(source->z), source->player-players, source->player->plr->lookdir, 
		FIX2FLT(z), FIX2FLT(slope));
#endif

	MissileMobj = P_SpawnMobj(x, y, z, type);
	if(MissileMobj->info->seesound)
	{
		//S_StartSound(MissileMobj->info->seesound, MissileMobj);
	}
	MissileMobj->target = source;
	MissileMobj->angle = an;
	MissileMobj->momx = movfac*FixedMul(MissileMobj->info->speed,
		finecosine[an>>ANGLETOFINESHIFT]);
	MissileMobj->momy = movfac*FixedMul(MissileMobj->info->speed,
		finesine[an>>ANGLETOFINESHIFT]);
	MissileMobj->momz = FixedMul(MissileMobj->info->speed, slope);
	if(MissileMobj->type == MT_MWAND_MISSILE 
		|| MissileMobj->type == MT_CFLAME_MISSILE)
	{ // Ultra-fast ripper spawning missile
		MissileMobj->x += (MissileMobj->momx>>3);
		MissileMobj->y += (MissileMobj->momy>>3);
		MissileMobj->z += (MissileMobj->momz>>3);
	}
	else
	{ // Normal missile
		MissileMobj->x += (MissileMobj->momx>>1);
		MissileMobj->y += (MissileMobj->momy>>1);
		MissileMobj->z += (MissileMobj->momz>>1);
	}
	if(!P_TryMove(MissileMobj, MissileMobj->x, MissileMobj->y))
	{ // Exploded immediately
		P_ExplodeMissile(MissileMobj);
		return(NULL);
	}
	return(MissileMobj);
}


//----------------------------------------------------------------------------
//
// P_SpawnPlayerMinotaur - 
//
//	Special missile that has larger blocking than player
//----------------------------------------------------------------------------

/*
mobj_t *P_SpawnPlayerMinotaur(mobj_t *source, mobjtype_t type)
{
	angle_t an;
	fixed_t x, y, z;
	fixed_t dist=0 *FRACUNIT;

	an = source->angle;
	x = source->x + FixedMul(dist, finecosine[an>>ANGLETOFINESHIFT]);
	y = source->y + FixedMul(dist, finesine[an>>ANGLETOFINESHIFT]);
	z = source->z + 4*8*FRACUNIT+((source->player->plr->lookdir)<<FRACBITS)/173;
	z -= source->floorclip;
	MissileMobj = P_SpawnMobj(x, y, z, type);
	if(MissileMobj->info->seesound)
	{
		//S_StartSound(MissileMobj->info->seesound, MissileMobj);
	}
	MissileMobj->target = source;
	MissileMobj->angle = an;
	MissileMobj->momx = FixedMul(MissileMobj->info->speed,
		finecosine[an>>ANGLETOFINESHIFT]);
	MissileMobj->momy = FixedMul(MissileMobj->info->speed,
		finesine[an>>ANGLETOFINESHIFT]);
	MissileMobj->momz = 0;

//	MissileMobj->x += (MissileMobj->momx>>3);
//	MissileMobj->y += (MissileMobj->momy>>3);
//	MissileMobj->z += (MissileMobj->momz>>3);

	if(!P_TryMove(MissileMobj, MissileMobj->x, MissileMobj->y))
	{ // Wouln't fit

		return(NULL);
	}
	return(MissileMobj);
}
*/

//---------------------------------------------------------------------------
//
// PROC P_SPMAngle
//
//---------------------------------------------------------------------------

mobj_t *P_SPMAngle(mobj_t *source, mobjtype_t type, angle_t angle)
{
	mobj_t *th;
	angle_t an;
	fixed_t x, y, z, slope;
	float fangle = LOOKDIR2RAD(source->player->plr->lookdir);
	float movfac = 1;
	boolean dontAim = cfg.noAutoAim /*&& !demorecording && !demoplayback && !netgame*/;

//
// see which target is to be aimed at
//
	an = angle;
	slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
	if (!linetarget || dontAim)
	{
		an += 1<<26;
		slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		if (!linetarget)
		{
			an -= 2<<26;
			slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		}
		if (!linetarget || dontAim)
		{
			an = angle;
			/*if(demoplayback || demorecording)
				slope = (((int)source->player->plr->lookdir)<<FRACBITS)/173;
			else
			{*/
			slope = FRACUNIT * sin(fangle) / 1.2;
			movfac = cos(fangle);
			//}
		}
	}
	x = source->x;
	y = source->y;
	z = source->z + 4*8*FRACUNIT+(((int)source->player->plr->lookdir)<<FRACBITS)/173;
	z -= source->floorclip;
	th = P_SpawnMobj(x, y, z, type);
//	if(th->info->seesound)
//	{
//		S_StartSound(th->info->seesound, th);
//	}
	th->target = source;
	th->angle = an;
	th->momx = movfac*FixedMul(th->info->speed, finecosine[an>>ANGLETOFINESHIFT]);
	th->momy = movfac*FixedMul(th->info->speed, finesine[an>>ANGLETOFINESHIFT]);
	th->momz = FixedMul(th->info->speed, slope);
	return(P_CheckMissileSpawn(th) ? th : NULL);
}

//===========================================================================
//
// P_SPMAngleXYZ
//
//===========================================================================

mobj_t *P_SPMAngleXYZ(mobj_t *source, fixed_t x, fixed_t y, 
	fixed_t z, mobjtype_t type, angle_t angle)
{
	mobj_t *th;
	angle_t an;
	fixed_t slope;
	float fangle = LOOKDIR2RAD(source->player->plr->lookdir);
	float movfac = 1;
	boolean dontAim = cfg.noAutoAim /*&& !demorecording && !demoplayback && !netgame*/;

//
// see which target is to be aimed at
//
	an = angle;
	slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
	if (!linetarget || dontAim)
	{
		an += 1<<26;
		slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		if (!linetarget)
		{
			an -= 2<<26;
			slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		}
		if (!linetarget || dontAim)
		{
			an = angle;
			/*if(demoplayback || demorecording)// || netgame)
				slope = (((int)source->player->plr->lookdir)<<FRACBITS)/173;
			else
			{*/
			slope = FRACUNIT * sin(fangle) / 1.2;
			movfac = cos(fangle);
			//}
		}
	}
	z += 4*8*FRACUNIT+(((int)source->player->plr->lookdir)<<FRACBITS)/173;
	z -= source->floorclip;
	th = P_SpawnMobj(x, y, z, type);
//	if(th->info->seesound)
//	{
//		S_StartSound(th->info->seesound, th);
//	}
	th->target = source;
	th->angle = an;
	th->momx = movfac*FixedMul(th->info->speed, finecosine[an>>ANGLETOFINESHIFT]);
	th->momy = movfac*FixedMul(th->info->speed, finesine[an>>ANGLETOFINESHIFT]);
	th->momz = FixedMul(th->info->speed, slope);
	return(P_CheckMissileSpawn(th) ? th : NULL);
}

mobj_t *P_SpawnKoraxMissile(fixed_t x, fixed_t y, fixed_t z,
	mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	mobj_t *th;
	angle_t an;
	int dist;

	z -= source->floorclip;
	th = P_SpawnMobj(x, y, z, type);
	if(th->info->seesound)
	{
		S_StartSound(th->info->seesound, th);
	}
	th->target = source; // Originator
	an = R_PointToAngle2(x, y, dest->x, dest->y);
	if(dest->flags&MF_SHADOW)
	{ // Invisible target
		an += (P_Random()-P_Random())<<21;
	}
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(th->info->speed, finecosine[an]);
	th->momy = FixedMul(th->info->speed, finesine[an]);
	dist = P_ApproxDistance(dest->x - x, dest->y - y);
	dist = dist/th->info->speed;
	if(dist < 1)
	{
		dist = 1;
	}
	th->momz = (dest->z-z+(30*FRACUNIT))/dist;
	return(P_CheckMissileSpawn(th) ? th : NULL);
}

//==========================================================================
// R_SetAllDoomsdayFlags
//	Updates ddflags of all visible mobjs (in sectorlinks).
//	Not strictly necessary (in single player games at least) but here
//	we tell the engine about light-emitting objects, special effects,
//	object properties (solid, local, low/nograv, etc.), color translation
//	and other interesting little details.
//	NOTE: Wrong source file? This is a refresh routine.
//==========================================================================
void R_SetAllDoomsdayFlags(void)
{
	int			i, Class;
	sector_t	*sec = sectors;
	mobj_t		*mo;

	// Only visible things are in the sector thinglists, so this is good.
	for(i = 0; i < numsectors; i++, sec++)
		for(mo = sec->thinglist; mo; mo = mo->snext)
		{
			if(IS_CLIENT && mo->ddflags & DDMF_REMOTE) continue;

			// Reset the flags for a new frame.
			mo->ddflags &= DDMF_CLEAR_MASK; 
			
			if(mo->flags & MF_LOCAL) mo->ddflags |= DDMF_LOCAL;
			if(mo->flags & MF_SOLID) mo->ddflags |= DDMF_SOLID;
			if(mo->flags & MF_MISSILE) mo->ddflags |= DDMF_MISSILE;
			if(mo->flags2 & MF2_FLY) 
				mo->ddflags |= DDMF_FLY | DDMF_NOGRAVITY;
			if(mo->flags2 & MF2_FLOATBOB) 
				mo->ddflags |= DDMF_BOB | DDMF_NOGRAVITY;
			if(mo->flags2 & MF2_LOGRAV) mo->ddflags |= DDMF_LOWGRAVITY;
			if(mo->flags & MF_NOGRAVITY/* || mo->flags2 & MF2_FLY*/) 
				mo->ddflags |= DDMF_NOGRAVITY;
 
			// $democam: cameramen are invisible
			if(P_IsCamera(mo)) mo->ddflags |= DDMF_DONTDRAW;

			// Choose which ddflags to set.
			if(mo->flags2 & MF2_DONTDRAW)
			{
				mo->ddflags |= DDMF_DONTDRAW;
				continue; // No point in checking the other flags.
			}

			if((mo->flags & MF_BRIGHTSHADOW) == MF_BRIGHTSHADOW)
				mo->ddflags |= DDMF_BRIGHTSHADOW;
			else 
			{
				if(mo->flags & MF_SHADOW)
					mo->ddflags |= DDMF_SHADOW;
				if(mo->flags & MF_ALTSHADOW 
					|| (cfg.translucentIceCorpse && mo->flags & MF_ICECORPSE))
					mo->ddflags |= DDMF_ALTSHADOW;
			}

			if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) 
				|| mo->flags & MF_FLOAT 
				|| (mo->flags & MF_MISSILE && !(mo->flags & MF_VIEWALIGN)))
				mo->ddflags |= DDMF_VIEWALIGN;
		
			mo->ddflags |= mo->flags & MF_TRANSLATION;

			// Which translation table to use?
			if(mo->flags & MF_TRANSLATION)
			{
				if(mo->player)
				{
					Class = mo->player->class;
				}
				else
				{
					Class = mo->special1;
				}
				if(Class > 2)
				{
					Class = 0;
				}
				// The last two bits.
				mo->ddflags |= Class << DDMF_CLASSTRSHIFT;
			}

			// An offset for the light emitted by this object.
/*			Class = MobjLightOffsets[mo->type];
			if(Class < 0) Class = 8-Class;
			// Class must now be in range 0-15.
			mo->ddflags |= Class << DDMF_LIGHTOFFSETSHIFT;*/
			
			// The Mage's ice shards need to be a bit smaller.
			// This'll make them half the normal size.
			if(mo->type == MT_SHARDFX1)
				mo->ddflags |= 2 << DDMF_LIGHTSCALESHIFT;
		}
}

