/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2006 James Haley <haleyjd@hotmail.com>
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

typedef struct losdata_s {
    divline_t       trace;
    float           startZ; // Eye z of looker.
    float           topSlope; // Slope to top of target.
    float           bottomSlope; // Slope to bottom of target.
    float           bBox[4];
    float           to[2];
} losdata_t;

// CODE --------------------------------------------------------------------

static boolean crossLineDef(const linedef_t* li, losdata_t* los,
                            divline_t* dl)
{
    divline_t           localDL, *dlPtr;

    // Try a quick, bounding-box rejection.
    if(li->bBox[BOXLEFT]   > los->bBox[BOXRIGHT] ||
       li->bBox[BOXRIGHT]  < los->bBox[BOXLEFT] ||
       li->bBox[BOXBOTTOM] > los->bBox[BOXTOP] ||
       li->bBox[BOXTOP]    < los->bBox[BOXBOTTOM])
        return false;

    if(P_PointOnDivlineSide(li->L_v1pos[VX], li->L_v1pos[VY],
                            &los->trace) ==
       P_PointOnDivlineSide(li->L_v2pos[VX], li->L_v2pos[VY],
                            &los->trace))
        return false; // Not crossed.

    if(dl)
        dlPtr = dl;
    else
        dlPtr = &localDL;

    P_MakeDivline(li, dlPtr);

    if(P_PointOnDivlineSide(FIX2FLT(los->trace.pos[VX]),
                            FIX2FLT(los->trace.pos[VY]), dlPtr) ==
       P_PointOnDivlineSide(los->to[VX], los->to[VY], dlPtr))
        return false; // Not crossed.

    return true; // Crossed.
}

/**
 * @return              @c true iff trace crosses the given subsector.
 */
static boolean crossSSec(uint ssecIdx, losdata_t* los)
{
    const subsector_t*  ssec = &ssectors[ssecIdx];

    if(ssec->polyObj)
    {   // Check polyobj lines.
        polyobj_t*          po = ssec->polyObj;
        seg_t**             segPtr = po->segs;

        while(*segPtr)
        {
            seg_t*              seg = *segPtr;

            if(seg->lineDef && seg->lineDef->validCount != validCount)
            {
                linedef_t*          li = seg->lineDef;

                li->validCount = validCount;

                if(crossLineDef(li, los, NULL))
                    return false; // Stop iteration.
            }

            *segPtr++;
        }
    }

    {
    // Check lines.
    seg_t**             segPtr = ssec->segs;
    while(*segPtr)
    {
        seg_t*              seg = *segPtr;

        if(seg->lineDef && seg->lineDef->validCount != validCount)
        {
            const linedef_t*    li = seg->lineDef;
            divline_t           dl;

            seg->lineDef->validCount = validCount;

            if(crossLineDef(li, los, &dl))
            {
                const sector_t*     front, *back;

                // Must be twosided.
                if(!li->L_frontside || !li->L_backside)
                    return false; // Stop iteration.

                front = seg->SG_frontsector;
                back  = seg->SG_backsector;

                // Do we need to check for occluding upper lower sections?
                if(front->SP_floorheight != back->SP_floorheight ||
                   front->SP_ceilheight  != back->SP_ceilheight)
                {   // A possible occluder.
                    float               openTop, openBottom, frac;

                    if(front->SP_ceilheight < back->SP_ceilheight)
                        openTop = front->SP_ceilheight;
                    else
                        openTop = back->SP_ceilheight;

                    if(front->SP_floorheight > back->SP_floorheight)
                        openBottom = front->SP_floorheight;
                    else
                        openBottom = back->SP_floorheight;

                    // Completely closed?
                    if(openBottom >= openTop)
                        return false; // Stop iteration.

                    frac = P_InterceptVector(&los->trace, &dl);

                    if(front->SP_floorheight != back->SP_floorheight)
                    {
                        float               slope =
                            (openBottom - los->startZ) / frac;

                        if(slope > los->bottomSlope)
                            los->bottomSlope = slope;
                    }

                    if(front->SP_ceilheight != back->SP_ceilheight)
                    {
                        float               slope =
                            (openTop - los->startZ) / frac;

                        if(slope < los->topSlope)
                            los->topSlope = slope;
                    }

                    if(los->topSlope <= los->bottomSlope)
                        return false; // Stop iteration.
                }
            }
        }

        *segPtr++;
    }
    }

    return true; // Continue iteration.
}

/**
 * @return              @c true iff trace crosses the node.
 */
static boolean crossBSPNode(unsigned int bspNum, losdata_t* los)
{
    while(!(bspNum & NF_SUBSECTOR))
    {
        const node_t*       node = &nodes[bspNum];
        int                 side = R_PointOnSide(
            FIX2FLT(los->trace.pos[VX]), FIX2FLT(los->trace.pos[VY]),
            &node->partition);

        // Would the trace completely cross this partition?
        if(side == R_PointOnSide(los->to[VX], los->to[VY],
                                 &node->partition))
        {   // Yes, decend!
            bspNum = node->children[side];
        }
        else
        {   // No.
            if(!crossBSPNode(node->children[side], los))
                return 0; // Cross the starting side.
            else
                bspNum = node->children[side^1]; // Cross the ending side.
        }
    }

    return crossSSec(bspNum & ~NF_SUBSECTOR, los);
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
boolean P_CheckLineSight(const float from[3], const float to[3],
                         float bottomSlope, float topSlope)
{
    losdata_t               los;

    los.startZ = from[VZ];
    los.topSlope = to[VZ] + topSlope - los.startZ;
    los.bottomSlope = to[VZ] + bottomSlope - los.startZ;
    los.trace.pos[VX] = FLT2FIX(from[VX]);
    los.trace.pos[VY] = FLT2FIX(from[VY]);
    los.trace.dX = FLT2FIX(to[VX] - from[VX]);
    los.trace.dY = FLT2FIX(to[VY] - from[VY]);
    los.to[VX] = to[VX];
    los.to[VY] = to[VY];

    if(from[VX] > to[VX])
    {
        los.bBox[BOXRIGHT]  = from[VX];
        los.bBox[BOXLEFT]   = to[VX];
    }
    else
    {
        los.bBox[BOXRIGHT]  = to[VX];
        los.bBox[BOXLEFT]   = from[VX];
    }

    if(from[VY] > to[VY])
    {
        los.bBox[BOXTOP]    = from[VY];
        los.bBox[BOXBOTTOM] = to[VY];
    }
    else
    {
        los.bBox[BOXTOP]    = to[VY];
        los.bBox[BOXBOTTOM] = from[VY];
    }

    validCount++;
    return crossBSPNode(numNodes - 1, &los);
}
