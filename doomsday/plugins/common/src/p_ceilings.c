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

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JHEXEN__
ceiling_t *activeceilings[MAXCEILINGS];
#else
ceilinglist_t *activeceilings;
#endif

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
            S_SectorSound(ceiling->sector, SORG_CEILING, sfx_dormov);
# else
            switch(ceiling->type)
            {
            case silentCrushAndRaise:
                break;
            default:
#  if __WOLFTC__
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pltmov);
#  else
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_stnmov);
#  endif
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
#  if __WOLFTC__
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pltstp);
#  else
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pstop);
#  endif
# endif
            case fastCrushAndRaise:
#endif
#if __JHEXEN__
            case CLEV_CRUSHANDRAISE:
#else
            case crushAndRaise:
#endif
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
            S_SectorSound(ceiling->sector, SORG_CEILING, sfx_dormov);
# else
            switch(ceiling->type)
            {
            case silentCrushAndRaise:
                break;
            default:
#  if __WOLFTC__
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pltmov);
#  else
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_stnmov);
#  endif
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
# if __WOLFTC__
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pltstp);
# else
                S_SectorSound(ceiling->sector, SORG_CEILING, sfx_pstop);
# endif
                ceiling->speed = CEILSPEED;
                ceiling->direction = 1;
                break;
#endif
#if __JHEXEN__
            case CLEV_CRUSHANDRAISE:
            case CLEV_CRUSHRAISEANDSTAY:
#else
            case crushAndRaise:
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
#if __JHEXEN__
                case CLEV_CRUSHANDRAISE:
                case CLEV_LOWERANDCRUSH:
                case CLEV_CRUSHRAISEANDSTAY:
#else
                case crushAndRaise:
                case lowerAndCrush:
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

/**
 * Move a ceiling up/down.
 */
#if __JHEXEN__
int EV_DoCeiling(line_t *line, byte *arg, ceiling_e type)
#else
int EV_DoCeiling(line_t *line, ceiling_e type)
#endif
{
    int         rtn = 0;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    ceiling_t  *ceiling;
    iterlist_t *list;

#if !__JHEXEN__
    // Reactivate in-stasis ceilings...for certain types.
    switch(type)
    {
    case fastCrushAndRaise:
# if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
    case silentCrushAndRaise:
# endif
    case crushAndRaise:
        P_ActivateInStasisCeiling(line);
        break;

    default:
        break;
    }
#endif

#if __JHEXEN__
    list = P_GetSectorIterListForTag((int) arg[0], false);
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

        // new door thinker
        rtn = 1;
        ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
        P_AddThinker(&ceiling->thinker);
        xsec->specialdata = ceiling;
        ceiling->thinker.function = T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = false;
#if __JHEXEN__
        ceiling->speed = FIX2FLT(arg[1] * (FRACUNIT / 8));
#endif
        switch(type)
        {
#if __JDOOM__ || __JHERETIC__ || __WOLFTC__ || __DOOM64TC__
        case fastCrushAndRaise:
            ceiling->crush = true;
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;

            ceiling->direction = -1;
            ceiling->speed = CEILSPEED * 2;
            break;
#endif
#if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
        case silentCrushAndRaise:
#endif
#if __JHEXEN__
        case CLEV_CRUSHRAISEANDSTAY:
            ceiling->crush = arg[2];    // arg[2] = crushing value
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;
            ceiling->direction = -1;
            break;
#endif
#if __JHEXEN__
        case CLEV_CRUSHANDRAISE:
#else
        case crushAndRaise:
#endif
#if !__JHEXEN__
            ceiling->crush = true;
#endif
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
#if __JHEXEN__
        case CLEV_LOWERANDCRUSH:
#else
        case lowerAndCrush:
#endif
#if __JHEXEN__
            ceiling->crush = arg[2];    // arg[2] = crushing value
#endif
#if __JHEXEN__
        case CLEV_LOWERTOFLOOR:
#else
        case lowerToFloor:
#endif
            ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
#if __JHEXEN__
            if(type != CLEV_LOWERTOFLOOR)
#else
            if(type != lowerToFloor)
#endif
                ceiling->bottomheight += 8;
            ceiling->direction = -1;
#if __DOOM64TC__
            ceiling->speed = CEILSPEED * 8; // d64tc
#elif __JDOOM__ || __JHERETIC__ || __WOLFTC__
            ceiling->speed = CEILSPEED;
#endif
            break;

#if __JHEXEN__
        case CLEV_RAISETOHIGHEST:
#else
        case raiseToHighest:
#endif
            ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
#if __DOOM64TC__
            ceiling->topheight -= 8;   // d64tc
#endif
            ceiling->direction = 1;
#if !__JHEXEN__
            ceiling->speed = CEILSPEED;
#endif
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
                ceiling->speed = CEILSPEED * bitmipL;
                ceiling->topheight -= bitmipR;
            }
            else
            {
                ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
                ceiling->bottomheight -= bitmipR;
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED * bitmipL;
            }
            }
