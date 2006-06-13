/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 *  Floor animation: raising stairs.
 *  2006/01/17 DJS - Recreated using jDoom's p_floor.c as a base.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Move a plane (floor or ceiling) and check for crushing
 */
result_e T_MovePlane(sector_t *sector, fixed_t speed, fixed_t dest,
                     boolean crush, int floorOrCeiling, int direction)
{
    boolean flag;
    fixed_t lastpos;
    fixed_t floorheight, ceilingheight;

    floorheight = P_GetFixedp(sector, DMU_FLOOR_HEIGHT);
    ceilingheight = P_GetFixedp(sector, DMU_CEILING_HEIGHT);

    switch(floorOrCeiling)
    {
    case 0:
        // FLOOR
        switch(direction)
        {
        case -1:
            // DOWN
            if(floorheight - speed < dest)
            {
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_FLOOR_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_SPEED, 0);
                    P_ChangeSector(sector, crush);
                }

                return pastdest;
            }
            else
            {
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_FLOOR_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_SPEED, 0);
                    P_ChangeSector(sector, crush);

                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(floorheight + speed > dest)
            {
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_FLOOR_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_SPEED, 0);
                    P_ChangeSector(sector, crush);
                }

                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos + speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    if(crush == true)
                        return crushed;

                    P_SetFixedp(sector, DMU_FLOOR_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_FLOOR_SPEED, 0);
                    P_ChangeSector(sector, crush);

                    return crushed;
                }
            }
            break;
        }
        break;

    case 1:
        // CEILING
        switch (direction)
        {
        case -1:
            // DOWN
            if(ceilingheight - speed < dest)
            {
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_CEILING_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_CEILING_SPEED, 0);
                    P_ChangeSector(sector, crush);
                }

                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    if(crush == true)
                        return crushed;

                    P_SetFixedp(sector, DMU_CEILING_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_CEILING_SPEED, 0);
                    P_ChangeSector(sector, crush);

                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(ceilingheight + speed > dest)
            {
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_CEILING_TARGET, lastpos);
                    P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFixedp(sector, DMU_CEILING_SPEED, 0);
                    P_ChangeSector(sector, crush);
                }

                return pastdest;
            }
            else
            {
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos + speed);
                flag = P_ChangeSector(sector, crush);
            }
            break;
        }
        break;

    }
    return ok;
}

/*
 * MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
 */
void T_MoveFloor(floormove_t * floor)
{
    xsector_t *xsec = P_XSector(floor->sector);
    result_e res =
        T_MovePlane(floor->sector, floor->speed, floor->floordestheight,
                    floor->crush, 0, floor->direction);

    if(!(leveltime & 7))
        S_SectorSound(floor->sector, SORG_FLOOR, sfx_dormov);

    if(res == pastdest)
    {
        P_SetIntp(floor->sector, DMU_FLOOR_SPEED, 0);

        xsec->specialdata = NULL;

        if(floor->direction == 1)
        {
            switch (floor->type)
            {
            case donutRaise:
                xsec->special = floor->newspecial;

                P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                          floor->texture);

            default:
                break;
            }
        }
        else if(floor->direction == -1)
        {
            switch (floor->type)
            {
            case lowerAndChange:
                xsec->special = floor->newspecial;

                P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                          floor->texture);

            default:
                break;
            }
        }
        P_RemoveThinker(&floor->thinker);

        if(floor->type == raiseBuildStep)
            S_SectorSound(floor->sector, SORG_FLOOR, sfx_pstop);
    }
}

/*
 * HANDLE FLOOR TYPES
 */
