/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * bsp_map.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_edit.h"
#include "de_refresh.h"

#include <stdlib.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void hardenSideSegList(gamemap_t* map, sidedef_t* side, seg_t* seg,
                              hedge_t* hEdge)
{
    uint                count;
    hedge_t*            first, *other;

    if(!side)
        return;

    // Have we already processed this side?
    if(side->segs)
        return;

    // Find the first seg.
    first = hEdge;
    while(first->prevOnSide)
        first = first->prevOnSide;

    // Count the segs for this side.
    count = 0;
    other = first;
    while(other)
    {
        other = other->nextOnSide;
        count++;
    }

    // Allocate the final side seg table.
    side->segCount = count;
    side->segs =
        Z_Malloc(sizeof(seg_t*) * (side->segCount + 1), PU_MAPSTATIC, 0);

    count = 0;
    other = first;
    while(other)
    {
        side->segs[count++] = &map->segs[other->index];
        other = other->nextOnSide;
    }
    side->segs[count] = NULL; // Terminate.
}

static int C_DECL hEdgeCompare(const void* p1, const void* p2)
{
    const hedge_t* a = ((const hedge_t**) p1)[0];
    const hedge_t* b = ((const hedge_t**) p2)[0];

    if(a->index == b->index)
        return 0;
    if(a->index < b->index)
        return -1;
    return 1;
}

typedef struct {
    size_t              curIdx;
    hedge_t***          indexPtr;
    boolean             write;
} hedgecollectorparams_t;

static boolean hEdgeCollector(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        hedgecollectorparams_t* params = (hedgecollectorparams_t*) data;
        bspleafdata_t*      leaf = (bspleafdata_t*) BinaryTree_GetData(tree);
        hedge_t*            hEdge;

        for(hEdge = leaf->hEdges; hEdge; hEdge = hEdge->next)
        {
            if(params->indexPtr)
            {   // Write mode.
                (*params->indexPtr)[params->curIdx++] = hEdge;
            }
            else
            {   // Count mode.
                if(hEdge->index == -1)
                    Con_Error("HEdge %p never reached a subsector!", hEdge);

                params->curIdx++;
            }
        }
    }

    return true; // Continue traversal.
}

static void buildSegsFromHEdges(gamemap_t* dest, binarytree_t* rootNode)
{
    uint                i;
    hedge_t**           index;
    hedgecollectorparams_t params;

    //
    // First we need to build a sorted index of the used hedges.
    //

    // Pass 1: Count the number of used hedges.
    params.curIdx = 0;
    params.indexPtr = NULL;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    if(!(params.curIdx > 0))
        Con_Error("buildSegsFromHEdges: No halfedges?");

    // Allocate the sort buffer.
    index = M_Malloc(sizeof(hedge_t*) * params.curIdx);

    // Pass 2: Collect ptrs the hedges and insert into the index.
    params.curIdx = 0;
    params.indexPtr = &index;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    // Sort the half-edges into ascending index order.
    qsort(index, params.curIdx, sizeof(hedge_t*), hEdgeCompare);

    dest->numSegs = (uint) params.curIdx;
    dest->segs = Z_Calloc(dest->numSegs * sizeof(seg_t), PU_MAPSTATIC, 0);
    for(i = 0; i < dest->numSegs; ++i)
    {
        seg_t*              seg = &dest->segs[i];
        hedge_t*            hEdge = index[i];

        seg->header.type = DMU_SEG;

        seg->SG_v1 = &dest->vertexes[hEdge->v[0]->buildData.index - 1];
        seg->SG_v2 = &dest->vertexes[hEdge->v[1]->buildData.index - 1];

        seg->side  = hEdge->side;
        if(hEdge->lineDef)
            seg->lineDef = &dest->lineDefs[hEdge->lineDef->buildData.index - 1];
        if(hEdge->twin)
            seg->backSeg = &dest->segs[hEdge->twin->index];

        seg->flags = 0;
        if(seg->lineDef)
        {
            linedef_t*          ldef = seg->lineDef;
            vertex_t*           vtx = seg->lineDef->L_v(seg->side);

            if(ldef->L_side(seg->side))
                seg->SG_frontsector = ldef->L_side(seg->side)->sector;

            if(ldef->L_frontside && ldef->L_backside)
            {
                seg->SG_backsector = ldef->L_side(seg->side ^ 1)->sector;
            }
            else
            {
                seg->SG_backsector = 0;
            }

            seg->offset = P_AccurateDistance(seg->SG_v1pos[VX] - vtx->V_pos[VX],
                                             seg->SG_v1pos[VY] - vtx->V_pos[VY]);
        }
        else
        {
            seg->lineDef = NULL;
            seg->SG_frontsector = NULL;
            seg->SG_backsector = NULL;
        }

        if(seg->lineDef)
            hardenSideSegList(dest, SEG_SIDEDEF(seg), seg, hEdge);

        seg->angle =
            bamsAtan2((int) (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]),
                      (int) (seg->SG_v2pos[VX] - seg->SG_v1pos[VX])) << FRACBITS;

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistance(seg->SG_v2pos[VX] - seg->SG_v1pos[VX],
                                         seg->SG_v2pos[VY] - seg->SG_v1pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->lineDef && SEG_SIDEDEF(seg))
        {
            sidedef_t*          side = SEG_SIDEDEF(seg);
            surface_t*          surface = &side->SW_topsurface;

            surface->normal[VY] = (seg->SG_v1pos[VX] - seg->SG_v2pos[VX]) / seg->length;
            surface->normal[VX] = (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }
    }

    // Free temporary storage
    M_Free(index);
}

