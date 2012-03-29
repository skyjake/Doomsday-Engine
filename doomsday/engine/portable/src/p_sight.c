/**
 * @file p_sight.c
 * Gamemap Line of Sight Testing. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

typedef struct losdata_s {
    int flags; // LS_* flags @see lineSightFlags
    divline_t trace;
    float startZ; // Eye z of looker.
    float topSlope; // Slope to top of target.
    float bottomSlope; // Slope to bottom of target.
    float bBox[4];
    float to[3];
} losdata_t;

static boolean interceptLineDef(const LineDef* li, losdata_t* los, divline_t* dl)
{
    divline_t localDL, *dlPtr;

    // Try a quick, bounding-box rejection.
    if(li->aaBox.minX > los->bBox[BOXRIGHT] ||
       li->aaBox.maxX < los->bBox[BOXLEFT] ||
       li->aaBox.minY > los->bBox[BOXTOP] ||
       li->aaBox.maxY < los->bBox[BOXBOTTOM])
        return false;

    if(P_PointOnDivlineSide(li->L_v1pos[VX], li->L_v1pos[VY], &los->trace) ==
       P_PointOnDivlineSide(li->L_v2pos[VX], li->L_v2pos[VY], &los->trace))
        return false; // Not crossed.

    if(dl)
        dlPtr = dl;
    else
        dlPtr = &localDL;

    P_MakeDivline(li, dlPtr);

    if(P_PointOnDivlineSide(FIX2FLT(los->trace.pos[VX]), FIX2FLT(los->trace.pos[VY]), dlPtr) ==
       P_PointOnDivlineSide(los->to[VX], los->to[VY], dlPtr))
        return false; // Not crossed.

    return true; // Crossed.
}

static boolean crossLineDef(const LineDef* li, byte side, losdata_t* los)
{
#define RTOP            0x1
#define RBOTTOM         0x2

    float frac;
    byte ranges = 0;
    divline_t dl;
    const Sector* fsec, *bsec;
    boolean noBack;

    if(!interceptLineDef(li, los, &dl))
        return true; // Ray does not intercept hedge on the X/Y plane.

    if(!li->L_side(side))
        return true; // HEdge is on the back side of a one-sided window.

    fsec = li->L_sector(side);
    bsec  = (li->L_backside? li->L_sector(side^1) : NULL);
    noBack = li->L_backside? false : true;

    if(!noBack && !(los->flags & LS_PASSLEFT) &&
       (!(bsec->SP_floorheight < fsec->SP_ceilheight) ||
        !(fsec->SP_floorheight < bsec->SP_ceilheight)))
        noBack = true;

    if(noBack)
    {
        if((los->flags & LS_PASSLEFT) &&
           P_PointXYOnLineDefSide(FIX2FLT(los->trace.pos[VX]),
                                  FIX2FLT(los->trace.pos[VY]), li))
            return true; // Ray does not intercept hedge from left to right.

        if(!(los->flags & (LS_PASSOVER | LS_PASSUNDER)))
            return false; // Stop iteration.
    }

    // Handle the case of a zero height backside in the top range.
    if(noBack)
    {
        ranges |= RTOP;
    }
    else
    {
        if(bsec->SP_floorheight != fsec->SP_floorheight)
            ranges |= RBOTTOM;
        if(bsec->SP_ceilheight != fsec->SP_ceilheight)
            ranges |= RTOP;
    }

    if(!ranges)
        return true;

    frac = P_InterceptVector(&los->trace, &dl);

    if((los->flags & LS_PASSOVER) &&
       los->bottomSlope > (fsec->SP_ceilheight - los->startZ) / frac)
        return true;

    if((los->flags & LS_PASSUNDER) &&
       los->topSlope < (fsec->SP_floorheight - los->startZ) / frac)
        return true;

    if(ranges & RTOP)
    {
        float top = (noBack? fsec->SP_ceilheight : fsec->SP_ceilheight < bsec->SP_ceilheight? fsec->SP_ceilheight : bsec->SP_ceilheight);
        float slope = (top - los->startZ) / frac;

        if((slope < los->topSlope) ^ (noBack && !(los->flags & LS_PASSOVER)) ||
           (noBack && los->topSlope > (fsec->SP_floorheight - los->startZ) / frac))
            los->topSlope = slope;
        if((slope < los->bottomSlope) ^ (noBack && !(los->flags & LS_PASSUNDER)) ||
           (noBack && los->bottomSlope > (fsec->SP_floorheight - los->startZ) / frac))
            los->bottomSlope = slope;
    }

    if(ranges & RBOTTOM)
    {
        float bottom = (noBack? fsec->SP_floorheight : fsec->SP_floorheight > bsec->SP_floorheight? fsec->SP_floorheight : bsec->SP_floorheight);
        float slope = (bottom - los->startZ) / frac;

        if(slope > los->bottomSlope)
            los->bottomSlope = slope;
        if(slope > los->topSlope)
            los->topSlope = slope;
    }

    if(los->topSlope <= los->bottomSlope)
        return false; // Stop iteration.

    return true;

#undef RTOP
#undef RBOTTOM
}

/**
 * @return  @c true iff trace crosses the given BSP leaf.
 */
