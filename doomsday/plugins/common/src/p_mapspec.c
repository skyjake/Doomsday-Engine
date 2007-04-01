/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
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
 * p_mapspec.c:
 *
 * Line Tag handling. Line and Sector groups. Specialized iteration
 * routines, respective utility functions...
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
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct taglist_s {
    int         tag;
    iterlist_t *list;
} taglist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

iterlist_t  *spechit; // for crossed line specials.
iterlist_t  *linespecials; // for surfaces that tick eg wall scrollers.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static taglist_t *lineTagLists = NULL;
static int numLineTagLists = 0;

static taglist_t *sectorTagLists = NULL;
static int numSectorTagLists = 0;

// CODE --------------------------------------------------------------------

/**
 *
 */
void P_DestroyLineTagLists(void)
{
    int     i;

    if(numLineTagLists == 0)
        return;

    for(i = 0; i < numLineTagLists; ++i)
    {
        P_EmptyIterList(lineTagLists[i].list);
        P_DestroyIterList(lineTagLists[i].list);
    }

    free(lineTagLists);
    lineTagLists = NULL;
    numLineTagLists = 0;
}

/**
 *
 */
iterlist_t *P_GetLineIterListForTag(int tag, boolean createNewList)
{
    int         i;
    taglist_t *tagList;

    // Do we have an existing list for this tag?
    for(i = 0; i < numLineTagLists; ++i)
        if(lineTagLists[i].tag == tag)
            return lineTagLists[i].list;

    if(!createNewList)
        return NULL;

    // Nope, we need to allocate another.
    numLineTagLists++;
    lineTagLists = realloc(lineTagLists, sizeof(taglist_t) * numLineTagLists);
    tagList = &lineTagLists[numLineTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = P_CreateIterList());
}

/**
 *
 */
void P_DestroySectorTagLists(void)
{
    int     i;

    if(numSectorTagLists == 0)
        return;

    for(i = 0; i < numSectorTagLists; ++i)
    {
        P_EmptyIterList(sectorTagLists[i].list);
        P_DestroyIterList(sectorTagLists[i].list);
    }

    free(sectorTagLists);
    sectorTagLists = NULL;
    numSectorTagLists = 0;
}

/**
 *
 */
iterlist_t *P_GetSectorIterListForTag(int tag, boolean createNewList)
{
    int         i;
    taglist_t *tagList;

    // Do we have an existing list for this tag?
    for(i = 0; i < numSectorTagLists; ++i)
        if(sectorTagLists[i].tag == tag)
            return sectorTagLists[i].list;

    if(!createNewList)
        return NULL;

    // Nope, we need to allocate another.
    numSectorTagLists++;
    sectorTagLists = realloc(sectorTagLists, sizeof(taglist_t) * numSectorTagLists);
    tagList = &sectorTagLists[numSectorTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = P_CreateIterList());
}

/**
 * Return sector_t * of sector next to current.
 * NULL if not two-sided line
 */
sector_t *P_GetNextSector(line_t *line, sector_t *sec)
{
    if(!(P_GetIntp(line, DMU_FLAGS) & ML_TWOSIDED))
        return NULL;

    if(P_GetPtrp(line, DMU_FRONT_SECTOR) == sec)
        return P_GetPtrp(line, DMU_BACK_SECTOR);

    return P_GetPtrp(line, DMU_FRONT_SECTOR);
}

/**
 * Find lowest floor height in surrounding sectors.
 */
float P_FindLowestFloorSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    float       floor;
    line_t     *check;
    sector_t   *other;

    floor = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFloatp(other, DMU_FLOOR_HEIGHT) < floor)
            floor = P_GetFloatp(other, DMU_FLOOR_HEIGHT);
    }

    return floor;
}

/**
 * Find highest floor height in surrounding sectors.
 */
float P_FindHighestFloorSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    float       floor = -500;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFloatp(other, DMU_FLOOR_HEIGHT) > floor)
            floor = P_GetFloatp(other, DMU_FLOOR_HEIGHT);
    }

    return floor;
}

/**
 * Passed a sector and a floor height, returns the fixed point value
 * of the smallest floor height in a surrounding sector larger than
 * the floor height passed. If no such height exists the floorheight
 * passed is returned.
 *
 * DJS - Rewritten using Lee Killough's algorithm for avoiding the
 *       the fixed array.
 */
float P_FindNextHighestFloor(sector_t *sec, float currentheight)
{
    int         i;
    int         lineCount;
    float       otherHeight;
    float       anotherHeight;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        otherHeight = P_GetFloatp(other, DMU_FLOOR_HEIGHT);

        if(otherHeight > currentheight)
        {
            while(++i < lineCount)
            {
                check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
                other = P_GetNextSector(check, sec);

                if(other)
                {
                    anotherHeight = P_GetFloatp(other, DMU_FLOOR_HEIGHT);

                    if(anotherHeight < otherHeight &&
                       anotherHeight > currentheight)
                        otherHeight = anotherHeight;
                }
            }

            return otherHeight;
        }
    }

    return currentheight;
}

/**
 * Find lowest ceiling in the surrounding sector.
 */
float P_FindLowestCeilingSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    float       height = FIX2FLT(DDMAXINT);
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFloatp(other, DMU_CEILING_HEIGHT) < height)
            height = P_GetFloatp(other, DMU_CEILING_HEIGHT);
    }
    return height;
}

/**
 * Find highest ceiling in the surrounding sectors.
 */
float P_FindHighestCeilingSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    float       height = 0;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFloatp(other, DMU_CEILING_HEIGHT) > height)
            height = P_GetFloatp(other, DMU_CEILING_HEIGHT);
    }
    return height;
}

/**
 * Find minimum light from an adjacent sector
 */
int P_FindMinSurroundingLight(sector_t *sec, int max)
{
    int         i;
    float       min;
    int         lineCount;
    line_t     *line;
    sector_t   *check;

    min = (float) max / 255.0f;
    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        line = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        check = P_GetNextSector(line, sec);

        if(!check)
            continue;

        if(P_GetFloatp(check, DMU_LIGHT_LEVEL) < min)
            min = P_GetFloatp(check, DMU_LIGHT_LEVEL);
    }

    return 255.0f * min;
}
