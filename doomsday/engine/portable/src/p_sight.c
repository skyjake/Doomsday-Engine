/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// killough 4/19/98:
// Convert LOS info to struct for reentrancy and efficiency of data locality

static struct losdata_s {
    divline_t       trace;
    float           startZ; // Eye z of looker.
    float           topSlope; // Slope to top of target.
    float           bottomSlope; // Slope to bottom of target.
} los;

// CODE --------------------------------------------------------------------

static boolean sightTraverse(intercept_t* in)
{
    linedef_t*          li;
    float               slope;

    if(in && in->type == ICPT_LINE)
    {
        li = in->d.lineDef;

        // Crosses a two sided line.
        P_LineOpening(li);

        // Check for totally closed doors.
        if(openbottom >= opentop)
            return false; // Stop iteration.

        if(li->L_frontsector->SP_floorheight !=
           li->L_backsector->SP_floorheight)
        {
            slope = (openbottom - los.startZ) / in->frac;
            if(slope > los.bottomSlope)
                los.bottomSlope = slope;
        }

        if(li->L_frontsector->SP_ceilheight !=
           li->L_backsector->SP_ceilheight)
        {
            slope = (opentop - los.startZ) / in->frac;
            if(slope < los.topSlope)
                los.topSlope = slope;
        }

        if(los.topSlope <= los.bottomSlope)
            return false; // Stop iteration.
    }

    return true; // Continue iteration.
}

static boolean checkSightLine(linedef_t* line, void* data)
{
    int                 s[2];
    divline_t           dl;

    s[0] = P_PointOnDivlineSide(line->L_v1pos[VX],
                                line->L_v1pos[VY], &los.trace);
    s[1] = P_PointOnDivlineSide(line->L_v2pos[VX],
                                line->L_v2pos[VY], &los.trace);
    if(s[0] == s[1])
        return true; // Line isn't crossed, continue iteration.

    P_MakeDivline(line, &dl);
    s[0] = P_PointOnDivlineSide(FIX2FLT(los.trace.pos[VX]),
                                FIX2FLT(los.trace.pos[VY]), &dl);
    s[1] = P_PointOnDivlineSide(FIX2FLT(los.trace.pos[VX] + los.trace.dX),
                                FIX2FLT(los.trace.pos[VY] + los.trace.dY),
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
 * Traces a line of sight.
 *
 * @param from          World position, trace origin coordinates.
 * @param to            World position, trace target coordinates.
 *
 * @return              @c true if the traverser function returns @c true
 *                      for all visited lines.
 */
boolean P_CheckLineSight(const float from[3], const float to[3])
{
    uint                originBlock[2], destBlock[2];
    float               delta[2];
    float               partial;
    fixed_t             intercept[2], step[2];
    uint                block[2];
    int                 stepDir[2];
    int                 count;
    gamemap_t*          map = P_GetCurrentMap();
    vec2_t              origin, dest, min, max;

    los.startZ = from[VZ];
    los.topSlope = to[VZ] + 1 - los.startZ;
    los.bottomSlope = to[VZ] - 1 - los.startZ;

    V2_Copy(origin, from);
    V2_Copy(dest, to);

    P_GetBlockmapBounds(map->blockMap, min, max);

    if(!(origin[VX] >= min[VX] && origin[VX] <= max[VX] &&
         origin[VY] >= min[VY] && origin[VY] <= max[VY]))
    {   // Origin is outside the blockmap (really? very unusual...)
        return false;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((origin[VX] < min[VX] && dest[VX] < min[VX]) ||
       (origin[VX] > max[VX] && dest[VX] > max[VX]) ||
       (origin[VY] < min[VY] && dest[VY] < min[VY]) ||
       (origin[VY] > max[VY] && dest[VY] > max[VY]))
    {   // Nothing intercepts outside the blockmap!
        return false;
    }

    // Don't side exactly on blockmap lines.
    if((FLT2FIX(origin[VX] - min[VX]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VX] += 1;
    if((FLT2FIX(origin[VY] - min[VY]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VY] += 1;

    los.trace.pos[VX] = FLT2FIX(origin[VX]);
    los.trace.pos[VY] = FLT2FIX(origin[VY]);
    los.trace.dX = FLT2FIX(dest[VX] - origin[VX]);
    los.trace.dY = FLT2FIX(dest[VY] - origin[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that dest is within the AABB of the blockmap (note we
     * would have already abandoned if origin lay outside. Also, to avoid
     * potential rounding errors which might occur when determining the
     * blocks later, we will shrink the bbox slightly first.
     */

    if(!(dest[VX] >= min[VX] && dest[VX] <= max[VX] &&
         dest[VY] >= min[VY] && dest[VY] <= max[VY]))
    {   // Dest is outside the blockmap.
        float               ab;
        vec2_t              bbox[4], point;

        V2_Set(bbox[0], min[VX] + 1, min[VY] + 1);
        V2_Set(bbox[1], min[VX] + 1, max[VY] - 1);
        V2_Set(bbox[2], max[VX] - 1, max[VY] - 1);
        V2_Set(bbox[3], max[VX] - 1, min[VY] + 1);

        ab = V2_Intercept(origin, dest, bbox[0], bbox[1], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[1], bbox[2], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[2], bbox[3], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[3], bbox[0], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);
    }

    if(!(P_ToBlockmapBlockIdx(map->blockMap, originBlock, origin) &&
         P_ToBlockmapBlockIdx(map->blockMap, destBlock, dest)))
    {   // Should never get here due to the clipping above.
        return false;
    }

    validCount++;
    P_ClearIntercepts();

    V2_Subtract(origin, origin, min);
    V2_Subtract(dest, dest, min);

    if(destBlock[VX] > originBlock[VX])
    {
        stepDir[VX] = 1;
        partial = 1 - FIX2FLT(
            (FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else if(destBlock[VX] < originBlock[VX])
    {
        stepDir[VX] = -1;
        partial = FIX2FLT(
            (FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
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
        partial = 1 - FIX2FLT(
            (FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else if(destBlock[VY] < originBlock[VY])
    {
        stepDir[VY] = -1;
        partial = FIX2FLT(
            (FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
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
                                               checkSightLine, 0))
            {
                return false; // Early out.
            }
        }

        if(!P_BlockmapLinesIterator(BlockMap, block, checkSightLine, 0))
        {
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

    // Couldn't early out, so go through the sorted list.
    P_CalcInterceptDistances(&los.trace);

    return P_TraverseIntercepts(sightTraverse, -1);
}
