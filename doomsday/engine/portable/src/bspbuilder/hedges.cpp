/**
 * @file bsp_edge.c
 * BSP Builder Half-edges. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include "de_base.h"
#include "de_console.h"
#include "de_bsp.h"
#include "de_edit.h"

#include "m_misc.h"

#include "bspbuilder/hedges.hh"
#include "bspbuilder/superblockmap.hh"
#include "bspbuilder/bspbuilder.hh"

using namespace de;

static void initBspHEdgeInfo(const bsp_hedge_t* hedge, BspHEdgeInfo* info)
{
    assert(hedge);
    if(!info) return;

    info->pSX = hedge->v[0]->buildData.pos[VX];
    info->pSY = hedge->v[0]->buildData.pos[VY];
    info->pEX = hedge->v[1]->buildData.pos[VX];
    info->pEY = hedge->v[1]->buildData.pos[VY];
    info->pDX = info->pEX - info->pSX;
    info->pDY = info->pEY - info->pSY;

    info->pLength = M_Length(info->pDX, info->pDY);
    info->pAngle  = M_SlopeToAngle(info->pDX, info->pDY);

    info->pPerp =  info->pSY * info->pDX - info->pSX * info->pDY;
    info->pPara = -info->pSX * info->pDX - info->pSY * info->pDY;
}

static void updateBspHEdgeInfo(const bsp_hedge_t* hedge, BspHEdgeInfo* info)
{
    assert(hedge);
    if(!info) return;

    initBspHEdgeInfo(hedge, info);
    if(info->pLength <= 0)
        Con_Error("HEdge {%p} is of zero length.", hedge);
}

bsp_hedge_t* BspBuilder::newHEdge(LineDef* lineDef, LineDef* sourceLineDef,
    Vertex* start, Vertex* end, Sector* sec, boolean back)
{
    bsp_hedge_t* hedge = allocHEdge();

    hedge->v[0] = start;
    hedge->v[1] = end;
    hedge->side = (back? 1 : 0);
    hedge->sector = sec;
    hedge->twin = NULL;
    hedge->nextOnSide = hedge->prevOnSide = NULL;
    hedge->index = -1;

    updateBspHEdgeInfo(hedge, &hedge->info);
    hedge->info.lineDef = lineDef;
    hedge->info.sourceLineDef = sourceLineDef;

    return hedge;
}

void BspBuilder::deleteHEdge(bsp_hedge_t* hedge)
{
    if(hedge)
    {
        freeHEdge(hedge);
    }
}

bsp_hedge_t* BspBuilder::splitHEdge(bsp_hedge_t* oldHEdge, double x, double y)
{
    bsp_hedge_t* newHEdge;
    Vertex* newVert;

/*#if _DEBUG
    if(oldHEdge->lineDef)
        Con_Message("Splitting Linedef %d (%p) at (%1.1f,%1.1f)\n", oldHEdge->lineDef->index, oldHEdge, x, y);
    else
        Con_Message("Splitting MiniHEdge %p at (%1.1f,%1.1f)\n", oldHEdge, x, y);
#endif*/

    /**
     * Create a new vertex (with correct wall_tip info) for the split that
     * happens along the given half-edge at the given location.
     */
    newVert = createVertex();
    newVert->buildData.pos[VX] = x;
    newVert->buildData.pos[VY] = y;
    newVert->buildData.refCount = (oldHEdge->twin? 4 : 2);

    // Compute wall_tip info.
    addEdgeTip(newVert, -oldHEdge->info.pDX, -oldHEdge->info.pDY, oldHEdge, oldHEdge->twin);
    addEdgeTip(newVert,  oldHEdge->info.pDX,  oldHEdge->info.pDY, oldHEdge->twin, oldHEdge);

    newHEdge = allocHEdge();

    // Copy the old half-edge info.
    memcpy(newHEdge, oldHEdge, sizeof(bsp_hedge_t));

    newHEdge->prevOnSide = oldHEdge;
    oldHEdge->nextOnSide = newHEdge;

    oldHEdge->v[1] = newVert;
    updateBspHEdgeInfo(oldHEdge, &oldHEdge->info);

    newHEdge->v[0] = newVert;
    updateBspHEdgeInfo(newHEdge, &newHEdge->info);

    //DEBUG_Message(("Splitting Vertex is %04X at (%1.1f,%1.1f)\n",
    //               newVert->index, newVert->V_pos[VX], newVert->V_pos[VY]));

    // Handle the twin.
    if(oldHEdge->twin)
    {
        //DEBUG_Message(("Splitting hedge->twin %p\n", oldHEdge->twin));

        newHEdge->twin = allocHEdge();

        // Copy the old hedge info.
        memcpy(newHEdge->twin, oldHEdge->twin, sizeof(bsp_hedge_t));

        // It is important to keep the twin relationship valid.
        newHEdge->twin->twin = newHEdge;

        newHEdge->twin->nextOnSide = oldHEdge->twin;
        oldHEdge->twin->prevOnSide = newHEdge->twin;

        oldHEdge->twin->v[0] = newVert;
        updateBspHEdgeInfo(oldHEdge->twin, &oldHEdge->twin->info);

        newHEdge->twin->v[1] = newVert;
        updateBspHEdgeInfo(newHEdge->twin, &newHEdge->twin->info);

        // Update superblock, if needed.
        if(oldHEdge->twin->block)
        {
            SuperBlock_HEdgePush(oldHEdge->twin->block, newHEdge->twin);
        }
        else
        {
            oldHEdge->twin->nextInLeaf = newHEdge->twin;
        }
    }

    return newHEdge;
}

Sector* BspBuilder::openSectorAtPoint(Vertex* vert, double dX, double dY)
{
    edgetip_t* tip;
    double angle = M_SlopeToAngle(dX, dY);

    // First check whether there's a wall_tip that lies in the exact direction of
    // the given direction (which is relative to the vertex).
    for(tip = vert->buildData.tipSet; tip; tip = tip->ET_next)
    {
        double diff = fabs(tip->angle - angle);

        if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
        {   // Yes, found one.
            return NULL;
        }
    }

    // OK, now just find the first wall_tip whose angle is greater than the angle
    // we're interested in. Therefore we'll be on the FRONT side of that tip edge.
    for(tip = vert->buildData.tipSet; tip; tip = tip->ET_next)
    {
        if(angle + ANG_EPSILON < tip->angle)
        {
            // Found it.
            return (tip->ET_edge[FRONT]? tip->ET_edge[FRONT]->sector : NULL);
        }

        if(!tip->ET_next)
        {
            // No more tips, therefore this is the BACK of the tip with the largest angle.
            return (tip->ET_edge[BACK]? tip->ET_edge[BACK]->sector : NULL);
        }
    }

    Con_Error("Vertex %d has no tips !", vert->buildData.index);
    exit(1); // Unreachable.
}
