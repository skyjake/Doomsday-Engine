/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 *  Stairs and donuts.
 */

// HEADER FILES ------------------------------------------------------------

#include "doom64tc.h"

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

int EV_BuildStairs(line_t *line, stair_e type)
{
    int         i, ok, texture;
    int         rtn = 0;
    float       height = 0, stairsize = 0;
    float       speed = 0;
    line_t     *ln;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    sector_t   *tsec;
    floormove_t *floor;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_XSector(sec);
        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(xsec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
        P_AddThinker(&floor->thinker);
        xsec->specialdata = floor;
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
        floor->floordestheight = height;

        texture = P_GetIntp(sec, DMU_FLOOR_TEXTURE);

        // Find next sector to raise
        // 1.   Find 2-sided line with same sector side[0]
        // 2.   Other side is the next sector to raise
        do
        {
            ok = 0;
            for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
            {
                ln = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

                if(!(P_GetIntp(ln, DMU_FLAGS) & ML_TWOSIDED))
                    continue;

                tsec = P_GetPtrp(ln, DMU_FRONT_SECTOR);
                if(sec != tsec)
                    continue;

                tsec = P_GetPtrp(ln, DMU_BACK_SECTOR);
                if(P_GetIntp(tsec, DMU_FLOOR_TEXTURE) != texture)
                    continue;

                height += stairsize;

                if(P_XSector(tsec)->specialdata)
                    continue;

                sec = tsec;
                floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);

                P_AddThinker(&floor->thinker);

                P_XSector(tsec)->specialdata = floor;
                floor->thinker.function = T_MoveFloor;
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = speed;
                floor->floordestheight = height;
                ok = 1;
                break;
            }
        } while(ok);
    }
    return rtn;
}

int EV_DoDonut(line_t *line)
{
    int         i, rtn = 0;
    sector_t   *s1 = NULL;
    sector_t   *s2;
    sector_t   *s3;
    line_t     *check;
    floormove_t *floor;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((s1 = P_IterListIterator(list)) != NULL)
    {
        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(P_XSector(s1)->specialdata)
            continue;

        rtn = 1;

        s2 = P_GetNextSector(P_GetPtrp(s1, DMU_LINE_OF_SECTOR | 0), s1);
        for(i = 0; i < P_GetIntp(s2, DMU_LINE_COUNT); i++)
        {
            check = P_GetPtrp(s2, DMU_LINE_OF_SECTOR | i);

            s3 = P_GetPtrp(check, DMU_BACK_SECTOR);

            if((!(P_GetIntp(check, DMU_FLAGS) & ML_TWOSIDED)) ||
               s3 == s1)
                continue;

            //  Spawn rising slime
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_XSector(s2)->specialdata = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED * .5;
            floor->texture = P_GetIntp(s3, DMU_FLOOR_TEXTURE);
            floor->newspecial = 0;
            floor->floordestheight = P_GetFloatp(s3, DMU_FLOOR_HEIGHT);

            //  Spawn lowering donut-hole
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_XSector(s1)->specialdata = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = lowerFloor;
            floor->crush = false;
            floor->direction = -1;
            floor->sector = s1;
            floor->speed = FLOORSPEED * .5;
            floor->floordestheight = P_GetFloatp(s3, DMU_FLOOR_HEIGHT);
            break;
        }
    }
    return rtn;
}
