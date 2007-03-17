/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * p_plats.c : Elevators and platforms, raising/lowering.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JHEXEN__
plat_t *activeplats[MAXPLATS];
#else
platlist_t *activeplats;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Move a plat up and down
 *
 * @parm plat: ptr to the plat to move
 */
void T_PlatRaise(plat_t *plat)
{
    result_e res;

    switch(plat->status)
    {
#if __JHEXEN__
    case PLAT_UP:
#else
    case up:
#endif
        res = T_MovePlane(plat->sector, plat->speed, plat->high,
                          plat->crush, 0, 1);

        // Play a "while-moving" sound?
#if __JHERETIC__
        if(!(leveltime & 31))
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_stnmov);
#endif
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        if(plat->type == raiseAndChange ||
           plat->type == raiseToNearestAndChange)
        {
            if(!(leveltime & 7))
            {
# if __WOLFTC__
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltmov);
# else
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_stnmov);
# endif
            }
        }
#endif
        if(res == crushed && (!plat->crush))
        {
            plat->count = plat->wait;
#if __JHEXEN__
            plat->status = PLAT_DOWN;
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
            plat->status = down;
# if __DOOM64TC__
            if(plat->type != downWaitUpDoor) // d64tc added test
# endif
            {
# if __WOLFTC__
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstr);
# else
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstart);
# endif
            }
#endif
        }
        else
        {
            if(res == pastdest)
            {
                plat->count = plat->wait;
#if __JHEXEN__
                plat->status = PLAT_WAITING;
#else
                plat->status = waiting;
#endif

#if __JHEXEN__
                SN_StopSequenceInSec(plat->sector);
#elif __WOLFTC__
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstp);
#else
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstop);
#endif
                switch(plat->type)
                {
#if __JHEXEN__
                case PLAT_DOWNWAITUPSTAY:
                case PLAT_DOWNBYVALUEWAITUPSTAY:
#else
# if !__JHERETIC__
                case blazeDWUS:
                case raiseToNearestAndChange:
# endif
# if __DOOM64TC__
                case blazeDWUSplus16: // d64tc
                case downWaitUpDoor: // d64tc
# endif
                case downWaitUpStay:
                case raiseAndChange:
#endif
                    P_RemoveActivePlat(plat);
                    break;

                default:
                    break;
                }
            }
        }
        break;

#if __JHEXEN__
    case PLAT_DOWN:
#else
    case down:
#endif
        res =
            T_MovePlane(plat->sector, plat->speed, plat->low, false,0, -1);

        if(res == pastdest)
        {
            plat->count = plat->wait;
#if __JHEXEN__
            plat->status = PLAT_WAITING;
#else
            plat->status = waiting;
#endif
#if __JHEXEN__
            switch(plat->type)
            {
            case PLAT_UPWAITDOWNSTAY:
            case PLAT_UPBYVALUEWAITDOWNSTAY:
                P_RemoveActivePlat(plat);
                break;

            default:
                break;
            }
#elif __DOOM64TC__
            switch(plat->type)
            {
            case upWaitDownStay:
                P_RemoveActivePlat(plat);
                break;

            default:
                break;
            }
#endif
#if __JHEXEN__
            SN_StopSequenceInSec(plat->sector);
#elif __WOLFTC__
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstp);
#else
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstop);
#endif
        }
        else
        {
            // Play a "while-moving" sound?
#if __JHERETIC__
            if(!(leveltime & 31))
                S_SectorSound(plat->sector, SORG_FLOOR, sfx_stnmov);
#endif
        }
        break;

#if __JHEXEN__
    case PLAT_WAITING:
#else
    case waiting:
#endif
        if(!--plat->count)
        {
            if(P_GetFloatp(plat->sector, DMU_FLOOR_HEIGHT) == plat->low)
            {
#if __JHEXEN__
                plat->status = PLAT_UP;
#else
                plat->status = up;
#endif
            }
            else
            {
#if __JHEXEN__
                plat->status = PLAT_DOWN;
#else
                plat->status = down;
#endif
            }
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#elif __WOLFTC__
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pltstr);
#else
            S_SectorSound(plat->sector, SORG_FLOOR, sfx_pstart);
#endif
        }
        break;

#if !__JHEXEN__
    case in_stasis:
        break;
#endif
    }
}

/**
 * Do Platforms.
 *
 * @param amount: is only used for SOME platforms.
 */