static void hardenSSecSegList(gamemap_t* dest, subsector_t* ssec,
                              hedge_t* list, size_t segCount)
{
    size_t              i;
    hedge_t*            cur;
    seg_t**             segs;

    segs = Z_Malloc(sizeof(seg_t*) * (segCount + 1), PU_MAPSTATIC, 0);

    for(cur = list, i = 0; cur; cur = cur->next, ++i)
        segs[i] = &dest->segs[cur->index];
    segs[segCount] = NULL; // Terminate.

    if(i != segCount)
        Con_Error("hardenSSecSegList: Miscounted?");

    ssec->segs = segs;
}

static void hardenLeaf(gamemap_t* map, subsector_t* dest,
                       const bspleafdata_t* src)
{
    seg_t**             segp;
    boolean             found;
    size_t              hEdgeCount;
    hedge_t*            hEdge;

    hEdge = src->hEdges;
    hEdgeCount = 0;
    do
    {
        hEdgeCount++;
    } while((hEdge = hEdge->next) != NULL);

    dest->header.type = DMU_SUBSECTOR;
    dest->segCount = (uint) hEdgeCount;
    dest->shadows = NULL;
    dest->vertices = NULL;

    hardenSSecSegList(map, dest, src->hEdges, hEdgeCount);

    // Determine which sector this subsector belongs to.
    segp = dest->segs;
    found = false;
    while(*segp)
    {
        seg_t* seg = *segp;

        if(!found && seg->lineDef && SEG_SIDEDEF(seg))
        {
            sidedef_t*          side = SEG_SIDEDEF(seg);

            dest->sector = side->sector;
            found = true;
        }

        seg->subsector = dest;
        segp++;
    }

    if(!dest->sector)
    {
        Con_Message("hardenLeaf: Warning orphan subsector %p.\n", dest);
    }
}

typedef struct {
    gamemap_t*      dest;
    uint            ssecCurIndex;
    uint            nodeCurIndex;
} hardenbspparams_t;

static boolean C_DECL hardenNode(binarytree_t* tree, void* data)
{
    binarytree_t*       right, *left;
    bspnodedata_t*      nodeData;
    hardenbspparams_t*  params;
    node_t*             node;

    if(BinaryTree_IsLeaf(tree))
        return true; // Continue iteration.

    nodeData = BinaryTree_GetData(tree);
    params = (hardenbspparams_t*) data;

    node = &params->dest->nodes[nodeData->index = params->nodeCurIndex++];
    node->header.type = DMU_NODE;

    node->partition.x = nodeData->partition.x;
    node->partition.y = nodeData->partition.y;
    node->partition.dX = nodeData->partition.dX;
    node->partition.dY = nodeData->partition.dY;

    node->bBox[RIGHT][BOXTOP]    = nodeData->bBox[RIGHT][BOXTOP];
    node->bBox[RIGHT][BOXBOTTOM] = nodeData->bBox[RIGHT][BOXBOTTOM];
    node->bBox[RIGHT][BOXLEFT]   = nodeData->bBox[RIGHT][BOXLEFT];
    node->bBox[RIGHT][BOXRIGHT]  = nodeData->bBox[RIGHT][BOXRIGHT];

    node->bBox[LEFT][BOXTOP]     = nodeData->bBox[LEFT][BOXTOP];
    node->bBox[LEFT][BOXBOTTOM]  = nodeData->bBox[LEFT][BOXBOTTOM];
    node->bBox[LEFT][BOXLEFT]    = nodeData->bBox[LEFT][BOXLEFT];
    node->bBox[LEFT][BOXRIGHT]   = nodeData->bBox[LEFT][BOXRIGHT];

    right = BinaryTree_GetChild(tree, RIGHT);
    if(right)
    {
        if(BinaryTree_IsLeaf(right))
        {
            bspleafdata_t*  leaf = (bspleafdata_t*) BinaryTree_GetData(right);
            uint            idx = params->ssecCurIndex++;

            node->children[RIGHT] = idx | NF_SUBSECTOR;
            hardenLeaf(params->dest, &params->dest->ssectors[idx], leaf);
        }
        else
        {
            bspnodedata_t* data = (bspnodedata_t*) BinaryTree_GetData(right);

            node->children[RIGHT] = data->index;
        }
    }

    left = BinaryTree_GetChild(tree, LEFT);
    if(left)
    {
        if(BinaryTree_IsLeaf(left))
        {
            bspleafdata_t*  leaf = (bspleafdata_t*) BinaryTree_GetData(left);
            uint            idx = params->ssecCurIndex++;

            node->children[LEFT] = idx | NF_SUBSECTOR;
            hardenLeaf(params->dest, &params->dest->ssectors[idx], leaf);
        }
        else
        {
            bspnodedata_t*  data = (bspnodedata_t*) BinaryTree_GetData(left);

            node->children[LEFT]  = data->index;
        }
    }

    return true; // Continue iteration.
}

