/**\file bsp_edge.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
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
 * GL-friendly BSP node builder, half-edges.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include "de_base.h"
#include "de_console.h"
#include "de_bsp.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static zblockset_t *hEdgeBlockSet;
static boolean hEdgeAllocatorInited = false;

// CODE --------------------------------------------------------------------

static __inline hedge_t *allocHEdge(void)
{
    if(hEdgeAllocatorInited)
    {   // Use the block allocator.
        hedge_t            *hEdge = ZBlockSet_Allocate(hEdgeBlockSet);
        memset(hEdge, 0, sizeof(hedge_t));
        return hEdge;
    }

    return M_Calloc(sizeof(hedge_t));
}

static __inline void freeHEdge(hedge_t *hEdge)
{
    if(hEdgeAllocatorInited)
    {   // Ignore, it'll be free'd along with the block allocator.
        return;
    }

    M_Free(hEdge);
}

static __inline edgetip_t *allocEdgeTip(void)
{
    return M_Calloc(sizeof(edgetip_t));
}

static __inline void freeEdgeTip(edgetip_t *tip)
{
    M_Free(tip);
}

/**
 * Init the half-edge block allocator.
 */
void BSP_InitHEdgeAllocator(void)
{
    if(hEdgeAllocatorInited)
        return; // Already been here.

    hEdgeBlockSet = ZBlockSet_New(sizeof(hedge_t), 512, PU_APPSTATIC);
    hEdgeAllocatorInited = true;
}

/**
 * Shutdown the half-edge block allocator. All elements will be free'd!
 */
void BSP_ShutdownHEdgeAllocator(void)
{
    if(hEdgeAllocatorInited)
    {
        ZBlockSet_Delete(hEdgeBlockSet);
        hEdgeBlockSet = NULL;

        hEdgeAllocatorInited = false;
    }
}

/**
 * Update the precomputed members of the hedge.
 */
static void updateHEdge(hedge_t *hedge)
{
    hedge->pSX = hedge->v[0]->buildData.pos[VX];
    hedge->pSY = hedge->v[0]->buildData.pos[VY];
    hedge->pEX = hedge->v[1]->buildData.pos[VX];
    hedge->pEY = hedge->v[1]->buildData.pos[VY];
    hedge->pDX = hedge->pEX - hedge->pSX;
    hedge->pDY = hedge->pEY - hedge->pSY;

    hedge->pLength = M_Length(hedge->pDX, hedge->pDY);
    hedge->pAngle  = M_SlopeToAngle(hedge->pDX, hedge->pDY);

    if(hedge->pLength <= 0)
        Con_Error("Hedge %p has zero p_length.", hedge);

    hedge->pPerp =  hedge->pSY * hedge->pDX - hedge->pSX * hedge->pDY;
    hedge->pPara = -hedge->pSX * hedge->pDX - hedge->pSY * hedge->pDY;
}

/**
 * Create a new half-edge.
 */
hedge_t *HEdge_Create(linedef_t *line, linedef_t *sourceLine,
                      vertex_t *start, vertex_t *end, sector_t *sec,
                      boolean back)
{
    hedge_t            *hEdge = allocHEdge();

    hEdge->v[0] = start;
    hEdge->v[1] = end;
    hEdge->lineDef = line;
    hEdge->side = (back? 1 : 0);
    hEdge->sector = sec;
    hEdge->twin = NULL;
    hEdge->nextOnSide = hEdge->prevOnSide = NULL;

    hEdge->sourceLine = sourceLine;
    hEdge->index = -1;

    updateHEdge(hEdge);

    return hEdge;
}

/**
 * Destroys the given half-edge.
 *
 * @param hEdge         Ptr to the half-edge to be destroyed.
 */
void HEdge_Destroy(hedge_t *hEdge)
{
    if(hEdge)
    {
        freeHEdge(hEdge);
    }
}

/**
 * Splits the given half-edge at the point (x,y). The new half-edge is
 * returned. The old half-edge is shortened (the original start vertex is
 * unchanged), the new half-edge becomes the cut-off tail (keeping the
 * original end vertex).
 *
 * \note
 * If the half-edge has a twin, it is also split and is inserted into the
 * same list as the original (and after it), thus all half-edges (except the
 * one we are currently splitting) must exist on a singly-linked list
 * somewhere.
 *
 * \note
 * We must update the count values of any superblock that contains the
 * half-edge (and/or backseg), so that future processing is not messed up by
 * incorrect counts.
 */
