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

/**
 * @todo Why are we not making use of the blockmap here? It would be much more
 *       efficient to use the blockmap, taking advantage of the inherent locality
 *       in the data structure.
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
    AABoxd aaBox;
    coord_t to[3];
} losdata_t;

static boolean interceptLineDef(const LineDef* li, losdata_t* los, divline_t* dl)
{
    divline_t localDL, *dlPtr;

    // Try a quick, bounding-box rejection.
    if(li->aaBox.minX > los->aaBox.maxX ||
       li->aaBox.maxX < los->aaBox.minX ||
       li->aaBox.minY > los->aaBox.maxY ||
       li->aaBox.maxY < los->aaBox.minY)
        return false;

    if(Divline_PointOnSide(&los->trace, li->L_v1origin) ==
       Divline_PointOnSide(&los->trace, li->L_v2origin))
        return false; // Not crossed.

    if(dl)
        dlPtr = dl;
    else
        dlPtr = &localDL;

    LineDef_SetDivline(li, dlPtr);

    if(Divline_PointXYOnSide(dlPtr, FIX2FLT(los->trace.origin[VX]), FIX2FLT(los->trace.origin[VY])) ==
       Divline_PointOnSide(dlPtr, los->to))
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
           LineDef_PointXYOnSide(li, FIX2FLT(los->trace.origin[VX]),
                                     FIX2FLT(los->trace.origin[VY])) < 0)
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

    frac = FIX2FLT(Divline_Intersection(&dl, &los->trace));

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
    if(bspLeaf->hedge)
    {
        const HEdge* hedge = bspLeaf->hedge;
        do
        {
            if(hedge->lineDef && hedge->lineDef->validCount != validCount)
            {
                LineDef* li = hedge->lineDef;
                li->validCount = validCount;
                if(!crossLineDef(li, hedge->side, los))
                    return false;
            }
        } while((hedge = hedge->next) != bspLeaf->hedge);
    }

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
        int side = Partition_PointXYOnSide(&node->partition,
                                         FIX2FLT(los->trace.origin[VX]), FIX2FLT(los->trace.origin[VY]));

        // Would the trace completely cross this partition?
        if(side == Partition_PointOnSide(&node->partition, los->to))
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

boolean GameMap_CheckLineSight(GameMap* map, const coord_t from[3], const coord_t to[3],
    coord_t bottomSlope, coord_t topSlope, int flags)
{
    losdata_t los;
    assert(map);

    los.flags = flags;
    los.startZ = from[VZ];
    los.topSlope = to[VZ] + topSlope - los.startZ;
    los.bottomSlope = to[VZ] + bottomSlope - los.startZ;
    los.trace.origin[VX] = FLT2FIX((float)from[VX]);
    los.trace.origin[VY] = FLT2FIX((float)from[VY]);
    los.trace.direction[VX] = FLT2FIX((float)(to[VX] - from[VX]));
    los.trace.direction[VY] = FLT2FIX((float)(to[VY] - from[VY]));
    los.to[VX] = to[VX];
    los.to[VY] = to[VY];
    los.to[VZ] = to[VZ];

    if(from[VX] > to[VX])
    {
        los.aaBox.maxX = from[VX];
        los.aaBox.minX = to[VX];
    }
    else
    {
        los.aaBox.maxX = to[VX];
        los.aaBox.minX = from[VX];
    }

    if(from[VY] > to[VY])
    {
        los.aaBox.maxY = from[VY];
        los.aaBox.minY = to[VY];
    }
    else
    {
        los.aaBox.maxY = to[VY];
        los.aaBox.minY = from[VY];
    }

    validCount++;
    return crossBspNode(map, map->bsp, &los);
}
