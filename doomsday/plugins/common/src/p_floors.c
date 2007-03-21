/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 *  p_floors.c: Moving floors.
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

/**
 * Move a plane (floor or ceiling) and check for crushing.
 */
result_e T_MovePlane(sector_t *sector, float speed, float dest,
                     boolean crush, int isCeiling, int direction)
{
    boolean     flag;
    float       lastpos;
    float       floorheight, ceilingheight;
    int         ptarget = (isCeiling? DMU_CEILING_TARGET : DMU_FLOOR_TARGET);
    int         pspeed = (isCeiling? DMU_CEILING_SPEED : DMU_FLOOR_SPEED);

    // Let the engine know about the movement of this plane.
    P_SetFloatp(sector, ptarget, dest);
    P_SetFloatp(sector, pspeed, speed);

    floorheight = P_GetFloatp(sector, DMU_FLOOR_HEIGHT);
    ceilingheight = P_GetFloatp(sector, DMU_CEILING_HEIGHT);

    switch(isCeiling)
    {
    case 0:
        // FLOOR
        switch(direction)
        {
        case -1:
            // DOWN
            if(floorheight - speed < dest)
            {
                // The move is complete.
                lastpos = floorheight;
                P_SetFloatp(sector, DMU_FLOOR_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    // Oh no, the move failed.
                    P_SetFloatp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                lastpos = floorheight;
                P_SetFloatp(sector, DMU_FLOOR_HEIGHT, floorheight - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFloatp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
#if __JHEXEN__
                    P_SetFloatp(sector, pspeed, 0);
#endif
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(floorheight + speed > dest)
            {
                // The move is complete.
                lastpos = floorheight;
                P_SetFloatp(sector, DMU_FLOOR_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    // Oh no, the move failed.
                    P_SetFloatp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = floorheight;
                P_SetFloatp(sector, DMU_FLOOR_HEIGHT, floorheight + speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
#if !__JHEXEN__
                    if(crush == true)
                        return crushed;
#endif
                    P_SetFloatp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
#if __JHEXEN__
                    P_SetFloatp(sector, pspeed, 0);
#endif
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;
        }
        break;

    case 1:
        // CEILING
        switch(direction)
        {
        case -1:
            // DOWN
            if(ceilingheight - speed < dest)
            {
                // The move is complete.
                lastpos = ceilingheight;
                P_SetFloatp(sector, DMU_CEILING_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFloatp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = ceilingheight;
                P_SetFloatp(sector, DMU_CEILING_HEIGHT, ceilingheight - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
#if !__JHEXEN__
                    if(crush == true)
                        return crushed;
#endif
                    P_SetFloatp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
#if __JHEXEN__
                    P_SetFloatp(sector, pspeed, 0);
#endif
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(ceilingheight + speed > dest)
            {
                // The move is complete.
                lastpos = ceilingheight;
                P_SetFloatp(sector, DMU_CEILING_HEIGHT, dest);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFloatp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFloatp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
#if __JHEXEN__
                P_SetFloatp(sector, pspeed, 0);
#endif
                return pastdest;
            }
            else
            {
                lastpos = ceilingheight;
                P_SetFloatp(sector, DMU_CEILING_HEIGHT, ceilingheight + speed);
                flag = P_ChangeSector(sector, crush);
            }
            break;
        }
        break;

    }
    return ok;
}

/**
 * Move a floor to it's destination (up or down).
 */
void T_MoveFloor(floormove_t *floor)
{
    xsector_t *xsec = P_XSector(floor->sector);
    result_e res;
    
#if __JHEXEN__
    if(floor->resetDelayCount)
    {
        floor->resetDelayCount--;
        if(!floor->resetDelayCount)
        {
            floor->floordestheight = floor->resetHeight;
            floor->direction = -floor->direction;
            floor->resetDelay = 0;
            floor->delayCount = 0;
            floor->delayTotal = 0;
        }
    }
    if(floor->delayCount)
    {
        floor->delayCount--;
        if(!floor->delayCount && floor->textureChange)
        {
            P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                      P_GetIntp(floor->sector, DMU_FLOOR_TEXTURE) +
                      floor->textureChange);
        }
        return;
    }
#endif

    res =
        T_MovePlane(floor->sector, floor->speed, floor->floordestheight,
                    floor->crush, 0, floor->direction);

#if __JHEXEN__
    if(floor->type == FLEV_RAISEBUILDSTEP)
    {
        if((floor->direction == 1 &&
            P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) >=
                floor->stairsDelayHeight) ||
           (floor->direction == -1 &&
            P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) <=
                floor->stairsDelayHeight))
        {
            floor->delayCount = floor->delayTotal;
            floor->stairsDelayHeight += floor->stairsDelayHeightDelta;
        }
    }
#endif

#if !__JHEXEN__
    if(!(leveltime & 7))
# if __WOLFTC__
        S_SectorSound(floor->sector, SORG_FLOOR, sfx_pltstr);
# elif __JHERETIC__
        S_SectorSound(floor->sector, SORG_FLOOR, sfx_dormov);
# else
        S_SectorSound(floor->sector, SORG_FLOOR, sfx_stnmov);
# endif
#endif

    if(res == pastdest)
    {
        P_SetFloatp(floor->sector, DMU_FLOOR_SPEED, 0);

#if __JHEXEN__
        SN_StopSequence(P_GetPtrp(floor->sector, DMU_SOUND_ORIGIN));
#else
# if !__DOOM64TC__
#  if __WOLFTC__
        S_SectorSound(floor->sector, SORG_FLOOR, sfx_pltstp);
#  else
#   if __JHERETIC__
        if(floor->type == raiseBuildStep)
#   endif
            S_SectorSound(floor->sector, SORG_FLOOR, sfx_pstop);
#  endif
# endif
#endif
#if __JHEXEN__
        if(floor->delayTotal)
            floor->delayTotal = 0;

        if(floor->resetDelay)
        {
            //          floor->resetDelayCount = floor->resetDelay;
            //          floor->resetDelay = 0;
            return;
        }
#endif
        xsec->specialdata = NULL;
#if __JHEXEN__
        if(floor->textureChange)
        {
            P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                      P_GetIntp(floor->sector, DMU_FLOOR_TEXTURE) -
                      floor->textureChange);
        }
#else
        if(floor->direction == 1)
        {
            switch(floor->type)
            {
            case donutRaise:
                xsec->special = floor->newspecial;

                P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                          floor->texture);
                break;

            default:
                break;
            }
        }
        else if(floor->direction == -1)
        {
            switch(floor->type)
            {
            case lowerAndChange:
                xsec->special = floor->newspecial;

                P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                          floor->texture);
                break;

            default:
                break;
            }
        }
#endif
#if __JHEXEN__
        P_TagFinished(P_XSector(floor->sector)->tag);
#endif
        P_RemoveThinker(&floor->thinker);
    }
}

/**
 * Handle moving floors.
 */
#if __JHEXEN__
int EV_DoFloor(line_t *line, byte *args, floor_e floortype)
#else
int EV_DoFloor(line_t *line, floor_e floortype)
#endif
{
#if !__JHEXEN__
    int         i;
    int         bottomtexture;
    line_t     *ln;
    sector_t   *frontsector;
#endif
    int         rtn = 0;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    floormove_t *floor;
    iterlist_t *list;

#if __DOOM64TC__
    // d64tc > bitmip? wha?
    float bitmipL = 0, bitmipR = 0;
    side_t *front = P_GetPtrp(line, DMU_SIDE0);
    side_t *back  = P_GetPtrp(line, DMU_SIDE1);

    bitmipL = P_GetFloatp(front, DMU_MIDDLE_TEXTURE_OFFSET_X);
    if(back)
        bitmipR = P_GetFloatp(back, DMU_MIDDLE_TEXTURE_OFFSET_X);
    // < d64tc
#endif

#if __JHEXEN__
    list = P_GetSectorIterListForTag((int) args[0], false);
#else
    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
#endif
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_XSector(sec);
        // If already moving, keep going...
        if(xsec->specialdata)
            continue;

        // New floor thinker.
        rtn = 1;
        floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
#if __JHEXEN__
        memset(floor, 0, sizeof(*floor));
#endif
        P_AddThinker(&floor->thinker);
        xsec->specialdata = floor;
        floor->thinker.function = T_MoveFloor;
        floor->type = floortype;
        floor->crush = false;
#if __JHEXEN__
        floor->speed = (float) args[1] * (1.0 / 8);
        if(floortype == FLEV_LOWERTIMES8INSTANT ||
           floortype == FLEV_RAISETIMES8INSTANT)
        {
            floor->speed = 2000;
        }
#endif
        switch(floortype)
        {
#if __JHEXEN__
        case FLEV_LOWERFLOOR:
#else
        case lowerFloor:
#endif
            floor->direction = -1;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 4;
# endif
#endif
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            break;
#if __JHEXEN__
        case FLEV_LOWERFLOORTOLOWEST:
#else
        case lowerFloorToLowest:
#endif
            floor->direction = -1;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 4;
# endif
#endif
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            break;
#if __JHEXEN__
        case FLEV_LOWERFLOORBYVALUE:
            floor->direction = -1;
            floor->sector = sec;
            floor->floordestheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) - (float) args[2];
            break;

        case FLEV_LOWERTIMES8INSTANT:
        case FLEV_LOWERBYVALUETIMES8:
            floor->direction = -1;
            floor->sector = sec;
            floor->floordestheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) - (float) args[2] * 8;
            break;
#endif
#if !__JHEXEN__
        case turboLower:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 4;
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
# if __JHERETIC__
            floor->floordestheight += 8;
# else
            if(floor->floordestheight != P_GetFloatp(sec,
                                                     DMU_FLOOR_HEIGHT))
                floor->floordestheight += 8;
# endif
            break;
#endif
#if __DOOM64TC__
        case lowerToEight: // d64tc
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            if(floor->floordestheight != P_GetFloatp(sec,
                                                     DMU_FLOOR_HEIGHT))
                floor->floordestheight += 8;
            break;

        case customFloor: // d64tc
            if(bitmipR > 0)
            {
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * bitmipL;
                floor->floordestheight = P_FindHighestFloorSurrounding(sec);

                if(floor->floordestheight != P_GetFloatp(sec,
                                                         DMU_FLOOR_HEIGHT))
                    floor->floordestheight += bitmipR;
            }
            else
            {
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * bitmipL;
                floor->floordestheight =
                    P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) - bitmipR;
            }
            break;

        case customChangeSec: // d64tc
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 16;
            floor->floordestheight = P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT);

            // KLUDGE: fake the engine into accepting this special
            P_XSector(sec)->special = bitmipR;
            // < KLUDGE
            break;
#endif
#if __JHEXEN__
        case FLEV_RAISEFLOORCRUSH:
#else
        case raiseFloorCrush:
#endif
#if __JHEXEN__
            floor->crush = (int) args[2]; // arg[2] = crushing value
#else
            floor->crush = true;
#endif
            floor->direction = 1;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 4;
# endif
#endif
#if __JHEXEN__
            floor->floordestheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT)-8;
