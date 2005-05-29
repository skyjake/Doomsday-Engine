
#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"

//==================================================================
//==================================================================
//
//                          CEILINGS
//
//==================================================================
//==================================================================

ceilinglist_t *activeceilings;

//==================================================================
//
//  T_MoveCeiling
//
//==================================================================
void T_MoveCeiling(ceiling_t * ceiling)
{
	result_e res;

	switch (ceiling->direction)
	{
	case 0:					// IN STASIS
		break;
	case 1:					// UP
		res =
			T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topheight,
						false, 1, ceiling->direction);
		if(!(leveltime & 7))
			//S_StartSound((mobj_t *)&ceiling->sector->soundorg, sfx_dormov);
			S_SectorSound(ceiling->sector, sfx_dormov);
		//gi.Sv_PlaneSound(ceiling->sector, true, sfx_dormov, 7);
		if(res == pastdest)
			switch (ceiling->type)
			{
			case raiseToHighest:
				P_RemoveActiveCeiling(ceiling);
				break;
			case fastCrushAndRaise:
			case crushAndRaise:
				ceiling->direction = -1;
				break;
			default:
				break;
			}
		break;
	case -1:					// DOWN
		res =
			T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight,
						ceiling->crush, 1, ceiling->direction);
		if(!(leveltime & 7))
			//S_StartSound((mobj_t *)&ceiling->sector->soundorg,sfx_dormov);
			S_SectorSound(ceiling->sector, sfx_dormov);
		//gi.Sv_PlaneSound(ceiling->sector, true, sfx_dormov, 7);
		if(res == pastdest)
			switch (ceiling->type)
			{
			case crushAndRaise:
				ceiling->speed = CEILSPEED;
			case fastCrushAndRaise:
				ceiling->direction = 1;
				break;
			case lowerAndCrush:
			case lowerToFloor:
				P_RemoveActiveCeiling(ceiling);
				break;
			default:
				break;
			}
		else if(res == crushed)
			switch (ceiling->type)
			{
			case crushAndRaise:
			case lowerAndCrush:
				ceiling->speed = CEILSPEED / 8;
				break;
			default:
				break;
			}
		break;
	}
}

//==================================================================
//
//      EV_DoCeiling
//      Move a ceiling up/down and all around!
//
//==================================================================
int EV_DoCeiling(line_t *line, ceiling_e type)
{
	int     secnum, rtn;
	sector_t *sec;
	ceiling_t *ceiling;

	secnum = -1;
	rtn = 0;

	//
	//  Reactivate in-stasis ceilings...for certain types.
	//
	switch (type)
	{
	case fastCrushAndRaise:
	case crushAndRaise:
		P_ActivateInStasisCeiling(line);
	default:
		break;
	}

	while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if(sec->specialdata)
			continue;

		//
		// new door thinker
		//
		rtn = 1;
		ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
		P_AddThinker(&ceiling->thinker);
		sec->specialdata = ceiling;
		ceiling->thinker.function = T_MoveCeiling;
		ceiling->sector = sec;
		ceiling->crush = false;
		switch (type)
		{
		case fastCrushAndRaise:
			ceiling->crush = true;
			ceiling->topheight = sec->ceilingheight;
			ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
			ceiling->direction = -1;
			ceiling->speed = CEILSPEED * 2;
			break;
		case crushAndRaise:
			ceiling->crush = true;
			ceiling->topheight = sec->ceilingheight;
		case lowerAndCrush:
		case lowerToFloor:
			ceiling->bottomheight = sec->floorheight;
			if(type != lowerToFloor)
				ceiling->bottomheight += 8 * FRACUNIT;
			ceiling->direction = -1;
			ceiling->speed = CEILSPEED;
			break;
		case raiseToHighest:
			ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
			ceiling->direction = 1;
			ceiling->speed = CEILSPEED;
			break;
		}

		ceiling->tag = sec->tag;
		ceiling->type = type;
		P_AddActiveCeiling(ceiling);
	}
	return rtn;
}

//
// P_AddActiveCeiling()
//
// Adds a ceiling to the head of the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_AddActiveCeiling(ceiling_t* ceiling)
{
  ceilinglist_t *list = malloc(sizeof *list);
  list->ceiling = ceiling;
  ceiling->list = list;
  if ((list->next = activeceilings))
    list->next->prev = &list->next;
  list->prev = &activeceilings;
  activeceilings = list;
}

//
// P_RemoveActiveCeiling()
//
// Removes a ceiling from the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_RemoveActiveCeiling(ceiling_t* ceiling)
{
  ceilinglist_t *list = ceiling->list;
  ceiling->sector->specialdata = NULL;  //jff 2/22/98
  P_RemoveThinker(&ceiling->thinker);
  if ((*list->prev = list->next))
    list->next->prev = list->prev;
  free(list);
}

//
// P_RemoveAllActiveCeilings()
//
// Removes all ceilings from the active ceiling list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActiveCeilings(void)
{
  while (activeceilings)
  {
    ceilinglist_t *next = activeceilings->next;
    free(activeceilings);
    activeceilings = next;
  }
}

//
// P_ActivateInStasisCeiling()
//
// Reactivates all stopped crushers with the right tag
//
// Passed the line reactivating the crusher
// Returns true if a ceiling reactivated
//
//jff 4/5/98 return if activated
int P_ActivateInStasisCeiling(line_t *line)
{
  ceilinglist_t *cl;
  int rtn=0;

  for (cl=activeceilings; cl; cl=cl->next)
  {
    ceiling_t *ceiling = cl->ceiling;
    if (ceiling->tag == line->tag && ceiling->direction == 0)
    {
		ceiling->direction = ceiling->olddirection;
		ceiling->thinker.function	 = T_MoveCeiling;
		//jff 4/5/98 return if activated
        rtn=1;
    }
  }
  return rtn;
}

//
// EV_CeilingCrushStop()
//
// Stops all active ceilings with the right tag
//
// Passed the linedef stopping the ceilings
// Returns true if a ceiling put in stasis
//
int EV_CeilingCrushStop(line_t* line)
{
  int rtn=0;

  ceilinglist_t *cl;
  for (cl=activeceilings; cl; cl=cl->next)
  {
    ceiling_t *ceiling = cl->ceiling;
    if (ceiling->direction != 0 && ceiling->tag == line->tag)
    {
      ceiling->olddirection = ceiling->direction;
      ceiling->direction = 0;
      ceiling->thinker.function = NULL;
      rtn=1;
    }
  }
  return rtn;
}
