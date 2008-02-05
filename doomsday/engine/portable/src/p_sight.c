/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * p_sight.c: Line of Sight Testing.
 *
 * This uses specialized forms of the maputils routines for optimized
 * performance.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float sightStartZ;            // eye z of looker
float topSlope, bottomSlope;  // slopes to top and bottom of target
int sightcounts[3];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static divline_t strace;

// CODE --------------------------------------------------------------------

boolean PTR_SightTraverse(intercept_t *in)
{
    linedef_t     *li;
    float       slope;

    if(in == NULL)
        return true; // Something was out of bounds?

    li = in->d.lineDef;

    // Crosses a two sided line.
    P_LineOpening(li);

    // Check for totally closed doors.
    if(openbottom >= opentop)
        return false; // Stop iteration.

    if(li->L_frontsector->SP_floorheight != li->L_backsector->SP_floorheight)
    {
        slope = (openbottom - sightStartZ) / in->frac;
        if(slope > bottomSlope)
            bottomSlope = slope;
    }

    if(li->L_frontsector->SP_ceilheight != li->L_backsector->SP_ceilheight)
    {
        slope = (opentop - sightStartZ) / in->frac;
        if(slope < topSlope)
            topSlope = slope;
    }

    if(topSlope <= bottomSlope)
        return false; // Stop iteration.

    return true; // Continue iteration.
}

boolean PIT_CheckSightLine(linedef_t *line, void *data)
{
    int         s[2];
    divline_t   dl;

    s[0] = P_PointOnDivlineSide(line->L_v1pos[VX],
                                line->L_v1pos[VY], &strace);
    s[1] = P_PointOnDivlineSide(line->L_v2pos[VX],
                                line->L_v2pos[VY], &strace);
    if(s[0] == s[1])
        return true; // Line isn't crossed, continue iteration.

    P_MakeDivline(line, &dl);
    s[0] = P_PointOnDivlineSide(FIX2FLT(strace.pos[VX]), FIX2FLT(strace.pos[VY]), &dl);
    s[1] = P_PointOnDivlineSide(FIX2FLT(strace.pos[VX] + strace.dX),
                                FIX2FLT(strace.pos[VY] + strace.dY),
                                &dl);
    if(s[0] == s[1])
        return true; // Line isn't crossed, continue iteration.

    // Try to early out the check.
    if(!line->L_backside)
        return false; // Stop iteration.

    // Store the line for later intersection testing.
    P_AddIntercept(0, true, line);

    return true; // Continue iteration.
}

/**
 * Traces a line from x1,y1 to x2,y2, calling the traverser function for
 * each.
 *
 * @param x1            Origin X (world) coordinate.
 * @param y1            Origin Y (world) coordinate.
 * @param x2            Destination X (world) coordinate.
 * @param y2            Destination Y (world) coordinate.
 *
 * @return              @c true if the traverser function returns @c true for
 *                      all visited lines.
 */