hedge_t *HEdge_Split(hedge_t *oldHEdge, double x, double y)
{
    hedge_t            *newHEdge;
    vertex_t           *newVert;

/*#if _DEBUG
if(oldHEdge->lineDef)
    Con_Message("Splitting Linedef %d (%p) at (%1.1f,%1.1f)\n",
                oldHEdge->lineDef->index, oldHEdge, x, y);
else
    Con_Message("Splitting MiniHEdge %p at (%1.1f,%1.1f)\n", oldHEdge, x, y);
#endif*/

    // Update superblock, if needed.
    if(oldHEdge->block)
        BSP_IncSuperBlockHEdgeCounts(oldHEdge->block,
                                     (oldHEdge->lineDef != NULL));
    /**
     * Create a new vertex (with correct wall_tip info) for the split that
     * happens along the given half-edge at the given location.
     */
    newVert = createVertex();
    newVert->buildData.pos[VX] = x;
    newVert->buildData.pos[VY] = y;
    newVert->buildData.refCount = (oldHEdge->twin? 4 : 2);

    // Compute wall_tip info.
    BSP_CreateVertexEdgeTip(newVert, -oldHEdge->pDX, -oldHEdge->pDY,
                            oldHEdge, oldHEdge->twin);
    BSP_CreateVertexEdgeTip(newVert, oldHEdge->pDX, oldHEdge->pDY,
                            oldHEdge->twin, oldHEdge);

    newHEdge = allocHEdge();

    // Copy the old half-edge info.
    memcpy(newHEdge, oldHEdge, sizeof(hedge_t));
    newHEdge->next = NULL;

    newHEdge->prevOnSide = oldHEdge;
    oldHEdge->nextOnSide = newHEdge;

    oldHEdge->v[1] = newVert;
    updateHEdge(oldHEdge);

    newHEdge->v[0] = newVert;
    updateHEdge(newHEdge);

/*#if _DEBUG
Con_Message("Splitting Vertex is %04X at (%1.1f,%1.1f)\n",
            newVert->index, newVert->V_pos[VX], newVert->V_pos[VY]);
#endif*/

    // Handle the twin.
    if(oldHEdge->twin)
    {
/*#if _DEBUG
Con_Message("Splitting hEdge->twin %p\n", oldHEdge->twin);
#endif*/

        // Update superblock, if needed.
        if(oldHEdge->twin->block)
            BSP_IncSuperBlockHEdgeCounts(oldHEdge->twin->block,
                                         (oldHEdge->twin != NULL));

        newHEdge->twin = allocHEdge();

        // Copy seg info.
        memcpy(newHEdge->twin, oldHEdge->twin, sizeof(hedge_t));

        // It is important to keep the twin relationship valid.
        newHEdge->twin->twin = newHEdge;

        newHEdge->twin->nextOnSide = oldHEdge->twin;
        oldHEdge->twin->prevOnSide = newHEdge->twin;

        oldHEdge->twin->v[0] = newVert;
        updateHEdge(oldHEdge->twin);

        newHEdge->twin->v[1] = newVert;
        updateHEdge(newHEdge->twin);

        // Link it into list.
        oldHEdge->twin->next = newHEdge->twin;
    }

    return newHEdge;
}

void BSP_CreateVertexEdgeTip(vertex_t *vert, double dx, double dy,
                             hedge_t *back, hedge_t *front)
{
    edgetip_t          *tip = allocEdgeTip();
    edgetip_t          *after;

    tip->angle = M_SlopeToAngle(dx, dy);
    tip->ET_edge[BACK]  = back;
    tip->ET_edge[FRONT] = front;

    // Find the correct place (order is increasing angle).
    for(after = vert->buildData.tipSet; after && after->ET_next;
        after = after->ET_next);

    while(after && tip->angle + ANG_EPSILON < after->angle)
        after = after->ET_prev;

    // Link it in.
    if(after)
        tip->ET_next = after->ET_next;
    else
        tip->ET_next = vert->buildData.tipSet;
    tip->ET_prev = after;

    if(after)
    {
        if(after->ET_next)
            after->ET_next->ET_prev = tip;

        after->ET_next = tip;
    }
    else
    {
        if(vert->buildData.tipSet)
            vert->buildData.tipSet->ET_prev = tip;

        vert->buildData.tipSet = tip;
    }
}

void BSP_DestroyVertexEdgeTip(edgetip_t *tip)
{
    if(tip)
    {
        freeEdgeTip(tip);
    }
}

/**
 * Check whether a line with the given delta coordinates and beginning
 * at this vertex is open. Returns a sector reference if it's open,
 * or NULL if closed (void space or directly along a linedef).
 */
sector_t *BSP_VertexCheckOpen(vertex_t *vert, double dX, double dY)
{
    edgetip_t          *tip;
    angle_g             angle = M_SlopeToAngle(dX, dY);

    // First check whether there's a wall_tip that lies in the exact
    // direction of the given direction (which is relative to the
    // vertex).
    for(tip = vert->buildData.tipSet; tip; tip = tip->ET_next)
    {
        angle_g     diff = fabs(tip->angle - angle);

        if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
        {   // Yes, found one.
            return NULL;
        }
    }

    // OK, now just find the first wall_tip whose angle is greater than
    // the angle we're interested in. Therefore we'll be on the FRONT
    // side of that tip edge.
    for(tip = vert->buildData.tipSet; tip; tip = tip->ET_next)
    {
        if(angle + ANG_EPSILON < tip->angle)
        {   // Found it.
            return (tip->ET_edge[FRONT]? tip->ET_edge[FRONT]->sector : NULL);
        }

        if(!tip->ET_next)
        {
            // No more tips, thus we must be on the BACK side of the tip
            // with the largest angle.
            return (tip->ET_edge[BACK]? tip->ET_edge[BACK]->sector : NULL);
        }
    }

    Con_Error("Vertex %d has no tips !", vert->buildData.index);
    return NULL;
}
