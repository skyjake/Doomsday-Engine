/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <yagisan@dengine.net>
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
 * bsp_edge.c: GL-friendly BSP node builder, half-edges.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

// MACROS ------------------------------------------------------------------

#define WT_edge                 hEdges

// TYPES -------------------------------------------------------------------

// An edge tip is where an edge meets a vertex.
typedef struct edgetip_s {
    // Link in list. List is kept in ANTI-clockwise order.
    struct edgetip_s *next;
    struct edgetip_s *prev;

    // Angle that line makes at vertex (degrees).
    angle_g     angle;

    // Segs on each side of the edge. Left is the side of increasing
    // angles, right is the side of decreasing angles. Either can be
    // NULL for one sided edges.
    struct hedge_s *hEdges[2];
} edgetip_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hedge_t **levHEdges = NULL;
static int numHEdges = 0;

static edgetip_t **hEdgeTips = NULL;
static int numHEdgeTips = 0;

// CODE --------------------------------------------------------------------

static hedge_t *allocHEdge(void)
{
#define BLKNUM 1024

    if((numHEdges % BLKNUM) == 0)
    {
        levHEdges =
            M_Realloc(levHEdges, (numHEdges + BLKNUM) * sizeof(hedge_t*));
    }

    levHEdges[numHEdges] = (hedge_t *) M_Calloc(sizeof(hedge_t));
    numHEdges += 1;

    return levHEdges[numHEdges - 1];

#undef BLKNUM
}

static void freeHEdge(hedge_t *hEdge)
{
    M_Free(hEdge);
}

static edgetip_t *allocEdgeTip(void)
{
#define BLKNUM 1024

    if((numHEdgeTips % BLKNUM) == 0) 
    {
        hEdgeTips = M_Realloc(hEdgeTips, (numHEdgeTips + BLKNUM) *
            sizeof(edgetip_t *));
    }

    hEdgeTips[numHEdgeTips] = (edgetip_t *) M_Calloc(sizeof(edgetip_t));
    numHEdgeTips += 1;

    return hEdgeTips[numHEdgeTips - 1];

#undef BLKNUM
}

/**
 * Update the precomputed members of the hedge.
 */
static void updateHEdge(hedge_t *hedge)
{
    hedge->pSX = hedge->v[0]->V_pos[VX];
    hedge->pSY = hedge->v[0]->V_pos[VY];
    hedge->pEX = hedge->v[1]->V_pos[VX];
    hedge->pEY = hedge->v[1]->V_pos[VY];
    hedge->pDX = hedge->pEX - hedge->pSX;
    hedge->pDY = hedge->pEY - hedge->pSY;

    hedge->pLength = M_Length(hedge->pDX, hedge->pDY);
    hedge->pAngle  = M_SlopeToAngle(hedge->pDX, hedge->pDY);

    if(hedge->pLength <= 0)
        Con_Error("Seg %p has zero p_length.", hedge);

    hedge->pPerp =  hedge->pSY * hedge->pDX - hedge->pSX * hedge->pDY;
    hedge->pPara = -hedge->pSX * hedge->pDX - hedge->pSY * hedge->pDY;
}

/**
 * Create a new vertex (with correct wall_tip info) for the split that
 * happens along the given half-edge at the given location.
 */
static mvertex_t *newVertexFromSplitHEdge(hedge_t *hEdge, double x, double y)
{
    mvertex_t *vert = NewVertex();

    vert->V_pos[VX] = x;
    vert->V_pos[VY] = y;

    vert->refCount = (hEdge->twin? 4 : 2);

    vert->index = numNormalVert;
    numNormalVert++;

    // Compute wall_tip info.
    BSP_CreateVertexEdgeTip(vert, -hEdge->pDX, -hEdge->pDY, hEdge, hEdge->twin);
    BSP_CreateVertexEdgeTip(vert, hEdge->pDX, hEdge->pDY, hEdge->twin, hEdge);
    return vert;
}

/**
 * Create a new half-edge.
 */
