/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * p_plats.c : Elevators and platforms, raising/lowering.
 */

// HEADER FILES ------------------------------------------------------------

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
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

// MACROS ------------------------------------------------------------------

// Sounds played by the platforms when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'sfx_None'.
#if __WOLFTC__
# define SFX_PLATFORMSTART      (sfx_pltstr)
# define SFX_PLATFORMMOVE       (sfx_pltmov)
# define SFX_PLATFORMSTOP       (sfx_pltstp)
#elif __JDOOM__
# define SFX_PLATFORMSTART      (sfx_pstart)
# define SFX_PLATFORMMOVE       (sfx_stnmov)
# define SFX_PLATFORMSTOP       (sfx_pstop)
#elif __JDOOM64__
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Called when a moving plat needs to be removed.
 *
 * @parm plat           Ptr to the plat to remove.
 */
static void stopPlat(plat_t *plat)
{
    P_ToXSector(plat->sector)->specialData = NULL;
#if __JHEXEN__
    P_TagFinished(P_ToXSector(plat->sector)->tag);
#endif
    P_ThinkerRemove(&plat->thinker);
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
        if(!(levelTime & 31))
            S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
#endif
#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
        if(plat->type == raiseAndChange ||
           plat->type == raiseToNearestAndChange)
        {
            if(!(levelTime & 7))
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
            if(plat->type != downWaitUpDoor) // jd64 added test
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
                case downWaitUpStay:
#if __JHEXEN__
                case downByValueWaitUpStay:
#else
# if !__JHERETIC__
                case blazeDWUS:
                case raiseToNearestAndChange:
# endif
# if __JDOOM64__
                case blazeDWUSplus16: // jd64
                case downWaitUpDoor: // jd64
# endif
                case raiseAndChange:
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
            case upByValueWaitDownStay:
# endif
            case upWaitDownStay:
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
            if(!(levelTime & 31))
                S_SectorSound(plat->sector, SORG_FLOOR, SFX_PLATFORMMOVE);
#endif
        }
        break;

    case PS_WAIT:
        if(!--plat->count)
        {
            if(P_GetFloatp(plat->sector, DMU_FLOOR_HEIGHT) == plat->low)
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
static int EV_DoPlat2(linedef_t *line, int tag, byte *args, plattype_e type,
                      int amount)
#else
static int EV_DoPlat2(linedef_t *line, int tag, plattype_e type, int amount)
#endif
{
    int                 rtn = 0;
    float               floorHeight;
    plat_t             *plat;
    sector_t           *sec = NULL;
#if !__JHEXEN__
    sector_t           *frontSector = P_GetPtrp(line, DMU_FRONT_SECTOR);
#endif
    xsector_t          *xsec;
    iterlist_t         *list;

    list = P_GetSectorIterListForTag(tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_ToXSector(sec);

        if(xsec->specialData)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;
        plat = Z_Calloc(sizeof(*plat), PU_LEVSPEC, 0);
        plat->thinker.function = T_PlatRaise;
        P_ThinkerAdd(&plat->thinker);

        plat->type = type;
        plat->sector = sec;

        xsec->specialData = plat;

        plat->crush = false;
        plat->tag = tag;
#if __JHEXEN__
        plat->speed = (float) args[1] * (1.0 / 8);
#endif
        floorHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        switch(type)
        {
#if !__JHEXEN__
        case raiseToNearestAndChange:
            plat->speed = PLATSPEED * .5;

            P_SetIntp(sec, DMU_FLOOR_MATERIAL,
                      P_GetIntp(frontSector, DMU_FLOOR_MATERIAL));

            {
            float               nextFloor;
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

        case raiseAndChange:
            plat->speed = PLATSPEED * .5;

            P_SetIntp(sec, DMU_FLOOR_MATERIAL,
                      P_GetIntp(frontSector, DMU_FLOOR_MATERIAL));

            plat->high = floorHeight + amount;
            plat->wait = 0;
            plat->state = PS_UP;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMMOVE);
            break;
#endif
        case downWaitUpStay:
            P_FindSectorSurroundingLowestFloor(sec, &plat->low);
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
        case upWaitDownStay:
            P_FindSectorSurroundingHighestFloor(sec, &plat->high);

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
        case downWaitUpDoor: // jd64
            plat->speed = PLATSPEED * 8;
            P_FindSectorSurroundingLowestFloor(sec, &plat->low);

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
       case downByValueWaitUpStay:
            plat->low = floorHeight - (float) args[3] * 8;
            if(plat->low > floorHeight)
                plat->low = floorHeight;
            plat->high = floorHeight;
            plat->wait = (int) args[2];
            plat->state = PS_DOWN;
            break;

        case upByValueWaitDownStay:
            plat->high = floorHeight + (float) args[3] * 8;
            if(plat->high < floorHeight)
                plat->high = floorHeight;
            plat->low = floorHeight;
            plat->wait = (int) args[2];
            plat->state = PS_UP;
            break;
#endif
#if __JDOOM__ || __JDOOM64__
        case blazeDWUS:
            plat->speed = PLATSPEED * 8;
            P_FindSectorSurroundingLowestFloor(sec, &plat->low);

            if(plat->low > floorHeight)
                plat->low = floorHeight;

            plat->high = floorHeight;
            plat->wait = PLATWAIT * TICSPERSEC;
            plat->state = PS_DOWN;
            S_SectorSound(sec, SORG_FLOOR, SFX_PLATFORMSTART);
            break;
#endif
        case perpetualRaise:
            P_FindSectorSurroundingLowestFloor(sec, &plat->low);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED;
#endif
            if(plat->low > floorHeight)
                plat->low = floorHeight;

            P_FindSectorSurroundingHighestFloor(sec, &plat->high);

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
int EV_DoPlat(linedef_t *line, byte *args, plattype_e type, int amount)
#else
int EV_DoPlat(linedef_t *line, plattype_e type, int amount)
#endif
{
#if __JHEXEN__
    return EV_DoPlat2(line, (int) args[0], args, type, amount);
#else
    int         rtn = 0;
    xline_t    *xline = P_ToXLine(line);

    // Activate all <type> plats that are in_stasis
    switch(type)
    {
    case perpetualRaise:
        rtn = P_PlatActivate(xline->tag);
        break;

    default:
        break;
    }

    return EV_DoPlat2(line, xline->tag, type, amount) || rtn;
#endif
}

#if !__JHEXEN__
typedef struct {
    short               tag;
    int                 count;
} activateplatparams_t;

static boolean activatePlat(thinker_t* th, void* context)
{
    plat_t*             plat = (plat_t*) th;
    activateplatparams_t* params = (activateplatparams_t*) context;

    if(plat->tag == (int) params->tag && plat->thinker.inStasis)
    {
        plat->state = plat->oldState;
        P_ThinkerSetStasis(&plat->thinker, false);
        params->count++;
    }

    return true; // Contiue iteration.
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
    P_IterateThinkers(T_PlatRaise, activatePlat, &params);

    return params.count;
}
#endif

typedef struct {
    short               tag;
} deactivateplatparams_t;

static boolean deactivatePlat(thinker_t* th, void* context)
{
    plat_t*             plat = (plat_t*) th;
    deactivateplatparams_t* params = (deactivateplatparams_t*) context;

#if __JHEXEN__
    // For THE one with the tag.
    if(plat->tag == params->tag)
    {
        // Destroy it.
        stopPlat(plat);
        return false; // Stop iteration.
    }
#else
    // For one with the tag and not in stasis.
    if(plat->tag == (int) params->tag && !plat->thinker.inStasis)
    {
        // Put it in stasis.
        plat->oldState = plat->state;
        P_ThinkerSetStasis(&plat->thinker, true);
    }
#endif

    return true; // Continue iteration.
}

/**
 * Handler for "stop perpetual floor" linedef type.
 *
 * @param tag           Tag of plats to put into stasis.
 *
 * @return              @c true, if a plat was put in stasis.
 */
#if __JHEXEN__
int P_PlatDeactivate(linedef_t *line, byte *args)
#else
int P_PlatDeactivate(short tag)
#endif
{
#if __JHEXEN__
    int                 tag = (int) args[0];
#endif
    deactivateplatparams_t params;

    params.tag = tag;
    P_IterateThinkers(T_PlatRaise, deactivatePlat, &params);

#if __JHEXEN__
    return false;
#else
    return true;
#endif
}
