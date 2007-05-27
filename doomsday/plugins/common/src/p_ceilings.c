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
 * p_ceilings.c : Moving ceilings (lowering, crushing, raising).
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
#include "p_start.h"

// MACROS ------------------------------------------------------------------

// Sounds played by the ceilings when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'sfx_None'.
#if __WOLFTC__
# define SFX_CEILINGMOVE        (sfx_pltmov)
# define SFX_CEILINGSTOP        (sfx_pltstp)
#elif __DOOM64TC__
# define SFX_CEILINGMOVE        (sfx_stnmov)
# define SFX_CEILINGSTOP        (sfx_pstop)
#elif __JDOOM__
# define SFX_CEILINGMOVE        (sfx_stnmov)
# define SFX_CEILINGSTOP        (sfx_pstop)
#elif __JHERETIC__
# define SFX_CEILINGMOVE        (sfx_dormov)
# define SFX_CEILINGSTOP        (sfx_none)
#elif __JHEXEN__
# define SFX_CEILINGMOVE        (SFX_NONE)
# define SFX_CEILINGSTOP        (SFX_NONE)
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ceilinglist_t *activeceilings;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_MoveCeiling(ceiling_t *ceiling)
{
    result_e res;

    switch(ceiling->direction)
    {
#if !__JHEXEN__
    case 0: // In stasis.
        break;
#endif
    case 1: // Going up.
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topheight,
                        false, 1, ceiling->direction);

        // Play a "while-moving" sound?
#if !__JHEXEN__
        if(!(leveltime & 7))
        {
# if __JHERETIC__
            S_SectorSound(ceiling->sector, SORG_CEILING, SFX_CEILINGMOVE);
# else
            switch(ceiling->type)
            {
            case silentCrushAndRaise:
                break;
            default:
                S_SectorSound(ceiling->sector, SORG_CEILING, SFX_CEILINGMOVE);
                break;
            }
# endif
        }
#endif

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
#endif
            switch(ceiling->type)
            {
#if !__JHEXEN__
            case raiseToHighest:
# if __DOOM64TC__
            case customCeiling: //d64tc
# endif
                P_RemoveActiveCeiling(ceiling);
                break;
# if !__JHERETIC__
            case silentCrushAndRaise:
                S_SectorSound(ceiling->sector, SORG_CEILING, SFX_CEILINGSTOP);
# endif
            case fastCrushAndRaise:
#endif
            case crushAndRaise:
                ceiling->direction = -1;
#if __JHEXEN__
                ceiling->speed = ceiling->speed * 2;
#endif
                break;

            default:
#if __JHEXEN__
                P_RemoveActiveCeiling(ceiling);
#endif
                break;
            }

        }
        break;

    case -1: // Going down.
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight,
                        ceiling->crush, 1, ceiling->direction);

        // Play a "while-moving" sound?
#if !__JHEXEN__
        if(!(leveltime & 7))
        {
# if __JHERETIC__
            S_SectorSound(ceiling->sector, SORG_CEILING, SFX_CEILINGMOVE);
# else
            switch(ceiling->type)
            {
            case silentCrushAndRaise:
                break;
            default:
                S_SectorSound(ceiling->sector, SORG_CEILING, SFX_CEILINGMOVE);
            }
# endif
        }
#endif

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
#endif
            switch(ceiling->type)
            {
#if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
            case silentCrushAndRaise:
                S_SectorSound(ceiling->sector, SORG_CEILING, SFX_CEILINGSTOP);
                ceiling->speed = CEILSPEED;
                ceiling->direction = 1;
                break;
#endif

            case crushAndRaise:
#if __JHEXEN__
            case crushRaiseAndStay:
#endif
#if __JHEXEN__
                ceiling->speed = ceiling->speed * .5;
#else
                ceiling->speed = CEILSPEED;
#endif
                ceiling->direction = 1;
                break;
#if !__JHEXEN__
            case fastCrushAndRaise:
                ceiling->direction = 1;
                break;

            case lowerAndCrush:
            case lowerToFloor:
# if __DOOM64TC__
            case customCeiling: //d64tc
# endif
                P_RemoveActiveCeiling(ceiling);
                break;
#endif

            default:
#if __JHEXEN__
                P_RemoveActiveCeiling(ceiling);
#endif
                break;
            }
        }
        else                    // ( res != pastdest )
        {
            if(res == crushed)
            {
                switch(ceiling->type)
                {
#if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
                case silentCrushAndRaise:
#endif
                case crushAndRaise:
                case lowerAndCrush:
#if __JHEXEN__
                case crushRaiseAndStay:
#endif
#if !__JHEXEN__
                    ceiling->speed = CEILSPEED * .125;
#endif
                    break;

                default:
                    break;
                }
            }
        }
        break;
    }
}

