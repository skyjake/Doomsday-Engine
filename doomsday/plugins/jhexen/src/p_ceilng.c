/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 *  p_ceilng.c : Moving ceilings.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"
#include "p_start.h"
#include "dmu_lib.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ceiling_t *activeceilings[MAXCEILINGS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_MoveCeiling(ceiling_t *ceiling)
{
    result_e res;

    switch(ceiling->direction)
    {
        //      case 0:         // IN STASIS
        //          break;
    case 1:                 // UP
        res = T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topheight,
                          false, 1, ceiling->direction);
        if(res == RES_PASTDEST)
        {
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
            switch(ceiling->type)
            {
            case CLEV_CRUSHANDRAISE:
                ceiling->direction = -1;
                ceiling->speed = ceiling->speed * 2;
                break;

            default:
                P_RemoveActiveCeiling(ceiling);
                break;
            }
        }
        break;

    case -1:                    // DOWN
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight,
                        ceiling->crush, 1, ceiling->direction);
        if(res == RES_PASTDEST)
        {
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
            switch(ceiling->type)
            {
            case CLEV_CRUSHANDRAISE:
            case CLEV_CRUSHRAISEANDSTAY:
                ceiling->direction = 1;
                ceiling->speed = ceiling->speed / 2;
                break;

            default:
                P_RemoveActiveCeiling(ceiling);
                break;
            }
        }
        else if(res == RES_CRUSHED)
        {
            switch(ceiling->type)
            {
            case CLEV_CRUSHANDRAISE:
            case CLEV_LOWERANDCRUSH:
            case CLEV_CRUSHRAISEANDSTAY:
                //ceiling->speed = ceiling->speed/4;
                break;

            default:
                break;
            }
        }
        break;
    }
}

/**
 * Move a ceiling up/down and all around!
 */
int EV_DoCeiling(line_t *line, byte *arg, ceiling_e type)
{
    int         rtn = 0;
    sector_t   *sec = NULL;
    ceiling_t  *ceiling;
    iterlist_t *list;

    list = P_GetSectorIterListForTag((int) arg[0], false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue;

        rtn = 1;

        ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
        P_AddThinker(&ceiling->thinker);
        P_XSector(sec)->specialdata = ceiling;
        ceiling->thinker.function = T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = 0;
        ceiling->speed = FIX2FLT(arg[1] * (FRACUNIT / 8));

        switch(type)
        {
        case CLEV_CRUSHRAISEANDSTAY:
            ceiling->crush = arg[2];    // arg[2] = crushing value
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;
            ceiling->direction = -1;
            break;

        case CLEV_CRUSHANDRAISE:
            ceiling->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
        case CLEV_LOWERANDCRUSH:
            ceiling->crush = arg[2];    // arg[2] = crushing value
        case CLEV_LOWERTOFLOOR:
            ceiling->bottomheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
            if(type != CLEV_LOWERTOFLOOR)
            {
                ceiling->bottomheight += 8;
            }
            ceiling->direction = -1;
            break;

        case CLEV_RAISETOHIGHEST:
            ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
            ceiling->direction = 1;
            break;

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

        default:
            rtn = 0;
            break;
        }

        ceiling->tag = P_XSector(sec)->tag;
        ceiling->type = type;

        P_AddActiveCeiling(ceiling);

        if(rtn)
        {
            SN_StartSequence(P_SectorSoundOrigin(ceiling->sector),
                             SEQ_PLATFORM + P_XSector(ceiling->sector)->seqType);
        }
    }
    return rtn;
}

void P_AddActiveCeiling(ceiling_t *c)
{
    int         i;

    for(i = 0; i < MAXCEILINGS; ++i)
        if(activeceilings[i] == NULL)
        {
            activeceilings[i] = c;
            return;
        }
}

void P_RemoveActiveCeiling(ceiling_t *c)
{
    int         i;

    for(i = 0; i < MAXCEILINGS; ++i)
        if(activeceilings[i] == c)
        {
            P_XSector(activeceilings[i]->sector)->specialdata = NULL;
            P_RemoveThinker(&activeceilings[i]->thinker);
            P_TagFinished(P_XSector(activeceilings[i]->sector)->tag);
            activeceilings[i] = NULL;
            break;
        }
}

/**
 * Stop a ceiling from crushing!
 */
int EV_CeilingCrushStop(line_t *line, byte *args)
{
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
}
