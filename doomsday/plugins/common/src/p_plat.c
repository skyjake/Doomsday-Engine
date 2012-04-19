/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 */

/**
 * p_plats.c : Elevators and platforms, raising/lowering.
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_tick.h"
#include "p_plat.h"

// MACROS ------------------------------------------------------------------

// Sounds played by the platforms when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'SFX_NONE'.
#if __JDOOM__
# define SFX_PLATFORMSTART      (SFX_PSTART)
# define SFX_PLATFORMMOVE       (SFX_STNMOV)
# define SFX_PLATFORMSTOP       (SFX_PSTOP)
#elif __JDOOM64__
# define SFX_PLATFORMSTART      (SFX_PSTART)
# define SFX_PLATFORMMOVE       (SFX_STNMOV)
# define SFX_PLATFORMSTOP       (SFX_PSTOP)
#elif __JHERETIC__
# define SFX_PLATFORMSTART      (SFX_PSTART)
# define SFX_PLATFORMMOVE       (SFX_STNMOV)
# define SFX_PLATFORMSTOP       (SFX_PSTOP)
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Called when a moving plat needs to be removed.
 *
 * @param plat          Ptr to the plat to remove.
 */
static void stopPlat(plat_t* plat)
{
    P_ToXSector(plat->sector)->specialData = NULL;
#if __JHEXEN__
    P_TagFinished(P_ToXSector(plat->sector)->tag);
#endif
    DD_ThinkerRemove(&plat->thinker);
}

/**
 * Move a plat up and down.
 *
 * @param plat          Ptr to the plat to be moved.
 */
void T_PlatRaise(plat_t* plat)
{
    result_e            res;

    switch(plat->state)
    {
    case PS_UP:
        res = T_MovePlane(plat->sector, plat->speed, plat->high,
                          plat->crush, 0, 1);

        // Play a "while-moving" sound?
#if __JHERETIC__
        if(!(mapTime & 31))
            S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
#endif
#if __JDOOM__ || __JDOOM64__
        if(plat->type == PT_RAISEANDCHANGE ||
           plat->type == PT_RAISETONEARESTANDCHANGE)
        {
            if(!(mapTime & 7))
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
        }
#endif
        if(res == crushed && (!plat->crush))
        {
            plat->count = plat->wait;
            plat->state = PS_DOWN;
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
# if __JDOOM64__
            if(plat->type != PT_DOWNWAITUPDOOR) // jd64 added test
# endif
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
        }
        else
        {
            if(res == pastdest)
            {
                plat->count = plat->wait;
                plat->state = PS_WAIT;
#if __JHEXEN__
                SN_StopSequenceInSec(plat->sector);
#else
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTOP);
#endif
                switch(plat->type)
                {
                case PT_DOWNWAITUPSTAY:
#if __JHEXEN__
                case PT_DOWNBYVALUEWAITUPSTAY:
#else
# if !__JHERETIC__
                case PT_DOWNWAITUPSTAYBLAZE:
                case PT_RAISETONEARESTANDCHANGE:
# endif
# if __JDOOM64__
                case PT_DOWNWAITUPPLUS16STAYBLAZE: // jd64
                case PT_DOWNWAITUPDOOR: // jd64
# endif
                case PT_RAISEANDCHANGE:
#endif
                    stopPlat(plat);
                    break;

                default:
                    break;
                }
            }
        }
        break;

    case PS_DOWN:
        res =
            T_MovePlane(plat->sector, plat->speed, plat->low, false, 0, -1);

        if(res == pastdest)
        {
            plat->count = plat->wait;
            plat->state = PS_WAIT;

#if __JHEXEN__ || __JDOOM64__
            switch(plat->type)
            {
# if __JHEXEN__
            case PT_UPBYVALUEWAITDOWNSTAY:
# endif
            case PT_UPWAITDOWNSTAY:
                stopPlat(plat);
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
            if(!(mapTime & 31))
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
#endif
        }
        break;

    case PS_WAIT:
        if(!--plat->count)
        {
            if(FEQUAL(P_GetDoublep(plat->sector, DMU_FLOOR_HEIGHT), plat->low))
                plat->state = PS_UP;
            else
                plat->state = PS_DOWN;
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
            S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
        }
        break;

    default:
        break;
    }
}

