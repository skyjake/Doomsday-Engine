/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * p_floor.c: Stairs and donuts.
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

typedef struct spreadsectorparams_s {
    sector_t           *baseSec;
    int                 material;
    sector_t           *foundSec;
} spreadsectorparams_t;

int findAdjacentSectorForSpread(void *ptr, void *context)
{
    linedef_t          *ln = (linedef_t*) ptr;
    spreadsectorparams_t *params = (spreadsectorparams_t*) context;
    sector_t           *frontSec, *backSec;

    frontSec = P_GetPtrp(ln, DMU_FRONT_SECTOR);
    if(!frontSec)
        return 1; // Continue iteration.

    if(params->baseSec != frontSec)
        return 1; // Continue iteration.

    backSec = P_GetPtrp(ln, DMU_BACK_SECTOR);
    if(!backSec)
        return 1; // Continue iteration.

    if(P_GetIntp(backSec, DMU_FLOOR_MATERIAL) != params->material)
        return 1; // Continue iteration.

    if(P_ToXSector(backSec)->specialData)
        return 1; // Continue iteration.

    // This looks good.
    params->foundSec = backSec;
    return 0; // Stop iteration.
}

int EV_BuildStairs(linedef_t *line, stair_e type)
{
    int                 rtn = 0;
    xsector_t          *xsec;
    sector_t           *sec = NULL;
    floormove_t        *floor;
    float               height = 0, stairsize = 0;
    float               speed = 0;
    iterlist_t         *list;
    spreadsectorparams_t params;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_ToXSector(sec);

        // Already moving? If so, keep going...
        if(xsec->specialData)
            continue;

        // New floor thinker.
        rtn = 1;
        floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
        P_AddThinker(&floor->thinker);
        xsec->specialData = floor;
        floor->thinker.function = T_MoveFloor;
        floor->direction = 1;
        floor->sector = sec;
        switch(type)
        {
        case build8:
            speed = FLOORSPEED * .25;
            stairsize = 8;
            break;
        case turbo16:
            speed = FLOORSPEED * 4;
            stairsize = 16;
            break;
        }
        floor->speed = speed;
        height = P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + stairsize;
        floor->floorDestHeight = height;

        // Find next sector to raise.
        // 1. Find 2-sided line with a front side in the same sector.
        // 2. Other side is the next sector to raise.
        params.baseSec = sec;
        params.material = P_GetIntp(sec, DMU_FLOOR_MATERIAL);
        params.foundSec = NULL;

        while(!P_Iteratep(params.baseSec, DMU_LINEDEF, &params,
                          findAdjacentSectorForSpread))
        {   // We found another sector to spread to.
            height += stairsize;

            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);

            P_AddThinker(&floor->thinker);

            P_ToXSector(params.foundSec)->specialData = floor;
            floor->thinker.function = T_MoveFloor;
            floor->direction = 1;
            floor->sector = params.foundSec;
            floor->speed = speed;
            floor->floorDestHeight = height;

            // Prepare for the next pass.
            params.baseSec = params.foundSec;
            params.foundSec = NULL;
        }
    }

    return rtn;
}

typedef struct findsectorfirstneighborparams_s {
    sector_t           *baseSec;
    sector_t           *foundSec;
} findsectorfirstneighborparams_t;

int findSectorFirstNeighbor(void *ptr, void *context)
{
    linedef_t          *li = (linedef_t*) ptr;
    findsectorfirstneighborparams_t *params =
        (findsectorfirstneighborparams_t*) context;
    sector_t           *frontSec, *backSec;

    frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR);
    backSec= P_GetPtrp(li, DMU_BACK_SECTOR);

    if(frontSec && backSec)
    {
        if(frontSec == params->baseSec && backSec != params->baseSec)
        {
            params->foundSec = backSec;
            return 0; // Stop iteration, this will do.
        }
        else if(backSec == params->baseSec && frontSec != params->baseSec)
        {
            params->foundSec = frontSec;
            return 0; // Stop iteration, this will do.
        }
    }

    return 1; // Continue iteration.
}

int EV_DoDonut(linedef_t *line)
{
    int                 rtn = 0;
    sector_t           *sec, *inner, *outer;
    iterlist_t         *list;
    findsectorfirstneighborparams_t params;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        // Already moving? If so, keep going...
        if(P_ToXSector(sec)->specialData)
            continue;

        rtn = 1;
        inner = outer = NULL;

        params.baseSec = sec;
        params.foundSec = NULL;
        if(!P_Iteratep(sec, DMU_LINEDEF, &params, findSectorFirstNeighbor))
        {
            outer = params.foundSec;

            params.baseSec = outer;
            params.foundSec = NULL;
            if(!P_Iteratep(outer, DMU_LINEDEF, &params, findSectorFirstNeighbor))
                inner = params.foundSec;
        }

        if(inner && outer)
        {   // Found both parts of the donut.
            floormove_t        *floor;
            float               destHeight =
                P_GetFloatp(inner, DMU_FLOOR_HEIGHT);

            // Spawn rising slime.
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_ToXSector(outer)->specialData = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = outer;
            floor->speed = FLOORSPEED * .5;
            floor->texture = P_GetIntp(inner, DMU_FLOOR_MATERIAL);
            floor->newSpecial = 0;
            floor->floorDestHeight = destHeight;

            // Spawn lowering donut-hole.
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_ToXSector(sec)->specialData = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = lowerFloor;
            floor->crush = false;
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * .5;
            floor->floorDestHeight = destHeight;
        }
    }

    return rtn;
}
