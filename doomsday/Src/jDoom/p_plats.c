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
// Revision 1.7  2004/05/30 08:42:41  skyjake
// Tweaked indentation style
//
// Revision 1.6  2004/05/29 09:53:29  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.5  2004/05/28 19:52:58  skyjake
// Finished switch from branch-1-7 to trunk, hopefully everything is fine
//
// Revision 1.2.2.1  2004/05/16 10:01:36  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.4.1  2003/11/19 17:07:13  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/02/27 23:14:32  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:21:57  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:47  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//  Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------

static const char
        rcsid[] = "$Id$";

#include "doomdef.h"

#include "m_random.h"

#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

plat_t *activeplats[MAXPLATS];

//
// Move a plat up and down
//
void T_PlatRaise(plat_t * plat)
{
	result_e res;

	switch (plat->status)
	{
	case up:
		res =
			T_MovePlane(plat->sector, plat->speed, plat->high, plat->crush, 0,
						1);

		if(plat->type == raiseAndChange ||
		   plat->type == raiseToNearestAndChange)
		{
			if(!(leveltime & 7))
				S_SectorSound(plat->sector, sfx_stnmov);
			//gi.Sv_PlaneSound(plat->sector, false, sfx_stnmov, 7);
		}

		if(res == crushed && (!plat->crush))
		{
			plat->count = plat->wait;
			plat->status = down;
			S_SectorSound(plat->sector, sfx_pstart);
		}
		else
		{
			if(res == pastdest)
			{
				plat->count = plat->wait;
				plat->status = waiting;
				S_SectorSound(plat->sector, sfx_pstop);

				switch (plat->type)
				{
				case blazeDWUS:
				case downWaitUpStay:
					P_RemoveActivePlat(plat);
					break;

				case raiseAndChange:
				case raiseToNearestAndChange:
					P_RemoveActivePlat(plat);
					break;

				default:
					break;
				}
			}
		}
		break;

	case down:
		res = T_MovePlane(plat->sector, plat->speed, plat->low, false, 0, -1);

		if(res == pastdest)
		{
			plat->count = plat->wait;
			plat->status = waiting;
			S_SectorSound(plat->sector, sfx_pstop);
		}
		break;

	case waiting:
		if(!--plat->count)
		{
			if(plat->sector->floorheight == plat->low)
				plat->status = up;
			else
				plat->status = down;
			S_SectorSound(plat->sector, sfx_pstart);
		}
	case in_stasis:
		break;
	}
}

//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int EV_DoPlat(line_t *line, plattype_e type, int amount)
{
	plat_t *plat;
	int     secnum;
	int     rtn;
	sector_t *sec;

	secnum = -1;
	rtn = 0;

	//  Activate all <type> plats that are in_stasis
	switch (type)
	{
	case perpetualRaise:
		P_ActivateInStasis(line->tag);
		break;

	default:
		break;
	}

	while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
	{
		sec = &sectors[secnum];

		if(sec->specialdata)
			continue;

		// Find lowest & highest floors around sector
		rtn = 1;
		plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
		P_AddThinker(&plat->thinker);

		plat->type = type;
		plat->sector = sec;
		plat->sector->specialdata = plat;
		plat->thinker.function = (actionf_p1) T_PlatRaise;
		plat->crush = false;
		plat->tag = line->tag;

		switch (type)
		{
		case raiseToNearestAndChange:
			plat->speed = PLATSPEED / 2;
			sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
			//gi.Sv_SectorReport(sec, false, sec->floorheight, 0, sec->floorpic, -1);
			plat->high = P_FindNextHighestFloor(sec, sec->floorheight);
			plat->wait = 0;
			plat->status = up;
			// NO MORE DAMAGE, IF APPLICABLE
			sec->special = 0;

			S_SectorSound(sec, sfx_stnmov);
			break;

		case raiseAndChange:
			plat->speed = PLATSPEED / 2;
			sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
			//gi.Sv_SectorReport(sec, false, sec->floorheight, 0, sec->floorpic, -1);
			plat->high = sec->floorheight + amount * FRACUNIT;
			plat->wait = 0;
			plat->status = up;

			S_SectorSound(sec, sfx_stnmov);
			break;

		case downWaitUpStay:
			plat->speed = PLATSPEED * 4;
			plat->low = P_FindLowestFloorSurrounding(sec);

			if(plat->low > sec->floorheight)
				plat->low = sec->floorheight;

			plat->high = sec->floorheight;
			plat->wait = 35 * PLATWAIT;
			plat->status = down;
			S_SectorSound(sec, sfx_pstart);
			break;

		case blazeDWUS:
			plat->speed = PLATSPEED * 8;
			plat->low = P_FindLowestFloorSurrounding(sec);

			if(plat->low > sec->floorheight)
				plat->low = sec->floorheight;

			plat->high = sec->floorheight;
			plat->wait = 35 * PLATWAIT;
			plat->status = down;
			S_SectorSound(sec, sfx_pstart);
			break;

		case perpetualRaise:
			plat->speed = PLATSPEED;
			plat->low = P_FindLowestFloorSurrounding(sec);

			if(plat->low > sec->floorheight)
				plat->low = sec->floorheight;

			plat->high = P_FindHighestFloorSurrounding(sec);

			if(plat->high < sec->floorheight)
				plat->high = sec->floorheight;

			plat->wait = 35 * PLATWAIT;
			plat->status = P_Random() & 1;

			S_SectorSound(sec, sfx_pstart);
			break;
		}
		P_AddActivePlat(plat);
	}
	return rtn;
}

void P_ActivateInStasis(int tag)
{
	int     i;

	for(i = 0; i < MAXPLATS; i++)
		if(activeplats[i] && (activeplats[i])->tag == tag &&
		   (activeplats[i])->status == in_stasis)
		{
			(activeplats[i])->status = (activeplats[i])->oldstatus;
			(activeplats[i])->thinker.function = (actionf_p1) T_PlatRaise;
		}
}

void EV_StopPlat(line_t *line)
{
	int     j;

	for(j = 0; j < MAXPLATS; j++)
		if(activeplats[j] && ((activeplats[j])->status != in_stasis) &&
		   ((activeplats[j])->tag == line->tag))
		{
			(activeplats[j])->oldstatus = (activeplats[j])->status;
			(activeplats[j])->status = in_stasis;
			(activeplats[j])->thinker.function = (actionf_v) NULL;
		}
}

void P_AddActivePlat(plat_t * plat)
{
	int     i;

	for(i = 0; i < MAXPLATS; i++)
		if(activeplats[i] == NULL)
		{
			activeplats[i] = plat;
			return;
		}
	Con_Error("P_AddActivePlat: no more plats!");
}

void P_RemoveActivePlat(plat_t * plat)
{
	int     i;

	for(i = 0; i < MAXPLATS; i++)
		if(plat == activeplats[i])
		{
			(activeplats[i])->sector->specialdata = NULL;
			P_RemoveThinker(&(activeplats[i])->thinker);
			activeplats[i] = NULL;

			return;
		}
	Con_Error("P_RemoveActivePlat: can't find plat!");
}
