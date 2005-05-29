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
// Revision 1.9  2005/05/29 12:45:09  danij
// Removed fixed limits on number of active plats/ceilings using modified code from PrBoom.
//
// Revision 1.8  2005/01/01 22:58:52  skyjake
// Resolved a bunch of compiler warnings
//
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

#include "doomdef.h"

#include "m_random.h"

#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

platlist_t *activeplats;

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

//
// P_ActivateInStasis()
//
// Activate a plat that has been put in stasis
// (stopped perpetual floor, instant floor/ceil toggle)
//
// Passed the tag of the plat that should be reactivated
// Returns nothing
//
void P_ActivateInStasis(int tag)
{
  platlist_t *pl;
  for (pl=activeplats; pl; pl=pl->next)   // search the active plats
  {
    plat_t *plat = pl->plat;              // for one in stasis with right tag
    if (plat->tag == tag && plat->status == in_stasis)
    {
		plat->status = plat->oldstatus;
		plat->thinker.function = (actionf_p1) T_PlatRaise;
	}
  }
}

//
// EV_StopPlat()
//
// Handler for "stop perpetual floor" linedef type
//
// Passed the linedef that stopped the plat
// Returns true if a plat was put in stasis
//
// jff 2/12/98 added int return value, fixed return
//
int EV_StopPlat(line_t* line)
{
  platlist_t *pl;
  for (pl=activeplats; pl; pl=pl->next)  // search the active plats
  {
    plat_t *plat = pl->plat;             // for one with the tag not in stasis
    if (plat->status != in_stasis && plat->tag == line->tag)
    {
      plat->oldstatus = plat->status;    // put it in stasis
      plat->status = in_stasis;
      plat->thinker.function = (actionf_v) NULL;
	}
  }
  return 1;
}

//
// P_AddActivePlat()
//
// Add a plat to the head of the active plat list
//
// Passed a pointer to the plat to add
// Returns nothing
//
void P_AddActivePlat(plat_t* plat)
{
  platlist_t *list = malloc(sizeof *list);
  list->plat = plat;
  plat->list = list;
  if ((list->next = activeplats))
    list->next->prev = &list->next;
  list->prev = &activeplats;
  activeplats = list;
}

//
// P_RemoveActivePlat()
//
// Remove a plat from the active plat list
//
// Passed a pointer to the plat to remove
// Returns nothing
//
void P_RemoveActivePlat(plat_t* plat)
{
  platlist_t *list = plat->list;
  plat->sector->specialdata = NULL;
  P_RemoveThinker(&plat->thinker);
  if ((*list->prev = list->next))
    list->next->prev = list->prev;
  free(list);
}

//
// P_RemoveAllActivePlats()
//
// Remove all plats from the active plat list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActivePlats(void)
{
  while (activeplats)
  {
    platlist_t *next = activeplats->next;
    free(activeplats);
    activeplats = next;
  }
}
