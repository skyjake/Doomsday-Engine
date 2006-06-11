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
 * Plats (i.e. elevator platforms) code, raising/lowering.
 *  2006/01/17 DJS - Recreated using jDoom's p_plats.c as a base.
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_stat.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

platlist_t *activeplats;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Move a plat up and down
 *
 * @parm plat: ptr to the plat to move
 */
void T_PlatRaise(plat_t * plat)
{
    result_e res;

    switch (plat->status)
    {
    case up:
        res = T_MovePlane(plat->sector, plat->speed, plat->high,
                          plat->crush, 0, 1);

        if(!(leveltime & 31))
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_stnmov);

        if(plat->type == raiseAndChange ||
           plat->type == raiseToNearestAndChange)
        {
            if(!(leveltime & 7))
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_stnmov);
        }

        if(res == crushed && (!plat->crush))
        {
            plat->count = plat->wait;
            plat->status = down;
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstart);
        }
        else
        {
            if(res == pastdest)
            {
                plat->count = plat->wait;
                plat->status = waiting;
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstop);

                switch (plat->type)
                {
                case downWaitUpStay:
                    P_RemoveActivePlat(plat);
                    break;

                case raiseAndChange:
                    P_RemoveActivePlat(plat);
                    break;

                default:
                    break;
                }
            }
        }
        break;

    case down:
        res = T_MovePlane(plat->sector, plat->speed,
                          plat->low, false, 0, -1);

        if(res == pastdest)
        {
            plat->count = plat->wait;
            plat->status = waiting;
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstop);
        }
        else
        {
            if(!(leveltime & 31))
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_stnmov);
        }
        break;

    case waiting:
        if(!--plat->count)
        {
            if(P_GetFixedp(plat->sector, DMU_FLOOR_HEIGHT) == plat->low)
                plat->status = up;
            else
                plat->status = down;
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstart);
        }
        break;

    case in_stasis:
        break;
    }
}

/*
 * Do Platforms.
 *
 * @param amount: is only used for SOME platforms.
 */
int EV_DoPlat(line_t *line, plattype_e type, int amount)
{
    plat_t *plat;
    int     secnum;
    int     rtn;
    fixed_t floorheight;
    sector_t *sec;
    sector_t*   frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

    secnum = -1;
    rtn = 0;

    //  Activate all <type> plats that are in_stasis
    switch (type)
    {
    case perpetualRaise:
        P_ActivateInStasis(P_XLine(line)->tag);
        break;

    default:
        break;
    }

    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);

        if(xsectors[secnum].specialdata)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;
        plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
        P_AddThinker(&plat->thinker);

        plat->type = type;
        plat->sector = P_ToPtr(DMU_SECTOR, secnum);

        xsectors[secnum].specialdata = plat;

        plat->thinker.function = T_PlatRaise;
        plat->crush = false;

        plat->tag = P_XLine(line)->tag;

        floorheight = P_GetFixed(DMU_SECTOR, secnum, DMU_FLOOR_HEIGHT);
        switch(type)
        {
        case raiseToNearestAndChange:
            plat->speed = PLATSPEED / 2;

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            plat->high = P_FindNextHighestFloor(sec, floorheight);

            plat->wait = 0;
            plat->status = up;
            // NO MORE DAMAGE, IF APPLICABLE
            xsectors[secnum].special = 0;

            S_SectorSound(sec, SORG_FLOOR, sfx_stnmov);
            break;

        case raiseAndChange:
            plat->speed = PLATSPEED / 2;

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            plat->high = floorheight + amount * FRACUNIT;
            plat->wait = 0;
            plat->status = up;

            S_SectorSound(sec, SORG_FLOOR, sfx_stnmov);
            break;

        case downWaitUpStay:
            plat->speed = PLATSPEED * 4;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
            plat->wait = 35 * PLATWAIT;
            plat->status = down;

            S_SectorSound(sec, SORG_FLOOR, sfx_pstart);
            break;

        case perpetualRaise:
            plat->speed = PLATSPEED;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = P_FindHighestFloorSurrounding(sec);

            if(plat->high < floorheight)
                plat->high = floorheight;

            plat->wait = 35 * PLATWAIT;
            plat->status = P_Random() & 1;

            S_SectorSound(sec, SORG_FLOOR, sfx_pstart);
            break;
        }
        P_AddActivePlat(plat);
    }
    return rtn;
}

/*
 * Activate a plat that has been put in stasis
 * (stopped perpetual floor, instant floor/ceil toggle)
 *
 * @parm tag: the tag of the plat that should be reactivated
 */
void P_ActivateInStasis(int tag)
{
    platlist_t *pl;

    // search the active plats
    for(pl = activeplats; pl; pl = pl->next)
    {
        plat_t *plat = pl->plat;

        // for one in stasis with right tag
        if(plat->tag == tag && plat->status == in_stasis)
        {
            plat->status = plat->oldstatus;
            plat->thinker.function = T_PlatRaise;
        }
    }
}

/*
 * Handler for "stop perpetual floor" linedef type
 * Returns true if a plat was put in stasis
 *
 * @parm line: ptr to the line that stopped the plat
 */
int EV_StopPlat(line_t *line)
{
    platlist_t *pl;

    // search the active plats
    for(pl = activeplats; pl; pl = pl->next)
    {
        plat_t *plat = pl->plat;

        // for one with the tag not in stasis
        if(plat->status != in_stasis && plat->tag == P_XLine(line)->tag)
        {
            // put it in stasis
            plat->oldstatus = plat->status;
            plat->status = in_stasis;
            plat->thinker.function = NULL;
        }
    }
    return 1;
}

/*
 * Add a plat to the head of the active plat list
 *
 * @parm plat: ptr to the plat to add
 */
void P_AddActivePlat(plat_t *plat)
{
    platlist_t *list = malloc(sizeof *list);

    list->plat = plat;
    plat->list = list;

    if((list->next = activeplats))
        list->next->prev = &list->next;

    list->prev = &activeplats;
    activeplats = list;
}

/*
 * Remove a plat from the active plat list
 *
 * @parm plat: ptr to the plat to remove
 */
void P_RemoveActivePlat(plat_t *plat)
{
    platlist_t *list = plat->list;

    P_XSector(plat->sector)->specialdata = NULL;

    P_RemoveThinker(&plat->thinker);

    if((*list->prev = list->next))
        list->next->prev = list->prev;

    free(list);
}

/*
 *Remove all plats from the active plat list
 */
void P_RemoveAllActivePlats(void)
{
    while(activeplats)
    {
        platlist_t *next = activeplats->next;

        free(activeplats);
        activeplats = next;
    }
}
