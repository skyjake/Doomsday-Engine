/**
 * @file hedges.cpp
 * BSP Builder Half-edges. @ingroup bsp
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

#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "edit_map.h"
#include "m_misc.h"
#include "hedge.h"

#include "map/bsp/partitioner.h"

using namespace de::bsp;

static void updateHEdgeInfo(const HEdge* hedge, BspHEdgeInfo* info)
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

    if(info->pLength <= 0)
        Con_Error("HEdge {%p} is of zero length.", hedge);
}

HEdge* Partitioner::newHEdge(LineDef* lineDef, LineDef* sourceLineDef,
    Vertex* start, Vertex* end, Sector* sec, bool back)
{
    HEdge* hedge = HEdge_New();

    hedge->v[0] = start;
    hedge->v[1] = end;
    hedge->sector = sec;
    Q_ASSERT(sec == NULL || GameMap_SectorIndex(map, sec) >= 0);
    hedge->side = (back? 1 : 0);

    BspHEdgeInfo* info = static_cast<BspHEdgeInfo*>(Z_Malloc(sizeof *info, PU_MAP, 0));
    HEdge_AttachBspBuildInfo(hedge, info);

    info->lineDef = lineDef;
    info->sourceLineDef = sourceLineDef;
    info->nextOnSide = info->prevOnSide = NULL;
    info->block = NULL;
    updateHEdgeInfo(hedge, info);

    return hedge;
}

HEdge* Partitioner::cloneHEdge(const HEdge& other)
{
    HEdge* hedge = HEdge_NewCopy(&other);
    if(other.bspBuildInfo)
    {
        BspHEdgeInfo* info = static_cast<BspHEdgeInfo*>(Z_Malloc(sizeof *info, PU_MAP, 0));
        memcpy(info, other.bspBuildInfo, sizeof(BspHEdgeInfo));
        HEdge_AttachBspBuildInfo(hedge, info);
    }
    return hedge;
}

bool Partitioner::hedgeIsInLeaf(const HEdge* hedge) const
{
    /// @todo Are we now able to determine this by testing hedge->leaf ?
    return !hedge->bspBuildInfo->block;
}

HEdge* Partitioner::splitHEdge(HEdge* oldHEdge, const_pvec2d_t point)
{
    Q_ASSERT(oldHEdge);

    //LOG_DEBUG("Splitting hedge %p at [%1.1f, %1.1f].")
    //        << oldHEdge << x << y;

    Vertex* newVert = newVertex(point);
    addEdgeTip(newVert, M_SlopeToAngle(-oldHEdge->bspBuildInfo->pDX, -oldHEdge->bspBuildInfo->pDY), oldHEdge, oldHEdge->twin);
    addEdgeTip(newVert, M_SlopeToAngle( oldHEdge->bspBuildInfo->pDX,  oldHEdge->bspBuildInfo->pDY), oldHEdge->twin, oldHEdge);

    HEdge* newHEdge = cloneHEdge(*oldHEdge);

    newHEdge->bspBuildInfo->prevOnSide = oldHEdge;
    oldHEdge->bspBuildInfo->nextOnSide = newHEdge;

    oldHEdge->v[1] = newVert;
    updateHEdgeInfo(oldHEdge, oldHEdge->bspBuildInfo);

    newHEdge->v[0] = newVert;
    updateHEdgeInfo(newHEdge, newHEdge->bspBuildInfo);

    // Handle the twin.
    if(oldHEdge->twin)
    {
        //LOG_DEBUG("Splitting hedge twin %p.") << oldHEdge->twin;

        // Copy the old hedge info.
        newHEdge->twin = cloneHEdge(*oldHEdge->twin);
        newHEdge->twin->twin = newHEdge;

        newHEdge->twin->bspBuildInfo->nextOnSide = oldHEdge->twin;
        oldHEdge->twin->bspBuildInfo->prevOnSide = newHEdge->twin;

        oldHEdge->twin->v[0] = newVert;
        updateHEdgeInfo(oldHEdge->twin, oldHEdge->twin->bspBuildInfo);

        newHEdge->twin->v[1] = newVert;
        updateHEdgeInfo(newHEdge->twin, newHEdge->twin->bspBuildInfo);

        // Has this already been added to a leaf?
        if(hedgeIsInLeaf(oldHEdge->twin))
        {
            // Update the in-leaf references.
            oldHEdge->twin->next = newHEdge->twin;
        }
    }

    return newHEdge;
}
