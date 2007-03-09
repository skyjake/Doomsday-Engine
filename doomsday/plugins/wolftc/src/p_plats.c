/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * Plats (i.e. elevator platforms) code, raising/lowering.
 */

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "dmu_lib.h"
#include "p_mapspec.h"

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

/**
 * Move a plat up and down
 *
 * @param plat              Ptr to the plat to move
 */
void T_PlatRaise(plat_t *plat)
{
    result_e res;

    switch(plat->status)
    {
    case up:
        res = T_MovePlane(plat->sector, plat->speed, plat->high,
                          plat->crush, 0, 1);

        if(plat->type == raiseAndChange ||
           plat->type == raiseToNearestAndChange)
        {
            if(!(leveltime & 7))
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltmov);
        }

        if(res == crushed && (!plat->crush))
        {
            plat->count = plat->wait;
            plat->status = down;
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstr);
        }
        else
        {
            if(res == pastdest)
            {
                plat->count = plat->wait;
                plat->status = waiting;
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstp);

                switch(plat->type)
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
        res = T_MovePlane(plat->sector, plat->speed,
                          plat->low, false, 0, -1);

        if(res == pastdest)
        {
            plat->count = plat->wait;
            plat->status = waiting;
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstp);
        }
        break;

    case waiting:
        if(!--plat->count)
        {
            if(P_GetFloatp(plat->sector, DMU_FLOOR_HEIGHT) == plat->low)
                plat->status = up;
            else
                plat->status = down;
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstr);
        }
        break;

    case in_stasis:
        break;
    }
}

/**
 * Do Platforms.
 *
 * @param amount            Is only used for SOME platforms.
 */
int EV_DoPlat(line_t *line, plattype_e type, int amount)
{
    int         rtn = 0;
    float       floorheight;
    plat_t     *plat;
    sector_t   *sec = NULL;
    sector_t   *frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
    iterlist_t *list;

    // Activate all <type> plats that are in_stasis
    switch(type)
    {
    case perpetualRaise:
        P_ActivateInStasis(P_XLine(line)->tag);
        break;

    default:
        break;
    }

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;
        plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
        P_AddThinker(&plat->thinker);

        plat->type = type;
        plat->sector = sec;

        P_XSector(sec)->specialdata = plat;

        plat->thinker.function = (actionf_p1) T_PlatRaise;
        plat->crush = false;

        plat->tag = P_XLine(line)->tag;

        floorheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
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
            P_XSector(sec)->special = 0;

            S_SectorSound(sec, SORG_FLOOR, sfx_pltmov);
            break;

        case raiseAndChange:
            plat->speed = PLATSPEED / 2;

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            plat->high = floorheight + amount * FRACUNIT;
            plat->wait = 0;
            plat->status = up;

            S_SectorSound(sec, SORG_FLOOR, sfx_pltmov);
            break;

        case downWaitUpStay:
            plat->speed = PLATSPEED * 4;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
            plat->wait = 35 * PLATWAIT;
            plat->status = down;

            S_SectorSound(sec, SORG_FLOOR, sfx_pltstr);
            break;

        case blazeDWUS:
            plat->speed = PLATSPEED * 8;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
            plat->wait = 35 * PLATWAIT;
            plat->status = down;

            S_SectorSound(sec, SORG_FLOOR, sfx_pltstr);
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

            S_SectorSound(sec, SORG_FLOOR, sfx_pltstr);
            break;
        }
        P_AddActivePlat(plat);
    }
    return rtn;
}

/**
 * Activate a plat that has been put in stasis
 * (stopped perpetual floor, instant floor/ceil toggle).
 *
 * @param tag               The tag of the plat that should be reactivated.
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
            plat->thinker.function = (actionf_p1) T_PlatRaise;
        }
    }
}

/**
 * Handler for "stop perpetual floor" linedef type.
 *
 * @param line              Ptr to the line that stopped the plat.
 *
 * @return                  <code>true</code> if a plat was put in stasis.
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
            plat->thinker.function = NOPFUNC;
        }
    }
    return 1;
}

/**
 * Add a plat to the head of the active plat list.
 *
 * @param plat              Ptr to the plat to add.
 */
void P_AddActivePlat(plat_t *plat)
{
    platlist_t *list = malloc(sizeof *list);

    list->plat = plat;
    plat->list = list;

    if((list->next = activeplats) != NULL)
        list->next->prev = &list->next;

    list->prev = &activeplats;
    activeplats = list;
}

/**
 * Remove a plat from the active plat list.
 *
 * @param plat              Ptr to the plat to remove.
 */
void P_RemoveActivePlat(plat_t *plat)
{
    platlist_t *list = plat->list;

    P_XSector(plat->sector)->specialdata = NULL;

    P_RemoveThinker(&plat->thinker);

    if((*list->prev = list->next) != NULL)
        list->next->prev = list->prev;

    free(list);
}

/**
 * Remove all plats from the active plat list
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