#else
            floor->floordestheight = P_FindLowestCeilingSurrounding(sec);

            if(floor->floordestheight > P_GetFloatp(sec, DMU_CEILING_HEIGHT))
                floor->floordestheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);

            floor->floordestheight -= 8 * (floortype == raiseFloorCrush);
#endif
            break;

#if __JHEXEN__
        case FLEV_RAISEFLOOR:
#else
        case raiseFloor:
#endif
            floor->direction = 1;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 4;
# endif
#endif
            floor->floordestheight = P_FindLowestCeilingSurrounding(sec);

            if(floor->floordestheight > P_GetFloatp(sec, DMU_CEILING_HEIGHT))
                floor->floordestheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);

#if !__JHEXEN__
            floor->floordestheight -= 8 * (floortype == raiseFloorCrush);
#endif
            break;
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        case raiseFloorTurbo:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 4;
# if __DOOM64TC__
            floor->speed *= 2;
# endif
            floor->floordestheight =
                P_FindNextHighestFloor(sec, P_GetFloatp(sec,
                                                        DMU_FLOOR_HEIGHT));
            break;
#endif
#if __JHEXEN__
        case FLEV_RAISEFLOORTONEAREST:
#else
        case raiseFloorToNearest:
#endif
            floor->direction = 1;
            floor->sector = sec;