static boolean crossBspLeaf(GameMap* map, const BspLeaf* bspLeaf, losdata_t* los)
{
    assert(bspLeaf);
    if(bspLeaf->polyObj)
    {
        // Check polyobj lines.
        Polyobj* po = bspLeaf->polyObj;
        LineDef** lineIter = po->lines;
        while(*lineIter)
        {
            LineDef* line = *lineIter;
            if(line->validCount != validCount)
            {
                line->validCount = validCount;
                if(!crossLineDef(line, FRONT, los))
                    return false; // Stop iteration.
            }
            lineIter++;
        }
    }

    // Check edges.
    { HEdge** hedgeIter = bspLeaf->hedges;
    while(*hedgeIter)
    {
        const HEdge* hedge = *hedgeIter;
        if(hedge->lineDef && hedge->lineDef->validCount != validCount)
        {
            LineDef* li = hedge->lineDef;
            li->validCount = validCount;
            if(!crossLineDef(li, hedge->side, los))
                return false;
        }
        hedgeIter++;
    }}

    return true; // Continue iteration.
}

/**
 * @return  @c true iff trace crosses the node.
 */
static boolean crossBspNode(GameMap* map, runtime_mapdata_header_t* bspPtr, losdata_t* los)
{
    while(bspPtr->type != DMU_BSPLEAF)
    {
        const BspNode* node = (BspNode*)bspPtr;
        int side = P_PointOnPartitionSide(FIX2FLT(los->trace.pos[VX]), FIX2FLT(los->trace.pos[VY]),
                                          &node->partition);

        // Would the trace completely cross this partition?
        if(side == P_PointOnPartitionSide(los->to[VX], los->to[VY], &node->partition))
        {
            // Yes, descend!
            bspPtr = node->children[side];
        }
        else
        {
            // No.
            if(!crossBspNode(map, node->children[side], los))
                return 0; // Cross the starting side.

            bspPtr = node->children[side^1]; // Cross the ending side.
        }
    }

    return crossBspLeaf(map, (BspLeaf*)bspPtr, los);
}

boolean GameMap_CheckLineSight(GameMap* map, const float from[3], const float to[3],
    float bottomSlope, float topSlope, int flags)
{
    losdata_t los;
    assert(map);

    los.flags = flags;
    los.startZ = from[VZ];
    los.topSlope = to[VZ] + topSlope - los.startZ;
    los.bottomSlope = to[VZ] + bottomSlope - los.startZ;
    los.trace.pos[VX] = FLT2FIX(from[VX]);
    los.trace.pos[VY] = FLT2FIX(from[VY]);
    los.trace.dX = FLT2FIX(to[VX] - from[VX]);
    los.trace.dY = FLT2FIX(to[VY] - from[VY]);
    los.to[VX] = to[VX];
    los.to[VY] = to[VY];
    los.to[VZ] = to[VZ];

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
    // A single leaf is a special case.
    if(!map->bsp)
    {
        return crossBspLeaf(map, map->bspLeafs, &los);
    }
    return crossBspNode(map, (runtime_mapdata_header_t*)(map->bsp), &los);
}