#endif
#if __JHEXEN__
        case CLEV_LOWERBYVALUE:
            ceiling->bottomheight =
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) - arg[2];
            ceiling->direction = -1;
            break;

        case CLEV_RAISEBYVALUE:
            ceiling->topheight =
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) + arg[2];
            ceiling->direction = 1;
            break;

        case CLEV_MOVETOVALUETIMES8:
            {
            float   destHeight = arg[2] * 8;

            if(arg[3])
            {
                destHeight = -destHeight;
            }
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
 * Adds a ceiling to the head of the list of active ceilings.
 *
 * @param ceiling       Ptr to the ceiling structure to be added.
 */
void P_AddActiveCeiling(ceiling_t *ceiling)
{
#if __JHEXEN__
    int         i;

    for(i = 0; i < MAXCEILINGS; ++i)
        if(activeceilings[i] == NULL)
        {
            activeceilings[i] = ceiling;
            return;
        }
#else
    ceilinglist_t *list = malloc(sizeof *list);

    list->ceiling = ceiling;
    ceiling->list = list;

    if((list->next = activeceilings) != NULL)
        list->next->prev = &list->next;

    list->prev = &activeceilings;
    activeceilings = list;
#endif
}

/**
 * Removes a ceiling from the list of active ceilings.
 *
 * @param ceiling       Ptr to the ceiling structure to be removed.
 */
void P_RemoveActiveCeiling(ceiling_t *ceiling)
{
#if __JHEXEN__
    int         i;

    for(i = 0; i < MAXCEILINGS; ++i)
        if(activeceilings[i] == ceiling)
        {
            P_XSector(activeceilings[i]->sector)->specialdata = NULL;
            P_RemoveThinker(&activeceilings[i]->thinker);
            P_TagFinished(P_XSector(activeceilings[i]->sector)->tag);
            activeceilings[i] = NULL;
            break;
        }
#else
    ceilinglist_t *list = ceiling->list;

    P_XSector(ceiling->sector)->specialdata = NULL;
    P_RemoveThinker(&ceiling->thinker);

    if((*list->prev = list->next) != NULL)
        list->next->prev = list->prev;

    free(list);
#endif
}

/**
 * Removes all ceilings from the active ceiling list.
 */
#if !__JHEXEN__
void P_RemoveAllActiveCeilings(void)
{
    while(activeceilings)
    {
        ceilinglist_t *next = activeceilings->next;

        free(activeceilings);
        activeceilings = next;
    }
}
#endif

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
    ceilinglist_t *cl;

    for(cl = activeceilings; cl; cl = cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;

        if(ceiling->direction == 0 && ceiling->tag == P_XLine(line)->tag)
        {
            ceiling->direction = ceiling->olddirection;
# if __JHERETIC__
            ceiling->thinker.function = T_MoveCeiling;
# else
            ceiling->thinker.function = (actionf_p1) T_MoveCeiling;
# endif
            // return true
            rtn = 1;
        }
    }
    return rtn;
}
#endif

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
    int         i;
    int         rtn;

    rtn = 0;
    for(i = 0; i < MAXCEILINGS; ++i)
    {
        if(activeceilings[i] && activeceilings[i]->tag == args[0])
        {
            rtn = 1;
            SN_StopSequence(P_SectorSoundOrigin(activeceilings[i]->sector));
            P_XSector(activeceilings[i]->sector)->specialdata = NULL;
            P_RemoveThinker(&activeceilings[i]->thinker);
            P_TagFinished(P_XSector(activeceilings[i]->sector)->tag);
            activeceilings[i] = NULL;
            break;
        }
    }
    return rtn;
#else
    int         rtn = 0;
    ceilinglist_t *cl;

    for(cl = activeceilings; cl; cl = cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;

        if(ceiling->direction != 0 && ceiling->tag == P_XLine(line)->tag)
        {
            ceiling->olddirection = ceiling->direction;
            ceiling->direction = 0;
            ceiling->thinker.function = NOPFUNC;

            // return true
            rtn = 1;
        }
    }
    return rtn;
#endif
}