#if __JHEXEN__
int EV_DoPlat(line_t *line, byte *args, plattype_e type, int amount)
#else
int EV_DoPlat(line_t *line, plattype_e type, int amount)
#endif
{
    int         rtn = 0;
    float       floorheight;
    plat_t     *plat;
    sector_t   *sec = NULL;
#if !__JHEXEN__
    sector_t   *frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
#endif
    xsector_t  *xsec;
    iterlist_t *list;

#if !__JHEXEN__
    // Activate all <type> plats that are in_stasis
    switch(type)
    {
    case perpetualRaise:
        P_ActivateInStasis(P_XLine(line)->tag);
        break;

    default:
        break;
    }
#endif

#if __JHEXEN__
    list = P_GetSectorIterListForTag((int) args[0], false);
#else
    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
#endif
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_XSector(sec);

        if(xsec->specialdata)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;
        plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
        P_AddThinker(&plat->thinker);

        plat->type = type;
        plat->sector = sec;

        xsec->specialdata = plat;
#if __JHERETIC__ || __JHEXEN__
        plat->thinker.function = T_PlatRaise;
#else
        plat->thinker.function = (actionf_p1) T_PlatRaise;
#endif
        plat->crush = false;

#if __JHEXEN__
        plat->tag = args[0];
        plat->speed = FIX2FLT(args[1] * (FRACUNIT / 8));
#else
        plat->tag = P_XLine(line)->tag;
#endif
        floorheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        switch(type)
        {
#if !__JHEXEN__
        case raiseToNearestAndChange:
            plat->speed = PLATSPEED * .5;

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            plat->high = P_FindNextHighestFloor(sec, floorheight);

            plat->wait = 0;
            plat->status = up;
            // NO MORE DAMAGE, IF APPLICABLE
            xsec->special = 0;

# if __WOLFTC__
            S_SectorSound(sec, SORG_FLOOR, sfx_pltmov);
# else
            S_SectorSound(sec, SORG_FLOOR, sfx_stnmov);
# endif
            break;

        case raiseAndChange:
            plat->speed = PLATSPEED * .5;

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            plat->high = floorheight + amount * FRACUNIT;
            plat->wait = 0;
            plat->status = up;

# if __WOLFTC__
            S_SectorSound(sec, SORG_FLOOR, sfx_pltmov);
# else
            S_SectorSound(sec, SORG_FLOOR, sfx_stnmov);
# endif
            break;
#endif
#if __JHEXEN__
        case PLAT_DOWNWAITUPSTAY:
#else
        case downWaitUpStay:
#endif
            plat->low = P_FindLowestFloorSurrounding(sec);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED * 4;
#endif
            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
#if __JHEXEN__
            plat->wait = args[2];
            plat->status = PLAT_DOWN;
#else
            plat->wait = 35 * PLATWAIT;
            plat->status = down;

# if __WOLFTC__
            S_SectorSound(sec, SORG_FLOOR, sfx_pltstr);
# else
            S_SectorSound(sec, SORG_FLOOR, sfx_pstart);
# endif
#endif
            break;
#if __DOOM64TC__
        case upWaitDownStay: // d64tc
            plat->speed = PLATSPEED * 8;
            plat->high = P_FindHighestFloorSurrounding(sec);

            if(plat->high < floorheight)
                plat->high = floorheight;

            plat->low = floorheight;
            plat->wait = 35 * PLATWAIT;
            plat->status = up;

            S_SectorSound(sec, SORG_FLOOR, sfx_pstart);
            break;

        case downWaitUpDoor: // d64tc
            plat->speed = PLATSPEED * 8;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;
            if(plat->low != floorheight)
                plat->low += 6;

            plat->high = floorheight;
            plat->wait = 50 * PLATWAIT;
            plat->status = down;
            break;
#endif
#if __JHEXEN__
       case PLAT_DOWNBYVALUEWAITUPSTAY:
            plat->low = floorheight - args[3] * 8;
            if(plat->low > floorheight)
                plat->low = floorheight;
            plat->high = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_DOWN;
            break;

        case PLAT_UPWAITDOWNSTAY:
            plat->high = P_FindHighestFloorSurrounding(sec);
            if(plat->high < floorheight)
                plat->high = floorheight;
            plat->low = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_UP;
            break;

        case PLAT_UPBYVALUEWAITDOWNSTAY:
            plat->high = floorheight + args[3] * 8;
            if(plat->high < floorheight)
                plat->high = floorheight;
            plat->low = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_UP;
            break;
#endif
#if __JDOOM__
        case blazeDWUS:
            plat->speed = PLATSPEED * 8;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
            plat->wait = 35 * PLATWAIT;
            plat->status = down;

# if __WOLFTC__
            S_SectorSound(sec, SORG_FLOOR, sfx_pltstr);
# else
            S_SectorSound(sec, SORG_FLOOR, sfx_pstart);
# endif
            break;
#endif
#if __JHEXEN__
        case PLAT_PERPETUALRAISE:
#else
        case perpetualRaise:
#endif
            plat->low = P_FindLowestFloorSurrounding(sec);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED;
#endif
            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = P_FindHighestFloorSurrounding(sec);

            if(plat->high < floorheight)
                plat->high = floorheight;

            plat->status = P_Random() & 1;
#if __JHEXEN__
            plat->wait = args[2];
#else
            plat->wait = 35 * PLATWAIT;

# if __WOLFTC__
            S_SectorSound(sec, SORG_FLOOR, sfx_pltstr);
# else
            S_SectorSound(sec, SORG_FLOOR, sfx_pstart);
# endif
#endif
            break;
        }

        P_AddActivePlat(plat);
#if __JHEXEN__
        SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#endif
    }
    return rtn;
}