static boolean C_DECL countNode(binarytree_t* tree, void* data)
{
    if(!BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;

    return true; // Continue iteration.
}

static boolean C_DECL countSSec(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;

    return true; // Continue iteration.
}

static void hardenBSP(gamemap_t* dest, binarytree_t* rootNode)
{
    dest->numNodes = 0;
    BinaryTree_PostOrder(rootNode, countNode, &dest->numNodes);
    dest->nodes =
        Z_Calloc(dest->numNodes * sizeof(node_t), PU_MAPSTATIC, 0);

    dest->numSSectors = 0;
    BinaryTree_PostOrder(rootNode, countSSec, &dest->numSSectors);
    dest->ssectors =
        Z_Calloc(dest->numSSectors * sizeof(subsector_t), PU_MAPSTATIC, 0);

    if(rootNode)
    {
        hardenbspparams_t params;

        params.dest = dest;
        params.ssecCurIndex = 0;
        params.nodeCurIndex = 0;

        BinaryTree_PostOrder(rootNode, hardenNode, &params);
    }
}

void BSP_InitForNodeBuild(gamemap_t* map)
{
    uint                i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t*          l = &map->lineDefs[i];
        vertex_t*           start = l->v[0];
        vertex_t*           end   = l->v[1];

        start->buildData.refCount++;
        end->buildData.refCount++;

        l->buildData.mlFlags = 0;

        // Check for zero-length line.
        if((fabs(start->buildData.pos[VX] - end->buildData.pos[VX]) < DIST_EPSILON) &&
           (fabs(start->buildData.pos[VY] - end->buildData.pos[VY]) < DIST_EPSILON))
            l->buildData.mlFlags |= MLF_ZEROLENGTH;

        if(l->inFlags & LF_POLYOBJ)
            l->buildData.mlFlags |= MLF_POLYOBJ;

        if(l->sideDefs[BACK] && l->sideDefs[FRONT])
        {
            l->buildData.mlFlags |= MLF_TWOSIDED;

            if(l->sideDefs[BACK]->sector == l->sideDefs[FRONT]->sector)
                l->buildData.mlFlags |= MLF_SELFREF;
        }
    }
}

static void hardenVertexes(gamemap_t* dest, vertex_t*** vertexes,
                           uint* numVertexes)
{
    uint                i;

    dest->numVertexes = *numVertexes;
    dest->vertexes =
        Z_Calloc(dest->numVertexes * sizeof(vertex_t), PU_MAPSTATIC, 0);

    for(i = 0; i < dest->numVertexes; ++i)
    {
        vertex_t*           destV = &dest->vertexes[i];
        vertex_t*           srcV = (*vertexes)[i];

        destV->header.type = DMU_VERTEX;
        destV->numLineOwners = srcV->numLineOwners;
        destV->lineOwners = srcV->lineOwners;

        //// \fixme Add some rounding.
        destV->V_pos[VX] = (float) srcV->buildData.pos[VX];
        destV->V_pos[VY] = (float) srcV->buildData.pos[VY];
    }
}

static void updateVertexLinks(gamemap_t* dest)
{
    uint                i;

    for(i = 0; i < dest->numLineDefs; ++i)
    {
        linedef_t*          line = &dest->lineDefs[i];

        line->L_v1 = &dest->vertexes[line->L_v1->buildData.index - 1];
        line->L_v2 = &dest->vertexes[line->L_v2->buildData.index - 1];
    }
}

void SaveMap(gamemap_t* dest, void* rootNode, vertex_t*** vertexes,
             uint* numVertexes)
{
    uint                startTime = Sys_GetRealTime();
    binarytree_t*       rn = (binarytree_t*) rootNode;

    hardenVertexes(dest, vertexes, numVertexes);
    updateVertexLinks(dest);
    buildSegsFromHEdges(dest, rn);
    hardenBSP(dest, rn);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("SaveMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