#if __JHEXEN__
static int doPlat(LineDef* line, int tag, byte* args, plattype_e type, int amount)
#else
static int doPlat(LineDef* line, int tag, plattype_e type, int amount)
#endif
{
    int rtn = 0;
    coord_t floorHeight;
    plat_t* plat;
    Sector* sec = NULL;
#if !__JHEXEN__
    Sector* frontSector = P_GetPtrp(line, DMU_FRONT_SECTOR);
#endif
    xsector_t* xsec;
    iterlist_t* list;

    list = P_GetSectorIterListForTag(tag, false);
    if(!list) return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        xsec = P_ToXSector(sec);

        if(xsec->specialData)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;

        plat = Z_Calloc(sizeof(*plat), PU_MAP, 0);
        plat->thinker.function = T_PlatRaise;
        DD_ThinkerAdd(&plat->thinker);

        plat->type = type;
        plat->sector = sec;

        xsec->specialData = plat;

        plat->crush = false;
        plat->tag = tag;
#if __JHEXEN__
        plat->speed = (float) args[1] * (1.0 / 8);
#endif
        floorHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
        switch(type)
        {
#if !__JHEXEN__
        case PT_RAISETONEARESTANDCHANGE:
            plat->speed = PLATSPEED * .5;

            P_SetPtrp(sec, DMU_FLOOR_MATERIAL,
                      P_GetPtrp(frontSector, DMU_FLOOR_MATERIAL));

            {
            coord_t nextFloor;
            if(P_FindSectorSurroundingNextHighestFloor(sec, floorHeight, &nextFloor))
                plat->high = nextFloor;
            else
                plat->high = floorHeight;
            }

            plat->wait = 0;
            plat->state = PS_UP;
            // No more damage if applicable.
            xsec->special = 0;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMMOVE);
            break;

        case PT_RAISEANDCHANGE:
            plat->speed = PLATSPEED * .5;

            P_SetPtrp(sec, DMU_FLOOR_MATERIAL,
                      P_GetPtrp(frontSector, DMU_FLOOR_MATERIAL));

            plat->high = floorHeight + amount;
            plat->wait = 0;
            plat->state = PS_UP;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMMOVE);
            break;
#endif
        case PT_DOWNWAITUPSTAY:
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED * 4;
#endif
            if(plat->low > floorHeight)
                plat->low = floorHeight;

            plat->high = floorHeight;
            plat->state = PS_DOWN;
#if __JHEXEN__
            plat->wait = (int) args[2];
#else
            plat->wait = PLATWAIT * TICSPERSEC;
#endif
#if !__JHEXEN__
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
            break;

#if __JDOOM64__ || __JHEXEN__
        case PT_UPWAITDOWNSTAY:
            P_FindSectorSurroundingHighestFloor(sec, -500, &plat->high);

            if(plat->high < floorHeight)
                plat->high = floorHeight;

            plat->low = floorHeight;
            plat->state = PS_UP;
# if __JHEXEN__
            plat->wait = (int) args[2];
# else
            plat->wait = PLATWAIT * TICSPERSEC;
# endif
# if __JDOOM64__
            plat->speed = PLATSPEED * 8;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
# endif
            break;
#endif
#if __JDOOM64__
        case PT_DOWNWAITUPDOOR: // jd64
            plat->speed = PLATSPEED * 8;
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);

            if(plat->low > floorHeight)
                plat->low = floorHeight;
            if(plat->low != floorHeight)
                plat->low += 6;

            plat->high = floorHeight;
            plat->wait = 50 * PLATWAIT;
            plat->state = PS_DOWN;
            break;
#endif
#if __JHEXEN__
       case PT_DOWNBYVALUEWAITUPSTAY:
            plat->low = floorHeight - (coord_t) args[3] * 8;
            if(plat->low > floorHeight)
                plat->low = floorHeight;
            plat->high = floorHeight;
            plat->wait = (int) args[2];
            plat->state = PS_DOWN;
            break;

        case PT_UPBYVALUEWAITDOWNSTAY:
            plat->high = floorHeight + (coord_t) args[3] * 8;
            if(plat->high < floorHeight)
                plat->high = floorHeight;
            plat->low = floorHeight;
            plat->wait = (int) args[2];
            plat->state = PS_UP;
            break;
