/**\file
 *\section License
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

// Sounds played by the platforms when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'sfx_None'.
#if __WOLFTC__
# define SFX_PLATFORMSTART      (sfx_pltstr)
# define SFX_PLATFORMMOVE       (sfx_pltmov)
# define SFX_PLATFORMSTOP       (sfx_pltstp)
#elif __DOOM64TC__
# define SFX_PLATFORMSTART      (sfx_pstart)
# define SFX_PLATFORMMOVE       (sfx_stnmov)
# define SFX_PLATFORMSTOP       (sfx_pstop)
#elif __JDOOM__
# define SFX_PLATFORMSTART      (sfx_pstart)
# define SFX_PLATFORMMOVE       (sfx_stnmov)
# define SFX_PLATFORMSTOP       (sfx_pstop)
#elif __JHERETIC__
# define SFX_PLATFORMSTART      (sfx_pstart)
# define SFX_PLATFORMMOVE       (sfx_stnmov)
# define SFX_PLATFORMSTOP       (sfx_pstop)
#elif __JHEXEN__
# define SFX_PLATFORMSTART      (SFX_NONE)
# define SFX_PLATFORMMOVE       (SFX_NONE)
# define SFX_PLATFORMSTOP       (SFX_NONE)
#endif

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
 * Move a plat up and down.
 *
 * @param plat          Ptr to the plat to be moved.
 */
void T_PlatRaise(plat_t *plat)
{
    result_e res;

    switch(plat->status)
    {
    case up:
        res = T_MovePlane(plat->sector, plat->speed, plat->high,
                          plat->crush, 0, 1);

        // Play a "while-moving" sound?
#if __JHERETIC__
        if(!(leveltime & 31))
            S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
#endif
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        if(plat->type == raiseAndChange ||
           plat->type == raiseToNearestAndChange)
        {
            if(!(leveltime & 7))
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
        }
#endif
        if(res == crushed && (!plat->crush))
        {
            plat->count = plat->wait;
            plat->status = down;
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
# if __DOOM64TC__
            if(plat->type != downWaitUpDoor) // d64tc added test
# endif
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
        }
        else
        {
            if(res == pastdest)
            {
                plat->count = plat->wait;
                plat->status = waiting;
#if __JHEXEN__
                SN_StopSequenceInSec(plat->sector);
#else
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTOP);
#endif
                switch(plat->type)
                {
                case downWaitUpStay:
#if __JHEXEN__
                case downByValueWaitUpStay:
#else
# if !__JHERETIC__
                case blazeDWUS:
                case raiseToNearestAndChange:
# endif
# if __DOOM64TC__
                case blazeDWUSplus16: // d64tc
                case downWaitUpDoor: // d64tc
# endif
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

    case down:
        res =
            T_MovePlane(plat->sector, plat->speed, plat->low, false,0, -1);

        if(res == pastdest)
        {
            plat->count = plat->wait;
            plat->status = waiting;

#if __JHEXEN__ || __DOOM64TC__
            switch(plat->type)
            {
# if __JHEXEN__
            case upByValueWaitDownStay:
# endif
            case upWaitDownStay:
                P_RemoveActivePlat(plat);
                break;

            default:
                break;
            }
#endif

#if __JHEXEN__
            SN_StopSequenceInSec(plat->sector);
#else
            S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTOP);
#endif
        }
        else
        {
            // Play a "while-moving" sound?
#if __JHERETIC__
            if(!(leveltime & 31))
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
#endif
        }
        break;

    case waiting:
        if(!--plat->count)
        {
            if(P_GetFloatp(plat->sector, DMU_FLOOR_HEIGHT) == plat->low)
                plat->status = up;
            else
                plat->status = down;
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
            S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
        }
        break;

#if !__JHEXEN__
    case in_stasis:
        break;
#endif
    }
}

#if __JHEXEN__
static int EV_DoPlat2(line_t *line, int tag, byte *args, plattype_e type,
                      int amount)
#else
static int EV_DoPlat2(line_t *line, int tag, plattype_e type, int amount)
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

    list = P_GetSectorIterListForTag(tag, false);
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
        plat->thinker.function = T_PlatRaise;

        plat->crush = false;
        plat->tag = tag;
#if __JHEXEN__
        plat->speed = (float) args[1] * (1.0 / 8);
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
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMMOVE);
            break;

        case raiseAndChange:
            plat->speed = PLATSPEED * .5;

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            plat->high = floorheight + amount * FRACUNIT;
            plat->wait = 0;
            plat->status = up;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMMOVE);
            break;