#if !__JHEXEN__
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 8;
# endif
#endif
            floor->floordestheight =
                P_FindNextHighestFloor(sec, P_GetFloatp(sec,
                                                        DMU_FLOOR_HEIGHT));
            break;
#if __JHEXEN__
        case FLEV_RAISEFLOORBYVALUE:
            floor->direction = 1;
            floor->sector = sec;
            floor->floordestheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + (float) args[2];
            break;

        case FLEV_RAISETIMES8INSTANT:
        case FLEV_RAISEBYVALUETIMES8:
            floor->direction = 1;
            floor->sector = sec;
            floor->floordestheight =
                P_GetFloatp(sec, DMU_FLOOR_HEIGHT) + (float) args[2] * 8;
            break;

        case FLEV_MOVETOVALUETIMES8:
            floor->sector = sec;
            floor->floordestheight = (float) args[2] * 8;
            if(args[3])
                floor->floordestheight = -floor->floordestheight;

            if(floor->floordestheight > P_GetFloatp(sec, DMU_FLOOR_HEIGHT))
                floor->direction = 1;
            else if(floor->floordestheight < P_GetFloatp(sec, DMU_FLOOR_HEIGHT))
                floor->direction = -1;
            else 
                rtn = 0; // Already at lowest position.

            break;