#endif
#if __JDOOM__ || __JDOOM64__
        case PT_DOWNWAITUPSTAYBLAZE:
            plat->speed = PLATSPEED * 8;
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);

            if(plat->low > floorHeight)
                plat->low = floorHeight;

            plat->high = floorHeight;
            plat->wait = PLATWAIT * TICSPERSEC;
            plat->state = PS_DOWN;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
            break;
#endif
        case PT_PERPETUALRAISE:
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED;
#endif
            if(plat->low > floorHeight)
                plat->low = floorHeight;

            P_FindSectorSurroundingHighestFloor(sec, -500, &plat->high);

            if(plat->high < floorHeight)
                plat->high = floorHeight;

            plat->state = P_Random() & 1;
#if __JHEXEN__
            plat->wait = (int) args[2];
#else
            plat->wait = PLATWAIT * TICSPERSEC;
#endif
#if !__JHEXEN__
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
#endif
            break;

        default:
            break;
        }

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
int EV_DoPlat(LineDef *line, byte *args, plattype_e type, int amount)
#else
int EV_DoPlat(LineDef *line, plattype_e type, int amount)
#endif
{
#if __JHEXEN__
    return doPlat(line, (int) args[0], args, type, amount);
#else
    int                 rtn = 0;
    xline_t*            xline = P_ToXLine(line);

    // Activate all <type> plats that are in stasis.
    switch(type)
    {
    case PT_PERPETUALRAISE:
        rtn = P_PlatActivate(xline->tag);
        break;

    default:
        break;
    }

    return doPlat(line, xline->tag, type, amount) || rtn;
#endif
}

#if !__JHEXEN__
typedef struct {
    short               tag;
    int                 count;
} activateplatparams_t;

static int activatePlat(thinker_t* th, void* context)
{
    plat_t*             plat = (plat_t*) th;
    activateplatparams_t* params = (activateplatparams_t*) context;

    if(plat->tag == (int) params->tag && plat->thinker.inStasis)
    {
        plat->state = plat->oldState;
        DD_ThinkerSetStasis(&plat->thinker, false);
        params->count++;
    }

    return false; // Contiue iteration.
}

/**
 * Activate a plat that has been put in stasis
 * (stopped perpetual floor, instant floor/ceil toggle)
 *
 * @param tag           Tag of plats that should be reactivated.
 */
int P_PlatActivate(short tag)
{
    activateplatparams_t params;

    params.tag = tag;
    params.count = 0;
    DD_IterateThinkers(T_PlatRaise, activatePlat, &params);

    return params.count;
}
#endif

typedef struct {
    short               tag;
    int                 count;
} deactivateplatparams_t;

static int deactivatePlat(thinker_t* th, void* context)
{
    plat_t*             plat = (plat_t*) th;
    deactivateplatparams_t* params = (deactivateplatparams_t*) context;

#if __JHEXEN__
    // For THE one with the tag.
    if(plat->tag == params->tag)
    {
        // Destroy it.
        stopPlat(plat);
        params->count++;
        return true; // Stop iteration.
    }
#else
    // For one with the tag and not in stasis.
    if(plat->tag == (int) params->tag && !plat->thinker.inStasis)
    {
        // Put it in stasis.
        plat->oldState = plat->state;
        DD_ThinkerSetStasis(&plat->thinker, true);
        params->count++;
    }
#endif

    return false; // Continue iteration.
}

/**
 * Handler for "stop perpetual floor" linedef type.
 *
 * @param tag           Tag of plats to put into stasis.
 *
 * @return              Number of plats put into stasis.
 */
int P_PlatDeactivate(short tag)
{
    deactivateplatparams_t params;

    params.tag = tag;
    params.count = 0;
    DD_IterateThinkers(T_PlatRaise, deactivatePlat, &params);

    return params.count;
}