#endif
        case downWaitUpStay:
            plat->low = P_FindLowestFloorSurrounding(sec);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED * 4;
#endif
            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
            plat->status = down;
#if __JHEXEN__
            plat->wait = (int) args[2];
#else
            plat->wait = PLATWAIT * TICSPERSEC;
#endif
#if !__JHEXEN__
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
            break;

#if __DOOM64TC__ || __JHEXEN__
        case upWaitDownStay:
            plat->high = P_FindHighestFloorSurrounding(sec);

            if(plat->high < floorheight)
                plat->high = floorheight;

            plat->low = floorheight;
            plat->status = up;
# if __JHEXEN__
            plat->wait = (int) args[2];
# else
            plat->wait = PLATWAIT * TICSPERSEC;
# endif
# if __DOOM64TC__
            plat->speed = PLATSPEED * 8;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
# endif
            break;
#endif
#if __DOOM64TC__
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
       case downByValueWaitUpStay:
            plat->low = floorheight - (float) args[3] * 8;
            if(plat->low > floorheight)
                plat->low = floorheight;
            plat->high = floorheight;
            plat->wait = (int) args[2];
            plat->status = down;
            break;

        case upByValueWaitDownStay:
            plat->high = floorheight + (float) args[3] * 8;
            if(plat->high < floorheight)
                plat->high = floorheight;
            plat->low = floorheight;
            plat->wait = (int) args[2];
            plat->status = up;
            break;
#endif
#if __JDOOM__
        case blazeDWUS:
            plat->speed = PLATSPEED * 8;
            plat->low = P_FindLowestFloorSurrounding(sec);

            if(plat->low > floorheight)
                plat->low = floorheight;

            plat->high = floorheight;
            plat->wait = PLATWAIT * TICSPERSEC;
            plat->status = down;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
            break;
#endif
        case perpetualRaise:
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
            plat->wait = (int) args[2];
#else
            plat->wait = PLATWAIT * TICSPERSEC;
#endif
#if !__JHEXEN__
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
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
#if __JHEXEN__
    return EV_DoPlat2(line, (int) args[0], args, type, amount);
#else
    int         rtn = 0;
    xline_t    *xline = P_XLine(line);

    // Activate all <type> plats that are in_stasis
    switch(type)
    {
    case perpetualRaise:
        rtn = P_ActivateInStasisPlat(xline->tag);
        break;

    default:
        break;
    }

    return EV_DoPlat2(line, xline->tag, type, amount) || rtn;
#endif
}

/**
 * Activate a plat that has been put in stasis
 * (stopped perpetual floor, instant floor/ceil toggle)
 *
 * @parm tag: the tag of the plat that should be reactivated
 */
#if !__JHEXEN__
int P_ActivateInStasisPlat(int tag)
{
    int         rtn = 0;
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
            rtn = 1;
        }
    }
    return rtn;
}
#endif


static boolean EV_StopPlat2(int tag)
{
    platlist_t *pl;

    // Search the active plats...
    for(pl = activeplats; pl; pl = pl->next)
    {
        plat_t *plat = pl->plat;

#if __JHEXEN__
        // ...for THE one with the tag.
        if(plat->tag == tag)
        {
            // Destroy it.
            P_RemoveActivePlat(plat);
            return false;
        }
#else
        // ..for one with the tag and not in stasis.
        if(plat->status != in_stasis && plat->tag == tag)
        {
            // Put it in stasis
            plat->oldstatus = plat->status;
            plat->status = in_stasis;
            plat->thinker.function = INSTASIS;
        }
#endif
    }

#if __JHEXEN__
    return false;
#else
    return true;
#endif
}

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
    return EV_StopPlat2((int) args[0]);
#else
    return EV_StopPlat2(P_XLine(line)->tag);
#endif
}

/**
 * Add a plat to the head of the active plat list.
 *
 * @param plat           Ptr to the plat to add.
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
 * @parm plat           Ptr to the plat to remove.
 */
void P_RemoveActivePlat(plat_t *plat)
{
    platlist_t *list = plat->list;

    P_XSector(plat->sector)->specialdata = NULL;
#if __JHEXEN__
    P_TagFinished(P_XSector(plat->sector)->tag);
#endif
    P_RemoveThinker(&plat->thinker);

    if((*list->prev = list->next) != NULL)
        list->next->prev = list->prev;

    free(list);
}

/**
 * Remove all plats from the active plat list.
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