hedge_t *BSP_CreateHEdge(mlinedef_t *line, mlinedef_t *sourceLine,
                         mvertex_t *start, mvertex_t *end, msector_t *sec,
                         boolean back)
{
    hedge_t    *hEdge = allocHEdge();

    hEdge->v[0] = start;
    hEdge->v[1] = end;
    hEdge->linedef = line;
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
hedge_t *BSP_SplitHEdge(hedge_t *oldHEdge, double x, double y)
{
    hedge_t    *newHEdge;
    mvertex_t  *newVert;
/*
#if _DEBUG
if(oldHEdge->linedef)
    Con_Message("Splitting Linedef %d (%p) at (%1.1f,%1.1f)\n",
                oldHEdge->linedef->index, oldHEdge, x, y);
else
    Con_Message("Splitting MiniHEdge %p at (%1.1f,%1.1f)\n", oldHEdge, x, y);
#endif
*/
    // Update superblock, if needed.
    if(oldHEdge->block)
        BSP_IncSuperBlockHEdgeCounts(oldHEdge->block,
                                     (oldHEdge->linedef != NULL));

    newVert = newVertexFromSplitHEdge(oldHEdge, x, y);
    newHEdge = allocHEdge();

    // Copy seg info.
    memcpy(newHEdge, oldHEdge, sizeof(*newHEdge));
    newHEdge->next = NULL;

    newHEdge->prevOnSide = oldHEdge;
    oldHEdge->nextOnSide = newHEdge;

    oldHEdge->v[1] = newVert;
    updateHEdge(oldHEdge);

    newHEdge->v[0] = newVert;
    updateHEdge(newHEdge);

/*
#if _DEBUG
Con_Message("Splitting Vertex is %04X at (%1.1f,%1.1f)\n",
            newVert->index, newVert->V_pos[VX], newVert->V_pos[VY]);
#endif
*/
    // Handle the twin.
    if(oldHEdge->twin)
    {
/*
#if _DEBUG
Con_Message("Splitting hEdge->twin %p\n", oldHEdge->twin);
#endif
*/
        // Update superblock, if needed.
        if(oldHEdge->twin->block)
            BSP_IncSuperBlockHEdgeCounts(oldHEdge->twin->block,
                                         (oldHEdge->twin != NULL));

        newHEdge->twin = allocHEdge();

        // Copy seg info.
        memcpy(newHEdge->twin, oldHEdge->twin, sizeof(*newHEdge->twin));

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

void BSP_CreateVertexEdgeTip(mvertex_t *vert, double dx, double dy,
                             hedge_t *back, hedge_t *front)
{
    edgetip_t *tip = allocEdgeTip();
    edgetip_t *after;

    tip->angle = M_SlopeToAngle(dx, dy);
    tip->WT_edge[BACK]  = back;
    tip->WT_edge[FRONT] = front;

    // Find the correct place (order is increasing angle).
    for(after = vert->tipSet; after && after->next; after = after->next);

    while(after && tip->angle + ANG_EPSILON < after->angle)
        after = after->prev;

    // Link it in.
    if(after)
        tip->next = after->next;
    else
        tip->next = vert->tipSet;
    tip->prev = after;

    if(after)
    {
        if(after->next)
            after->next->prev = tip;

        after->next = tip;
    }
    else
    {
        if(vert->tipSet)
            vert->tipSet->prev = tip;

        vert->tipSet = tip;
    }
}

void BSP_CountEdgeTips(mvertex_t *vert, int *oneSided, int *twoSided)
{
    edgetip_t *tip;

    *oneSided = 0;
    *twoSided = 0;

    for(tip = vert->tipSet; tip; tip = tip->next)
    {
        if(!tip->WT_edge[BACK] || !tip->WT_edge[FRONT])
            (*oneSided) += 1;
        else
            (*twoSided) += 1;
    }
}

/**
 * Check whether a line with the given delta coordinates and beginning
 * at this vertex is open. Returns a sector reference if it's open,
 * or NULL if closed (void space or directly along a linedef).
 */
msector_t *BSP_VertexCheckOpen(mvertex_t *vert, double dX, double dY)
{
    edgetip_t  *tip;
    angle_g     angle = M_SlopeToAngle(dX, dY);

    // First check whether there's a wall_tip that lies in the exact
    // direction of the given direction (which is relative to the
    // vertex).
    for(tip = vert->tipSet; tip; tip = tip->next)
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
    for(tip = vert->tipSet; tip; tip = tip->next)
    {
        if(angle + ANG_EPSILON < tip->angle)
        {   // Found it.
            return (tip->WT_edge[FRONT]? tip->WT_edge[FRONT]->sector : NULL);
        }

        if(!tip->next)
        {
            // No more tips, thus we must be on the BACK side of the tip
            // with the largest angle.
            return (tip->WT_edge[BACK]? tip->WT_edge[BACK]->sector : NULL);
        }
    }

    Con_Error("Vertex %d has no tips !", vert->index);
    return NULL;
}

void BSP_FreeHEdges(void)
{
    int         i;

    for(i = 0; i < numHEdges; ++i)
        freeHEdge(levHEdges[i]);

    if(levHEdges)
        M_Free(levHEdges);

    levHEdges = NULL;
    numHEdges = 0;
}

void BSP_FreeEdgeTips(void)
{
    int         i;

    for(i = 0; i < numHEdgeTips; ++i)
        M_Free(hEdgeTips[i]);

    if(hEdgeTips)
        M_Free(hEdgeTips);

    hEdgeTips = NULL;
    numHEdgeTips = 0;
}

hedge_t *LookupHEdge(int index)
{
    if(index >= numHEdges)
        Con_Error("No such half-edge #%d", index);

    return levHEdges[index];
}

int BSP_GetNumHEdges(void)
{
    return numHEdges;
}

static int C_DECL hEdgeCompare(const void *p1, const void *p2)
{
    const hedge_t *a = ((const hedge_t **) p1)[0];
    const hedge_t *b = ((const hedge_t **) p2)[0];

    if(a->index < 0)
        Con_Error("Seg %p never reached a subsector !", a);

    if(b->index < 0)
        Con_Error("Seg %p never reached a subsector !", b);

    return (a->index - b->index);
}

void BSP_SortHEdgesByIndex(void)
{
    // Sort the half-edges into ascending index order.
    qsort(levHEdges, numHEdges, sizeof(hedge_t *), hEdgeCompare);
}

/**
 * Compute the parallel distance from a partition line to a point.
 */
double ParallelDist(hedge_t *part, double x, double y)
{
    return (x * part->pDX + y * part->pDY + part->pPara) / part->pLength;
}

/**
 * Compute the perpendicular distance from a partition line to a point.
 */
double PerpDist(hedge_t *part, double x, double y)
{
    return (x * part->pDY - y * part->pDX + part->pPerp) / part->pLength;
}