#if __DOOM64TC__
static int EV_DoCeiling2(line_t *line, int tag, float basespeed,
                         ceiling_e type)
#elif __JHEXEN__
static int EV_DoCeiling2(byte *arg, int tag, float basespeed, ceiling_e type)
#else
static int EV_DoCeiling2(int tag, float basespeed, ceiling_e type)
#endif
{
    int         rtn = 0;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    ceiling_t  *ceiling;
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

        // new door thinker
        rtn = 1;
        ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
        P_AddThinker(&ceiling->thinker);
        xsec->specialdata = ceiling;
        ceiling->thinker.function = T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = false;
        ceiling->speed = basespeed;

        switch(type)
        {
#if __JDOOM__ || __JHERETIC__ || __WOLFTC__ || __DOOM64TC__
        case fastCrushAndRaise:
            ceiling->crush = true;
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;

            ceiling->direction = -1;
            ceiling->speed *= 2;
            break;
#endif
#if __JHEXEN__
        case crushRaiseAndStay:
            ceiling->crush = (int) arg[2];    // arg[2] = crushing value
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;
            ceiling->direction = -1;
            break;
#endif
#if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
        case silentCrushAndRaise:
#endif
        case crushAndRaise:
#if !__JHEXEN__
            ceiling->crush = true;
#endif
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);

        case lowerAndCrush:
#if __JHEXEN__
            ceiling->crush = (int) arg[2];    // arg[2] = crushing value
#endif
        case lowerToFloor:
            ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);

            if(type != lowerToFloor)
                ceiling->bottomheight += 8;
            ceiling->direction = -1;
#if __DOOM64TC__
            ceiling->speed *= 8; // d64tc
#endif
            break;

        case raiseToHighest:
            ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
#if __DOOM64TC__
            ceiling->topheight -= 8;   // d64tc
#endif
            ceiling->direction = 1;
            break;
#if __DOOM64TC__
        case customCeiling: // d64tc
            {
            //bitmip? wha?
            side_t *front = P_GetPtrp(line, DMU_SIDE0);
            side_t *back = P_GetPtrp(line, DMU_SIDE1);
            float bitmipL = 0, bitmipR = 0;

            bitmipL = P_GetFloatp(front, DMU_MIDDLE_TEXTURE_OFFSET_X);
            if(back)
                bitmipR = P_GetFloatp(back, DMU_MIDDLE_TEXTURE_OFFSET_X);

            if(bitmipR > 0)
            {
                ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
                ceiling->direction = 1;
                ceiling->speed *= bitmipL;
                ceiling->topheight -= bitmipR;
            }
            else
            {
                ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
                ceiling->bottomheight -= bitmipR;
                ceiling->direction = -1;
                ceiling->speed *= bitmipL;
            }
            }
#endif
#if __JHEXEN__
        case lowerByValue:
            ceiling->bottomheight =
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) - (float) arg[2];
            ceiling->direction = -1;
            break;

        case raiseByValue:
            ceiling->topheight =
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) + (float) arg[2];
            ceiling->direction = 1;
            break;

        case moveToValueTimes8:
            {
            float   destHeight = (float) arg[2] * 8;

            if(arg[3]) // Going down?
                destHeight = -destHeight;

            if(P_GetFloatp(sec, DMU_CEILING_HEIGHT) <= destHeight)
            {
                ceiling->direction = 1;
                ceiling->topheight = destHeight;
                if(P_GetFloatp(sec, DMU_CEILING_HEIGHT) == destHeight)
                    rtn = 0;
            }
            else if(P_GetFloatp(sec, DMU_CEILING_HEIGHT) > destHeight)
            {
                ceiling->direction = -1;
                ceiling->bottomheight = destHeight;
            }
            break;
            }
#endif
        default:
#if __JHEXEN__
            rtn = 0;
#endif
            break;
        }

        ceiling->tag = xsec->tag;
        ceiling->type = type;
        P_AddActiveCeiling(ceiling);