/**
 * Activate a plat that has been put in stasis
 * (stopped perpetual floor, instant floor/ceil toggle)
 *
 * @parm tag: the tag of the plat that should be reactivated
 */
#if !__JHEXEN__
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
# if __JHERETIC__
            plat->thinker.function = T_PlatRaise;
# else
            plat->thinker.function = (actionf_p1) T_PlatRaise;
# endif
        }
    }
}
#endif

/**
 * Handler for "stop perpetual floor" linedef type.
 *
 * @parm line           Ptr to the line that stopped the plat.
 *
 * @return              <code>true</code> if the plat was put in stasis.
 */
#if __JHEXEN__
boolean EV_StopPlat(line_t *line, byte *args)
#else
boolean EV_StopPlat(line_t *line)
#endif
{
#if __JHEXEN__
    int         i;

    // Search the active plats...
    for(i = 0; i < MAXPLATS; ++i)
    {
        // ...for the one with the tag.
        if((activeplats[i])->tag == args[0])
        {
            // Destroy it.
            P_XSector((activeplats[i])->sector)->specialdata = NULL;
            P_TagFinished(P_XSector((activeplats[i])->sector)->tag);
            P_RemoveThinker(&(activeplats[i])->thinker);
            activeplats[i] = NULL;
            return false;
        }
    }
    return false;
#else
    platlist_t *pl;

    // Search the active plats...
    for(pl = activeplats; pl; pl = pl->next)
    {
        plat_t *plat = pl->plat;

        // ..for one with the tag and not in stasis.
        if(plat->status != in_stasis && plat->tag == P_XLine(line)->tag)
        {
            // Put it in stasis
            plat->oldstatus = plat->status;
            plat->status = in_stasis;
            plat->thinker.function = NOPFUNC;
        }
    }
    return true;
#endif
}

/**
 * Add a plat to the head of the active plat list.
 *
 * @param plat           Ptr to the plat to add.
 */
void P_AddActivePlat(plat_t *plat)
{
#if __JHEXEN__
    int         i;

    for(i = 0; i < MAXPLATS; ++i)
        if(activeplats[i] == NULL)
        {
            activeplats[i] = plat;
            return;
        }
    Con_Error("P_AddActivePlat: no more plats!");
#else
    platlist_t *list = malloc(sizeof *list);

    list->plat = plat;
    plat->list = list;

    if((list->next = activeplats) != NULL)
        list->next->prev = &list->next;

    list->prev = &activeplats;
    activeplats = list;
#endif
}

/**
 * Remove a plat from the active plat list.
 *
 * @parm plat           Ptr to the plat to remove.
 */
void P_RemoveActivePlat(plat_t *plat)
{
#if __JHEXEN__
    int         i;

    for(i = 0; i < MAXPLATS; ++i)
        if(plat == activeplats[i])
        {
            P_XSector((activeplats[i])->sector)->specialdata = NULL;
            P_TagFinished(P_XSector(plat->sector)->tag);
            P_RemoveThinker(&(activeplats[i])->thinker);
            activeplats[i] = NULL;
            return;
        }
    Con_Error("P_RemoveActivePlat: can't find plat!");
#else
    platlist_t *list = plat->list;

    P_XSector(plat->sector)->specialdata = NULL;

    P_RemoveThinker(&plat->thinker);

    if((*list->prev = list->next) != NULL)
        list->next->prev = list->prev;

    free(list);
#endif
}

/**
 * Remove all plats from the active plat list.
 */
#if !__JHEXEN__
void P_RemoveAllActivePlats(void)
{
    while(activeplats)
    {
        platlist_t *next = activeplats->next;

        free(activeplats);
        activeplats = next;
    }
}
#endif