int EV_DoFloor(line_t *line, floor_e floortype)
{
    int     secnum;
    int     rtn;
    int     i;
    int     bottomtexture;
    xsector_t *xsec;
    sector_t *sec;
    sector_t *frontsector;
    line_t  *ln;
    floormove_t *floor;

    secnum = -1;
    rtn = 0;
    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);
        xsec = &xsectors[secnum];
        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(xsec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
        P_AddThinker(&floor->thinker);
        xsec->specialdata = floor;
        floor->thinker.function = T_MoveFloor;
        floor->type = floortype;
        floor->crush = false;

        switch (floortype)
        {
        case lowerFloor:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            break;

        case lowerFloorToLowest:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            break;

        case turboLower:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 4;
            floor->floordestheight =
                (8 * FRACUNIT) + P_FindHighestFloorSurrounding(sec);
            break;

        case raiseFloorCrush:
            floor->crush = true;
        case raiseFloor:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindLowestCeilingSurrounding(sec);

            if(floor->floordestheight >
               P_GetFixedp(sec, DMU_CEILING_HEIGHT))
                floor->floordestheight =
                    P_GetFixedp(sec, DMU_CEILING_HEIGHT);

            floor->floordestheight -=
                (8 * FRACUNIT) * (floortype == raiseFloorCrush);
            break;

        case raiseFloorToNearest:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight =
                P_FindNextHighestFloor(sec, P_GetFixedp(sec,
                                                        DMU_FLOOR_HEIGHT));
            break;

        case raiseFloor24:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight =
                P_GetFixedp(floor->sector,
                            DMU_FLOOR_HEIGHT) + 24 * FRACUNIT;
            break;

        case raiseFloor24AndChange:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight =
                P_GetFixedp(floor->sector,
                            DMU_FLOOR_HEIGHT) + 24 * FRACUNIT;

            frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            xsec->special = P_XSector(frontsector)->special;
            break;

        case raiseToTexture:
            {
                int     minsize = MAXINT;
                side_t *side;

                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
                {
                    ln = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

                    if(P_GetIntp(ln, DMU_FLAGS) & ML_TWOSIDED)
                    {
                        side = P_GetPtrp(ln, DMU_SIDE0);
                        bottomtexture = P_GetIntp(side, DMU_BOTTOM_TEXTURE);
                        if(bottomtexture >= 0)
                        {
                            Set(DD_TEXTURE_HEIGHT_QUERY, bottomtexture);
                            if(Get(DD_QUERY_RESULT) < minsize)
                                minsize = Get(DD_QUERY_RESULT);
                        }

                        side = P_GetPtrp(ln, DMU_SIDE1);
                        bottomtexture = P_GetIntp(side, DMU_BOTTOM_TEXTURE);
                        if(bottomtexture >= 0)
                        {
                            Set(DD_TEXTURE_HEIGHT_QUERY, bottomtexture);

                            if(Get(DD_QUERY_RESULT) < minsize)
                                minsize = Get(DD_QUERY_RESULT);
                        }
                    }
                }
                floor->floordestheight =
                    P_GetFixedp(floor->sector, DMU_FLOOR_HEIGHT)
                    + minsize;
                break;
            }

        case lowerAndChange:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            floor->texture = P_GetIntp(sec, DMU_FLOOR_TEXTURE);

            for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
            {
                // Choose the correct texture and special on two sided lines.
                ln = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

                if(P_GetIntp(ln, DMU_FLAGS) & ML_TWOSIDED)
                {
                    if(P_GetPtrp(ln, DMU_FRONT_SECTOR) == sec)
                    {
                        sec = P_GetPtrp(ln, DMU_BACK_SECTOR);
                        if(P_GetFixedp(sec, DMU_FLOOR_HEIGHT) ==
                                                      floor->floordestheight)
                        {
                            floor->texture =
                                P_GetIntp(sec,DMU_FLOOR_TEXTURE);
                            floor->newspecial =
                                xsectors[P_ToIndex(sec)].special;
                            break;
                        }
                    }
                    else
                    {
                        sec = P_GetPtrp(ln, DMU_FRONT_SECTOR);
                        if(P_GetFixedp(sec, DMU_FLOOR_HEIGHT) ==
                                                      floor->floordestheight)
                        {
                            floor->texture =
                                P_GetIntp(sec, DMU_FLOOR_TEXTURE);
                            floor->newspecial =
                                xsectors[P_ToIndex(sec)].special;
                            break;
                        }
                    }
                }
            }
        default:
            break;
        }
    }
    return rtn;
}

int EV_BuildStairs(line_t *line, stair_e type)
{
    int     secnum;
    int     height;
    int     i;
    int     newsecnum;
    int     texture;
    int     ok;
    int     rtn;
    line_t  *ln;
    xsector_t *xsec;
    sector_t *sec;
    sector_t *tsec;

    floormove_t *floor;

    fixed_t stairsize = 0;

    secnum = -1;
    rtn = 0;
    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        // ALREADY MOVING?  IF SO, KEEP GOING...
        sec = P_ToPtr(DMU_SECTOR, secnum);
        xsec = &xsectors[secnum];

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
        switch (type)
        {
        case build8:
            stairsize = 8 * FRACUNIT;
            break;
        case build16:
            stairsize = 16 * FRACUNIT;
            break;
        }
        floor->type = raiseBuildStep;

        floor->speed = FLOORSPEED;
        height = P_GetFixedp(sec, DMU_FLOOR_HEIGHT) + stairsize;
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

                newsecnum = P_ToIndex(tsec);
                if(secnum != newsecnum)
                    continue;

                tsec = P_GetPtrp(ln, DMU_BACK_SECTOR);

                newsecnum = P_ToIndex(tsec);
                if(P_GetIntp(tsec, DMU_FLOOR_TEXTURE) != texture)
                    continue;

                height += stairsize;

                if(xsectors[newsecnum].specialdata)
                    continue;

                sec = tsec;
                secnum = newsecnum;
                floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);

                P_AddThinker(&floor->thinker);

                xsectors[newsecnum].specialdata = floor;
                floor->thinker.function = T_MoveFloor;
                floor->type = raiseBuildStep;
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = height;
                ok = 1;
                break;
            }
        } while(ok);
    }
    return rtn;
}