#if __JHEXEN__
        if(rtn)
        {
            SN_StartSequence(P_SectorSoundOrigin(ceiling->sector),
                             SEQ_PLATFORM + P_XSector(ceiling->sector)->seqType);
        }
#endif
    }
    return rtn;
}

/**
 * Move a ceiling up/down.
 */
#if __JHEXEN__
int EV_DoCeiling(line_t *line, byte *args, ceiling_e type)
#else
int EV_DoCeiling(line_t *line, ceiling_e type)
#endif
{
#if __JHEXEN__
    return EV_DoCeiling2(args, (int) args[0], (float) args[1] * (1.0 / 8),
                         type);
#else
    int         rtn = 0;
    // Reactivate in-stasis ceilings...for certain types.
    switch(type)
    {
    case fastCrushAndRaise:
# if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
    case silentCrushAndRaise:
# endif
    case crushAndRaise:
        rtn = P_ActivateInStasisCeiling(line);
        break;

    default:
        break;
    }
# if __DOOM64TC__
    return EV_DoCeiling2(line, P_XLine(line)->tag, CEILSPEED, type) || rtn;
# else
    return EV_DoCeiling2(P_XLine(line)->tag, CEILSPEED, type) || rtn;
# endif
#endif
}

/**
 * Adds a ceiling to the head of the list of active ceilings.
 *
 * @param ceiling       Ptr to the ceiling structure to be added.
 */
void P_AddActiveCeiling(ceiling_t *ceiling)
{
    ceilinglist_t *list = malloc(sizeof *list);

    list->ceiling = ceiling;
    ceiling->list = list;

    if((list->next = activeceilings) != NULL)
        list->next->prev = &list->next;

    list->prev = &activeceilings;
    activeceilings = list;
}

/**
 * Removes a ceiling from the list of active ceilings.
 *
 * @param ceiling       Ptr to the ceiling structure to be removed.
 */
void P_RemoveActiveCeiling(ceiling_t *ceiling)
{
    ceilinglist_t *list = ceiling->list;

    P_XSector(ceiling->sector)->specialdata = NULL;
#if __JHEXEN__
    P_TagFinished(P_XSector(ceiling->sector)->tag);
#endif
    P_RemoveThinker(&ceiling->thinker);

    if((*list->prev = list->next) != NULL)
        list->next->prev = list->prev;

    free(list);
}

/**
 * Removes all ceilings from the active ceiling list.
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

/**
 * Reactivates all stopped crushers with the right tag.
 *
 * @param line          Ptr to the line reactivating the crusher.
 *
 * @return              <code>true</code> if a ceiling is reactivated.
 */
#if !__JHEXEN__
int P_ActivateInStasisCeiling(line_t *line)
{
    int         rtn = 0;
    xline_t    *xline = P_XLine(line);
    ceilinglist_t *cl;

    for(cl = activeceilings; cl; cl = cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;

        if(ceiling->direction == 0 && ceiling->tag == xline->tag)
        {
            ceiling->direction = ceiling->olddirection;
            ceiling->thinker.function = T_MoveCeiling;
            rtn = 1;
        }
    }
    return rtn;
}
#endif

static int EV_CeilingCrushStop2(int tag)
{
    int         rtn = 0;
    ceilinglist_t *cl;

    for(cl = activeceilings; cl; cl = cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;

#if __JHEXEN__
        if(ceiling->tag == tag)
        {   // Destroy it.
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
            P_RemoveActiveCeiling(ceiling);
            rtn = 1;
            break;
        }
#else
        if(ceiling->direction != 0 && ceiling->tag == tag)
        {   // Put it into stasis.
            ceiling->olddirection = ceiling->direction;
            ceiling->direction = 0;
            ceiling->thinker.function = INSTASIS;
            rtn = 1;
        }
#endif
    }

    return rtn;
}

/**
 * Stops all active ceilings with the right tag.
 *
 * @param line          Ptr to the line stopping the ceilings.
 *
 * @return              <code>true</code> if a ceiling put in stasis.
 */
#if __JHEXEN__
int EV_CeilingCrushStop(line_t *line, byte *args)
#else
int EV_CeilingCrushStop(line_t *line)
#endif
{
#if __JHEXEN__
    return EV_CeilingCrushStop2((int) args[0]);
#else
    return EV_CeilingCrushStop2(P_XLine(line)->tag);
#endif
}
