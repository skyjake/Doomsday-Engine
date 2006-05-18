/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Ceiling aninmation (lowering, crushing, raising)
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"

#include "Common/dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ceilinglist_t *activeceilings;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_MoveCeiling(ceiling_t * ceiling)
{
    result_e res;

    switch (ceiling->direction)
    {
    case 0:
        // IN STASIS
        break;
    case 1:
        // UP
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topheight,
                        false, 1, ceiling->direction);

        if(!(leveltime & 7))
        {
            switch (ceiling->type)
            {
            case silentCrushAndRaise:
                break;
            default:
                //S_StartSound((mobj_t*)&ceiling->sector->soundorg, sfx_stnmov);
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_stnmov);
                break;
            }
        }

        if(res == pastdest)
        {
            switch (ceiling->type)
            {
            case raiseToHighest:
                P_RemoveActiveCeiling(ceiling);
                break;

            case silentCrushAndRaise:
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pstop);
            case fastCrushAndRaise:
            case crushAndRaise:
                ceiling->direction = -1;
                break;

            default:
                break;
            }

        }
        break;

    case -1:
        // DOWN
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight,
                        ceiling->crush, 1, ceiling->direction);

        if(!(leveltime & 7))
        {
            switch (ceiling->type)
            {
            case silentCrushAndRaise:
                break;
            default:
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_stnmov);
            }
        }

        if(res == pastdest)
        {
            switch (ceiling->type)
            {
            case silentCrushAndRaise:
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pstop);
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
        }
        else                    // ( res != pastdest )
        {
            if(res == crushed)
            {
                switch (ceiling->type)
                {
                case silentCrushAndRaise:
                case crushAndRaise:
                case lowerAndCrush:
                    ceiling->speed = CEILSPEED / 8;
                    break;

                default:
                    break;
                }
            }
        }
        break;
    }
}

/*
 * Move a ceiling up/down.
 */
int EV_DoCeiling(line_t *line, ceiling_e type)
{
    int     secnum;
    int     rtn;
    xsector_t *xsec;
    sector_t *sec;
    ceiling_t *ceiling;

    secnum = -1;
    rtn = 0;

    //  Reactivate in-stasis ceilings...for certain types.
    switch (type)
    {
    case fastCrushAndRaise:
    case silentCrushAndRaise:
    case crushAndRaise:
        P_ActivateInStasisCeiling(line);
    default:
        break;
    }

    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);

        xsec = &xsectors[secnum];
        if(xsec->specialdata)
            continue;

        // new door thinker
        rtn = 1;
        ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
        P_AddThinker(&ceiling->thinker);
        xsec->specialdata = ceiling;
        ceiling->thinker.function = T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = false;

        switch (type)
        {
        case fastCrushAndRaise:
            ceiling->crush = true;
            ceiling->topheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight =
                P_GetFixedp(sec, DMU_FLOOR_HEIGHT) + (8 * FRACUNIT);

            ceiling->direction = -1;
            ceiling->speed = CEILSPEED * 2;
            break;

        case silentCrushAndRaise:
        case crushAndRaise:
            ceiling->crush = true;
            ceiling->topheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT);
        case lowerAndCrush:
        case lowerToFloor:
            ceiling->bottomheight = P_GetFixedp(sec, DMU_FLOOR_HEIGHT);
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

        ceiling->tag = xsec->tag;
        ceiling->type = type;
        P_AddActiveCeiling(ceiling);
    }
    return rtn;
}

/*
 * Adds a ceiling to the head of the list of active ceilings
 *
 * @param ceiling: ptr to the ceiling structure to be added
 */
void P_AddActiveCeiling(ceiling_t *ceiling)
{
    ceilinglist_t *list = malloc(sizeof *list);

    list->ceiling = ceiling;
    ceiling->list = list;

    if((list->next = activeceilings))
        list->next->prev = &list->next;

    list->prev = &activeceilings;
    activeceilings = list;
}

/*
 * Removes a ceiling from the list of active ceilings
 *
 * @param ceiling: ptr to the ceiling structure to be removed
 */
void P_RemoveActiveCeiling(ceiling_t *ceiling)
{
    ceilinglist_t *list = ceiling->list;

    P_XSector(ceiling->sector)->specialdata = NULL;
    P_RemoveThinker(&ceiling->thinker);

    if((*list->prev = list->next))
        list->next->prev = list->prev;

    free(list);
}

/*
 * Removes all ceilings from the active ceiling list
 */
void P_RemoveAllActiveCeilings(void)
{
    while(activeceilings)
    {
        ceilinglist_t *next = activeceilings->next;

        free(activeceilings);
        activeceilings = next;
    }
}

/*
 * Reactivates all stopped crushers with the right tag
 * Returns true if a ceiling reactivated
 *
 * @param line: ptr to the line reactivating the crusher
 */
int P_ActivateInStasisCeiling(line_t *line)
{
    int rtn = 0;
    ceilinglist_t *cl;

    for(cl = activeceilings; cl; cl = cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;

        if(ceiling->direction == 0 && ceiling->tag == P_XLine(line)->tag)
        {
            ceiling->direction = ceiling->olddirection;
            ceiling->thinker.function = (actionf_p1) T_MoveCeiling;

            // return true
            rtn = 1;
        }
    }
    return rtn;
}

/*
 * Stops all active ceilings with the right tag
 * Returns true if a ceiling put in stasis
 *
 * @param line: ptr to the line stopping the ceilings
 */
int EV_CeilingCrushStop(line_t *line)
{
    int rtn = 0;
    ceilinglist_t *cl;

    for(cl = activeceilings; cl; cl = cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;

        if(ceiling->direction != 0 && ceiling->tag == P_XLine(line)->tag)
        {
            ceiling->olddirection = ceiling->direction;
            ceiling->direction = 0;
            ceiling->thinker.function = NULL;

            // return true
            rtn = 1;
        }
    }
    return rtn;
}
