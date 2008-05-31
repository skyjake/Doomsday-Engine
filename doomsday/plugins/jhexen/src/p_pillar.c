/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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

/**
 * p_pillar.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_iterlist.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_BuildPillar(pillar_t *pillar)
{
    result_e            res1;
    result_e            res2;

    // First, raise the floor
    res1 = T_MovePlane(pillar->sector, pillar->floorSpeed, pillar->floorDest, pillar->crush, 0, pillar->direction); // floorOrCeiling, direction
    // Then, lower the ceiling
    res2 =
        T_MovePlane(pillar->sector, pillar->ceilingSpeed, pillar->ceilingDest,
                    pillar->crush, 1, -pillar->direction);
    if(res1 == pastdest && res2 == pastdest)
    {
        P_ToXSector(pillar->sector)->specialData = NULL;
        SN_StopSequence(P_GetPtrp(pillar->sector, DMU_SOUND_ORIGIN));
        P_TagFinished(P_ToXSector(pillar->sector)->tag);
        P_ThinkerRemove(&pillar->thinker);
    }
}

int EV_BuildPillar(linedef_t *line, byte *args, boolean crush)
{
    int                 rtn = 0;
    float               newHeight;
    sector_t           *sec = NULL;
    pillar_t           *pillar;
    iterlist_t         *list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going.

        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) ==
           P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            continue; // Pillar is already closed.

        rtn = 1;
        if(!args[2])
        {
            newHeight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) +
                ((P_GetFloatp(sec, DMU_CEILING_HEIGHT) -
                  P_GetFloatp(sec, DMU_FLOOR_HEIGHT)) * .5f);
        }
        else
        {
            newHeight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + (float) args[2];
        }

        pillar = Z_Calloc(sizeof(*pillar), PU_LEVSPEC, 0);
        P_ToXSector(sec)->specialData = pillar;
        P_ThinkerAdd(&pillar->thinker);
        pillar->thinker.function = T_BuildPillar;
        pillar->sector = sec;

        if(!args[2])
        {
            pillar->ceilingSpeed = pillar->floorSpeed =
                (float) args[1] * (1.0f / 8);
        }
        else if(newHeight - P_GetFloatp(sec, DMU_FLOOR_HEIGHT) >
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newHeight)
        {
            pillar->floorSpeed = (float) args[1] * (1.0f / 8);
            pillar->ceilingSpeed =
                (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newHeight) *
                      (pillar->floorSpeed / (newHeight - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0f / 8);
            pillar->floorSpeed =
                (newHeight - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed /
                                  (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newHeight));
        }

        pillar->floorDest = newHeight;
        pillar->ceilingDest = newHeight;
        pillar->direction = 1;
        pillar->crush = crush * (int) args[3];
        SN_StartSequence(P_GetPtrp(pillar->sector, DMU_SOUND_ORIGIN),
                         SEQ_PLATFORM + P_ToXSector(pillar->sector)->seqType);
    }
    return rtn;
}

int EV_OpenPillar(linedef_t *line, byte *args)
{
    int                 rtn = 0;
    sector_t           *sec = NULL;
    pillar_t           *pillar;
    iterlist_t         *list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going...

        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) !=
           P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            continue; // Pillar isn't closed.

        rtn = 1;
        pillar = Z_Calloc(sizeof(*pillar), PU_LEVSPEC, 0);
        P_ToXSector(sec)->specialData = pillar;
        P_ThinkerAdd(&pillar->thinker);
        pillar->thinker.function = T_BuildPillar;
        pillar->sector = sec;
        if(!args[2])
        {
            P_FindSectorSurroundingLowestFloor(sec, &pillar->floorDest);
        }
        else
        {
            pillar->floorDest =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) - (float) args[2];
        }

        if(!args[3])
        {
            P_FindSectorSurroundingHighestCeiling(sec, &pillar->ceilingDest);
        }
        else
        {
            pillar->ceilingDest =
                P_GetFloatp(sec, DMU_CEILING_HEIGHT) + (float) args[3];
        }

        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) - pillar->floorDest >=
           pillar->ceilingDest - P_GetFloatp(sec, DMU_CEILING_HEIGHT))
        {
            pillar->floorSpeed = (float) args[1] * (1.0f / 8);
            pillar->ceilingSpeed =
                (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - pillar->ceilingDest) *
                    (pillar->floorSpeed /
                        (pillar->floorDest - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0f / 8);
            pillar->floorSpeed =
                (pillar->floorDest - P_GetFloatp(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed /
                        (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - pillar->ceilingDest));
        }

        pillar->direction = -1; // Open the pillar.
        SN_StartSequence(P_GetPtrp(pillar->sector, DMU_SOUND_ORIGIN),
                         SEQ_PLATFORM + P_ToXSector(pillar->sector)->seqType);
    }

    return rtn;
}