boolean P_SightPathTraverse(float x1, float y1, float x2, float y2)
{
    float       origin[2], dest[2];
    uint        originBlock[2], destBlock[2];
    float       delta[2];
    float       partial;
    fixed_t     intercept[2], step[2];
    uint        block[2];
    int         stepDir[2];
    int         count;
    gamemap_t  *map = P_GetCurrentMap();
    vec2_t      bmapOrigin;

    P_GetBlockmapBounds(map->blockMap, bmapOrigin, NULL);

    validCount++;
    P_ClearIntercepts();

    origin[VX] = x1;
    origin[VY] = y1;
    dest[VX] = x2;
    dest[VY] = y2;

    // Don't side exactly on blockmap lines.
    if((FLT2FIX(origin[VX] - bmapOrigin[VX]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VX] += 1;
    if((FLT2FIX(origin[VY] - bmapOrigin[VY]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VY] += 1;

    strace.pos[VX] = FLT2FIX(origin[VX]);
    strace.pos[VY] = FLT2FIX(origin[VY]);
    strace.dX = FLT2FIX(dest[VX] - origin[VX]);
    strace.dY = FLT2FIX(dest[VY] - origin[VY]);

    // Points should never be out of bounds but check anyway.
    if(!(P_ToBlockmapBlockIdx(map->blockMap, originBlock, origin) &&
         P_ToBlockmapBlockIdx(map->blockMap, destBlock, dest)))
        return false;

    origin[VX] -= bmapOrigin[VX];
    origin[VY] -= bmapOrigin[VY];
    dest[VX] -= bmapOrigin[VX];
    dest[VY] -= bmapOrigin[VY];

    if(destBlock[VX] > originBlock[VX])
    {
        stepDir[VX] = 1;
        partial = 1 - FIX2FLT((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else if(destBlock[VX] < originBlock[VX])
    {
        stepDir[VX] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else
    {
        stepDir[VX] = 0;
        partial = 1;
        delta[VY] = 256;
    }
    intercept[VY] = (FLT2FIX(origin[VY]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VY]);

    if(destBlock[VY] > originBlock[VY])
    {
        stepDir[VY] = 1;
        partial = 1 - FIX2FLT((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else if(destBlock[VY] < originBlock[VY])
    {
        stepDir[VY] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else
    {
        stepDir[VY] = 0;
        partial = 1;
        delta[VX] = 256;
    }
    intercept[VX] = (FLT2FIX(origin[VX]) >> MAPBTOFRAC) +
            FLT2FIX(partial * delta[VX]);

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[VX] = originBlock[VX];
    block[VY] = originBlock[VY];
    step[VX] = FLT2FIX(delta[VX]);
    step[VY] = FLT2FIX(delta[VY]);
    for(count = 0; count < 64; count++)
    {
        if(numPolyObjs > 0)
        {
            if(!P_BlockmapPolyobjLinesIterator(BlockMap, block,
                                               PIT_CheckSightLine, 0))
            {
                sightcounts[1]++;
                return false; // Early out.
            }
        }

        if(!P_BlockmapLinesIterator(BlockMap, block, PIT_CheckSightLine, 0))
        {
            sightcounts[1]++;
            return false; // Early out.
        }

        // At or past the target?
        if((block[VX] == destBlock[VX] && block[VY] == destBlock[VY]) ||
           (((dest[VX] >= origin[VX] && block[VX] >= destBlock[VX]) ||
             (dest[VX] <  origin[VX] && block[VX] <= destBlock[VX])) &&
            ((dest[VY] >= origin[VY] && block[VY] >= destBlock[VY]) ||
             (dest[VY] <  origin[VY] && block[VY] <= destBlock[VY]))))
            break;

        if((unsigned) (intercept[VY] >> FRACBITS) == block[VY])
        {
            intercept[VY] += step[VY];
            block[VX] += stepDir[VX];
        }
        else if((unsigned) (intercept[VX] >> FRACBITS) == block[VX])
        {
            intercept[VX] += step[VX];
            block[VY] += stepDir[VY];
        }
    }

    // Couldn't early out, so go through the sorted list
    sightcounts[2]++;

    return P_SightTraverseIntercepts(&strace, PTR_SightTraverse);
}

/**
 * Checks the reject matrix to find out if the two sectors are visible
 * from each other.
 */
boolean P_CheckReject(sector_t *sec1, sector_t *sec2)
{
    uint        s1, s2;
    uint        pnum, bytenum, bitnum;

    if(rejectMatrix != NULL)
    {
        // Determine subsector entries in REJECT table.
        s1 = GET_SECTOR_IDX(sec1);
        s2 = GET_SECTOR_IDX(sec2);
        pnum = s1 * numSectors + s2;
        bytenum = pnum >> 3;
        bitnum = 1 << (pnum & 7);

        // Check in REJECT table.
        if(rejectMatrix[bytenum] & bitnum)
        {
            sightcounts[0]++;
            // Can't possibly be connected.
            return false;
        }
    }
    return true;
}

/**
 * Look from eyes of t1 to any part of t2 (start from middle of t1).
 *
 * @param t1            The mobj doing the looking.
 * @param t2            The mobj being looked at.
 *
 * @return              @c true if a straight line between t1 and t2 is
 *                      unobstructed.
 */
boolean P_CheckSight(mobj_t *t1, mobj_t *t2)
{
    // If either is unlinked, they can't see each other.
    if(!t1->subsector || !t2->subsector)
        return false;

    // Check for trivial rejection.
    if(!P_CheckReject(t1->subsector->sector, t2->subsector->sector))
        return false;

    if(t2->dPlayer && t2->dPlayer->flags & DDPF_CAMERA)
        return false; // Cameramen don't exist!

    // Check precisely.
    sightStartZ = t1->pos[VZ];
    if(!(t1->dPlayer && t1->dPlayer->flags & DDPF_CAMERA))
        sightStartZ += t1->height + -(t1->height / 4);

    topSlope = t2->pos[VZ] + t2->height - sightStartZ;
    bottomSlope = t2->pos[VZ] - sightStartZ;

    return P_SightPathTraverse(t1->pos[VX], t1->pos[VY],
                               t2->pos[VX], t2->pos[VY]);
}

boolean P_CheckLineSight(float from[3], float to[3])
{
    sightStartZ = from[VZ];
    topSlope = to[VZ] + 1 - sightStartZ;
    bottomSlope = to[VZ] - 1 - sightStartZ;

    return P_SightPathTraverse(from[VX], from[VY], to[VX], to[VY]);
}