#endif
#if !__JHEXEN__
        case raiseFloor24:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 8;
# endif
            floor->floordestheight =
                P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) + 24;
            break;
#endif
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        case raiseFloor512:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight =
                P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) + 512;
            break;
#endif
#if !__JHEXEN__
        case raiseFloor24AndChange:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
# if __DOOM64TC__
            floor->speed *= 8;
# endif
            floor->floordestheight =
                P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) + 24;

            frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            xsec->special = P_XSector(frontsector)->special;
            break;
# if __DOOM64TC__
        case raiseFloor32: // d64tc
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 8;
            floor->floordestheight =
                P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT) + 32;
            break;
# endif
        case raiseToTexture:
            {
            int     minsize = MAXINT;
            side_t *side;

            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); ++i)
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
                P_GetFloatp(floor->sector, DMU_FLOOR_HEIGHT)
                + FIX2FLT(minsize);
            }
            break;

        case lowerAndChange:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            floor->texture = P_GetIntp(sec, DMU_FLOOR_TEXTURE);

            for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); ++i)
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
                                P_XSector(sec)->special;
                            break;
                        }
                    }
                    else
                    {
                        sec = P_GetPtrp(ln, DMU_FRONT_SECTOR);
                        if(P_GetFloatp(sec, DMU_FLOOR_HEIGHT) ==
                                                      floor->floordestheight)
                        {
                            floor->texture =
                                P_GetIntp(sec, DMU_FLOOR_TEXTURE);
                            floor->newspecial =
                                P_XSector(sec)->special;
                            break;
                        }
                    }
                }
            }
            break;
#endif
        default:
#if __JHEXEN__
            rtn = 0;
#endif
            break;
        }
    }

#if __JHEXEN__
    if(rtn)
    {
        SN_StartSequence(P_GetPtrp(floor->sector, DMU_SOUND_ORIGIN),
                         SEQ_PLATFORM + P_XSector(floor->sector)->seqType);
    }
#endif
    return rtn;
}

#if __JHEXEN__
int EV_FloorCrushStop(line_t *line, byte *args)
{
    thinker_t  *think;
    floormove_t *floor;
    boolean     rtn;

    rtn = 0;
    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != T_MoveFloor)
            continue;

        floor = (floormove_t *) think;
        if(floor->type != FLEV_RAISEFLOORCRUSH)
            continue;

        // Completely remove the crushing floor
        SN_StopSequence(P_GetPtrp(floor->sector, DMU_SOUND_ORIGIN));
        P_XSector(floor->sector)->specialdata = NULL;
        P_TagFinished(P_XSector(floor->sector)->tag);
        P_RemoveThinker(&floor->thinker);
        rtn = 1;
    }
    return rtn;
}
#endif

#if __JHEXEN__
int EV_DoFloorAndCeiling(line_t *line, byte *args, boolean raise)
{
    boolean     floor, ceiling;
    sector_t   *sec = NULL;
    iterlist_t *list;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);

    // Original Hexen KLUDGE: 
    // Due to the fact that sectors can only have one special thinker
    // linked at a time, this routine manually removes the link before
    // then creating a second thinker for the sector.
    // In order to commonize this we should maintain seperate links in
    // xsector_t for each type of special (not thinker type) i.e:
    //
    //   floor, ceiling, lightlevel
    //
    // Note: floor and ceiling are capable of moving at different speeds
    // and with different target heights, we must remain compatible.
    if(raise)
    {
        floor = EV_DoFloor(line, args, FLEV_RAISEFLOORBYVALUE);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            P_XSector(sec)->specialdata = NULL;
        }
        ceiling = EV_DoCeiling(line, args, raiseByValue);
    }
    else
    {
        floor = EV_DoFloor(line, args, FLEV_LOWERFLOORBYVALUE);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            P_XSector(sec)->specialdata = NULL;
        }
        ceiling = EV_DoCeiling(line, args, lowerByValue);
    }
    // < KLUDGE
    return (floor | ceiling);
}
#endif
