/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
    res2 = T_MovePlane(pillar->sector, pillar->ceilingSpeed, pillar->ceilingDest, pillar->crush, 1, -pillar->direction);
    if(res1 == pastdest && res2 == pastdest)
    {
        P_ToXSector(pillar->sector)->specialData = NULL;
        SN_StopSequenceInSec(pillar->sector);
        P_TagFinished(P_ToXSector(pillar->sector)->tag);
        Thinker_Remove(&pillar->thinker);
    }
}

int EV_BuildPillar(LineDef* line, byte* args, boolean crush)
{
    int rtn = 0;
    coord_t newHeight;
    Sector* sec = NULL;
    pillar_t* pillar;
    iterlist_t* list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list) return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going.

        if(FEQUAL(P_GetDoublep(sec, DMU_FLOOR_HEIGHT),
                  P_GetDoublep(sec, DMU_CEILING_HEIGHT)))
            continue; // Pillar is already closed.

        rtn = 1;
        if(!args[2])
        {
            newHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) +
                ((P_GetDoublep(sec, DMU_CEILING_HEIGHT) -
                  P_GetDoublep(sec, DMU_FLOOR_HEIGHT)) * .5f);
        }
        else
        {
            newHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + (coord_t) args[2];
        }

        pillar = Z_Calloc(sizeof(*pillar), PU_MAP, 0);
        pillar->thinker.function = T_BuildPillar;
        Thinker_Add(&pillar->thinker);

        P_ToXSector(sec)->specialData = pillar;
        pillar->sector = sec;

        if(!args[2])
        {
            pillar->ceilingSpeed = pillar->floorSpeed =
                (float) args[1] * (1.0f / 8);
        }
        else if(newHeight - P_GetDoublep(sec, DMU_FLOOR_HEIGHT) >
                P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newHeight)
        {
            pillar->floorSpeed = (float) args[1] * (1.0f / 8);
            pillar->ceilingSpeed =
                (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newHeight) *
                      (pillar->floorSpeed / (newHeight - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0f / 8);
            pillar->floorSpeed =
                (newHeight - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed / (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newHeight));
        }

        pillar->floorDest = newHeight;
        pillar->ceilingDest = newHeight;
        pillar->direction = 1;
        pillar->crush = crush * (int) args[3];
        SN_StartSequence(P_GetPtrp(pillar->sector, DMU_BASE),
                         SEQ_PLATFORM + P_ToXSector(pillar->sector)->seqType);
    }
    return rtn;
}

int EV_OpenPillar(LineDef* line, byte* args)
{
    int rtn = 0;
    Sector* sec = NULL;
    pillar_t* pillar;
    iterlist_t* list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list) return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going...

        if(!FEQUAL(P_GetDoublep(sec, DMU_FLOOR_HEIGHT),
                   P_GetDoublep(sec, DMU_CEILING_HEIGHT)))
            continue; // Pillar isn't closed.

        rtn = 1;

        pillar = Z_Calloc(sizeof(*pillar), PU_MAP, 0);
        pillar->thinker.function = T_BuildPillar;
        Thinker_Add(&pillar->thinker);

        P_ToXSector(sec)->specialData = pillar;
        pillar->sector = sec;
        if(!args[2])
        {
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &pillar->floorDest);
        }
        else
        {
            pillar->floorDest =
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT) - (coord_t) args[2];
        }

        if(!args[3])
        {
            P_FindSectorSurroundingHighestCeiling(sec, 0, &pillar->ceilingDest);
        }
        else
        {
            pillar->ceilingDest =
                P_GetDoublep(sec, DMU_CEILING_HEIGHT) + (coord_t) args[3];
        }

        if(P_GetDoublep(sec, DMU_FLOOR_HEIGHT) - pillar->floorDest >=
           pillar->ceilingDest - P_GetDoublep(sec, DMU_CEILING_HEIGHT))
        {
            pillar->floorSpeed = (float) args[1] * (1.0f / 8);
            pillar->ceilingSpeed =
                (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - pillar->ceilingDest) *
                    (pillar->floorSpeed / (pillar->floorDest - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0f / 8);
            pillar->floorSpeed =
                (pillar->floorDest - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed / (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - pillar->ceilingDest));
        }

        pillar->direction = -1; // Open the pillar.
        SN_StartSequence(P_GetPtrp(pillar->sector, DMU_BASE),
                         SEQ_PLATFORM + P_ToXSector(pillar->sector)->seqType);
    }

    return rtn;
}
