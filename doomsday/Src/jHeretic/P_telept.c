
// P_telept.c

#include "Doomdef.h"
#include "P_local.h"
#include "Soundst.h"

mobj_t *P_SpawnTeleFog(int x, int y)
{
	subsector_t *ss = R_PointInSubsector(x,y);
	return P_SpawnMobj (x, y, 
		ss->sector->floorheight+TELEFOGHEIGHT, 
		MT_TFOG);
}

//----------------------------------------------------------------------------
//
// FUNC P_Teleport
//
//----------------------------------------------------------------------------

boolean P_Teleport(mobj_t *thing, fixed_t x, fixed_t y, angle_t angle)
{
	fixed_t oldx;
	fixed_t oldy;
	fixed_t oldz;
	fixed_t aboveFloor;
	fixed_t fogDelta;
	player_t *player;
	unsigned an;
	mobj_t *fog;

	oldx = thing->x;
	oldy = thing->y;
	oldz = thing->z;
	aboveFloor = thing->z-thing->floorz;
	if(!P_TeleportMove(thing, x, y))
	{
		return(false);
	}
	if(thing->player)
	{
		player = thing->player;
		if(player->powers[pw_flight] && aboveFloor)
		{
			thing->z = thing->floorz+aboveFloor;
			if(thing->z+thing->height > thing->ceilingz)
			{
				thing->z = thing->ceilingz-thing->height;
			}
			player->plr->viewz = thing->z+player->plr->viewheight;
		}
		else
		{
			thing->z = thing->floorz;
			player->plr->viewz = thing->z+player->plr->viewheight;
			player->plr->clLookDir = 0;
			player->plr->lookdir = 0;
		}
		player->plr->clAngle = angle;
		player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
	}
	else if(thing->flags&MF_MISSILE)
	{
		thing->z = thing->floorz+aboveFloor;
		if(thing->z+thing->height > thing->ceilingz)
		{
			thing->z = thing->ceilingz-thing->height;
		}
	}
	else
	{
		thing->z = thing->floorz;
	}
	// Spawn teleport fog at source and destination
	fogDelta = thing->flags&MF_MISSILE ? 0 : TELEFOGHEIGHT;
	fog = P_SpawnMobj(oldx, oldy, oldz+fogDelta, MT_TFOG);
	S_StartSound(sfx_telept, fog);
	an = angle>>ANGLETOFINESHIFT;
	fog = P_SpawnMobj(x+20*finecosine[an],
		y+20*finesine[an], thing->z+fogDelta, MT_TFOG);
	S_StartSound(sfx_telept, fog);
	if(thing->player && !thing->player->powers[pw_weaponlevel2])
	{ // Freeze player for about .5 sec
		thing->reactiontime = 18;
	}
	thing->angle = angle;
	if(thing->flags2&MF2_FOOTCLIP && P_GetThingFloorType(thing) != FLOOR_SOLID)
	{
		thing->flags2 |= MF2_FEETARECLIPPED;
	}
	else if(thing->flags2&MF2_FEETARECLIPPED)
	{
		thing->flags2 &= ~MF2_FEETARECLIPPED;
	}
	if(thing->flags&MF_MISSILE)
	{
		angle >>= ANGLETOFINESHIFT;
		thing->momx = FixedMul(thing->info->speed, finecosine[angle]);
		thing->momy = FixedMul(thing->info->speed, finesine[angle]);
	}
	else
	{
		thing->momx = thing->momy = thing->momz = 0;
	}
	P_ClearThingSRVO(thing);
	return(true);
}

//----------------------------------------------------------------------------
//
// FUNC EV_Teleport
//
//----------------------------------------------------------------------------

boolean EV_Teleport(line_t *line, int side, mobj_t *thing)
{
	int i;
	int tag;
	mobj_t *m;
	thinker_t *thinker;
	sector_t *sector;

	if(thing->flags2&MF2_NOTELEPORT)
	{
		return(false);
	}
	if(side == 1)
	{ // Don't teleport when crossing back side
		return(false);
	}
	tag = line->tag;
	for(i = 0; i < numsectors; i++)
	{
		if(sectors[i].tag == tag)
		{
			thinker = thinkercap.next;
			for(thinker = thinkercap.next; thinker != &thinkercap;
				thinker = thinker->next)
			{
				if(thinker->function != P_MobjThinker)
				{ // Not a mobj
					continue;
				}
				m = (mobj_t *)thinker;
				if(m->type != MT_TELEPORTMAN )
				{ // Not a teleportman
					continue;
				}
				sector = m->subsector->sector;
				if(sector-sectors != i)
				{ // Wrong sector
					continue;
				}
				return(P_Teleport(thing, m->x, m->y, m->angle));
			}
		}
	}
	return(false);
}

