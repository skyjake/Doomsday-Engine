/**
 * @file bsp_map.c
 * BSP Builder. @ingroup map
 *
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

#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_bsp.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_edit.h"
#include "de_refresh.h"

static void hardenSidedefHEdgeList(GameMap* map, SideDef* side, HEdge* hedge,
    bsp_hedge_t* bspHEdge)
{
    bsp_hedge_t* first, *other;
    uint count;

    if(!side) return;

    // Have we already processed this side?
    if(side->hedges) return;

    // Find the first hedge.
    first = bspHEdge;
    while(first->prevOnSide)
        first = first->prevOnSide;

    // Count the hedges for this side.
    count = 0;
    other = first;
    while(other)
    {
        other = other->nextOnSide;
        count++;
    }

    // Allocate the final side hedge table.
    side->hedgeCount = count;
    side->hedges = Z_Malloc(sizeof(HEdge*) * (side->hedgeCount + 1), PU_MAPSTATIC, 0);

    count = 0;
    other = first;
    while(other)
    {
        side->hedges[count++] = &map->hedges[other->index];
        other = other->nextOnSide;
    }
    side->hedges[count] = NULL; // Terminate.
}

static int C_DECL hEdgeCompare(const void* p1, const void* p2)
{
    const bsp_hedge_t* a = ((const bsp_hedge_t**) p1)[0];
    const bsp_hedge_t* b = ((const bsp_hedge_t**) p2)[0];

    if(a->index == b->index) return 0;
    if(a->index < b->index) return -1;
    return 1;
}

typedef struct {
    size_t curIdx;
    bsp_hedge_t*** indexPtr;
    boolean write;
} hedgecollectorparams_t;

static boolean hEdgeCollector(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        hedgecollectorparams_t* params = (hedgecollectorparams_t*) data;
        bspleafdata_t* leaf = (bspleafdata_t*) BinaryTree_GetData(tree);
        bsp_hedge_t* hEdge;

        for(hEdge = leaf->hEdges; hEdge; hEdge = hEdge->next)
        {
            if(params->indexPtr)
            {
                // Write mode.
                (*params->indexPtr)[params->curIdx++] = hEdge;
            }
            else
            {
                // Count mode.
                if(hEdge->index == -1)
                    Con_Error("HEdge %p never reached a BspLeaf!", hEdge);

                params->curIdx++;
            }
        }
    }

    return true; // Continue traversal.
}

static void buildHEdgesFromBSPHEdges(GameMap* dest, binarytree_t* rootNode)
{
    uint i;
    bsp_hedge_t** index;
    hedgecollectorparams_t params;

    //
    // First we need to build a sorted index of the used hedges.
    //

    // Pass 1: Count the number of used hedges.
    params.curIdx = 0;
    params.indexPtr = NULL;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    if(!(params.curIdx > 0))
        Con_Error("buildHEdgesFromBSPHEdges: No hedges?");

    // Allocate the sort buffer.
    index = M_Malloc(sizeof(bsp_hedge_t*) * params.curIdx);

    // Pass 2: Collect ptrs the hedges and insert into the index.
    params.curIdx = 0;
    params.indexPtr = &index;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    // Sort the half-edges into ascending index order.
    qsort(index, params.curIdx, sizeof(bsp_hedge_t*), hEdgeCompare);

    dest->numHEdges = (uint) params.curIdx;
    dest->hedges = Z_Calloc(dest->numHEdges * sizeof(HEdge), PU_MAPSTATIC, 0);
    for(i = 0; i < dest->numHEdges; ++i)
    {
        HEdge* hedge = &dest->hedges[i];
        bsp_hedge_t* bspHEdge = index[i];

        hedge->header.type = DMU_HEDGE;

        hedge->HE_v1 = &dest->vertexes[bspHEdge->v[0]->buildData.index - 1];
        hedge->HE_v2 = &dest->vertexes[bspHEdge->v[1]->buildData.index - 1];

        hedge->side  = bspHEdge->side;
        if(bspHEdge->lineDef)
            hedge->lineDef = &dest->lineDefs[bspHEdge->lineDef->buildData.index - 1];
        if(bspHEdge->twin)
            hedge->twin = &dest->hedges[bspHEdge->twin->index];

        hedge->flags = 0;
        if(hedge->lineDef)
        {
            LineDef* ldef = hedge->lineDef;
            Vertex* vtx = hedge->lineDef->L_v(hedge->side);

            if(ldef->L_side(hedge->side))
                hedge->HE_frontsector = ldef->L_side(hedge->side)->sector;

            if(ldef->L_frontside && ldef->L_backside)
            {
                hedge->HE_backsector = ldef->L_side(hedge->side ^ 1)->sector;
            }
            else
            {
                hedge->HE_backsector = 0;
            }

            hedge->offset = P_AccurateDistance(hedge->HE_v1pos[VX] - vtx->V_pos[VX],
                                               hedge->HE_v1pos[VY] - vtx->V_pos[VY]);
        }
        else
        {
            hedge->lineDef = NULL;
            hedge->HE_frontsector = NULL;
            hedge->HE_backsector = NULL;
        }

        if(hedge->lineDef)
            hardenSidedefHEdgeList(dest, HEDGE_SIDEDEF(hedge), hedge, bspHEdge);

        hedge->angle = bamsAtan2((int) (hedge->HE_v2pos[VY] - hedge->HE_v1pos[VY]),
                                 (int) (hedge->HE_v2pos[VX] - hedge->HE_v1pos[VX])) << FRACBITS;

        // Calculate the length of the segment.
        hedge->length = P_AccurateDistance(hedge->HE_v2pos[VX] - hedge->HE_v1pos[VX],
                                           hedge->HE_v2pos[VY] - hedge->HE_v1pos[VY]);

        if(hedge->length == 0)
            hedge->length = 0.01f; // Hmm...

        // Calculate the tangent space surface vectors.
        if(hedge->lineDef && HEDGE_SIDEDEF(hedge))
        {
            SideDef* side = HEDGE_SIDEDEF(hedge);
            Surface* surface = &side->SW_topsurface;

            surface->normal[VY] = (hedge->HE_v1pos[VX] - hedge->HE_v2pos[VX]) / hedge->length;
            surface->normal[VX] = (hedge->HE_v2pos[VY] - hedge->HE_v1pos[VY]) / hedge->length;
            surface->normal[VZ] = 0;
            V3_BuildTangents(surface->tangent, surface->bitangent, surface->normal);

            // All surfaces of a sidedef have the same tangent space vectors.
            memcpy(side->SW_middletangent, surface->tangent, sizeof(surface->tangent));
            memcpy(side->SW_middlebitangent, surface->bitangent, sizeof(surface->bitangent));
            memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));

            memcpy(side->SW_bottomtangent, surface->tangent, sizeof(surface->tangent));
            memcpy(side->SW_bottombitangent, surface->bitangent, sizeof(surface->bitangent));
            memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }
    }

    // Free temporary storage
    M_Free(index);
}

static void hardenBspLeafHEdgeList(GameMap* dest, BspLeaf* bspLeaf, bsp_hedge_t* list,
    size_t hedgeCount)
{
    size_t i;
    bsp_hedge_t* cur;
    HEdge** hedges;

    hedges = Z_Malloc(sizeof(HEdge*) * (hedgeCount + 1), PU_MAPSTATIC, 0);

    for(cur = list, i = 0; cur; cur = cur->next, ++i)
        hedges[i] = &dest->hedges[cur->index];
    hedges[hedgeCount] = NULL; // Terminate.

    if(i != hedgeCount)
        Con_Error("hardenBspLeafHEdgeList: Miscounted?");

    bspLeaf->hedges = hedges;
}

static void hardenLeaf(GameMap* map, BspLeaf* dest, const bspleafdata_t* src)
{
    HEdge** segp;
    boolean found;
    size_t hEdgeCount;
    bsp_hedge_t* hEdge;

    hEdge = src->hEdges;
    hEdgeCount = 0;
    do
    {
        hEdgeCount++;
    } while((hEdge = hEdge->next) != NULL);

    dest->header.type = DMU_BSPLEAF;
    dest->hedgeCount = (uint) hEdgeCount;
    dest->shadows = NULL;
    dest->vertices = NULL;

    hardenBspLeafHEdgeList(map, dest, src->hEdges, hEdgeCount);

    // Determine which sector this BSP leaf belongs to.
    segp = dest->hedges;
    found = false;
    while(*segp)
    {
        HEdge* hedge = *segp;
        if(!found && hedge->lineDef && HEDGE_SIDEDEF(hedge))
        {
            SideDef* side = HEDGE_SIDEDEF(hedge);
            dest->sector = side->sector;
            found = true;
        }
        hedge->bspLeaf = dest;
        segp++;
    }

    if(!dest->sector)
    {
        Con_Message("hardenLeaf: Warning orphan BSP leaf %p.\n", dest);
    }
}

typedef struct {
    GameMap* dest;
    uint leafCurIndex;
    uint nodeCurIndex;
} hardenbspparams_t;

static boolean C_DECL hardenNode(binarytree_t* tree, void* data)
{
    binarytree_t* right, *left;
    BspNode* nodeData;
    hardenbspparams_t* params;
    BspNode* node;

    if(BinaryTree_IsLeaf(tree))
        return true; // Continue iteration.

    nodeData = BinaryTree_GetData(tree);
    params = (hardenbspparams_t*) data;

    node = &params->dest->bspNodes[nodeData->buildData.index = params->nodeCurIndex++];
    node->header.type = DMU_BSPNODE;

    node->partition.x = nodeData->partition.x;
    node->partition.y = nodeData->partition.y;
    node->partition.dX = nodeData->partition.dX;
    node->partition.dY = nodeData->partition.dY;

    memcpy(&node->aaBox[RIGHT], &nodeData->aaBox[RIGHT], sizeof(node->aaBox[RIGHT]));
    memcpy(&node->aaBox[LEFT],  &nodeData->aaBox[LEFT],  sizeof(node->aaBox[LEFT]));

    right = BinaryTree_GetChild(tree, RIGHT);
    if(right)
    {
        if(BinaryTree_IsLeaf(right))
        {
            bspleafdata_t* leaf = (bspleafdata_t*) BinaryTree_GetData(right);
            uint idx = params->leafCurIndex++;

            node->children[RIGHT] = idx | NF_LEAF;
            hardenLeaf(params->dest, &params->dest->bspLeafs[idx], leaf);
        }
        else
        {
            BspNode* data = (BspNode*) BinaryTree_GetData(right);

            node->children[RIGHT] = data->buildData.index;
        }
    }

    left = BinaryTree_GetChild(tree, LEFT);
    if(left)
    {
        if(BinaryTree_IsLeaf(left))
        {
            bspleafdata_t* leaf = (bspleafdata_t*) BinaryTree_GetData(left);
            uint idx = params->leafCurIndex++;

            node->children[LEFT] = idx | NF_LEAF;
            hardenLeaf(params->dest, &params->dest->bspLeafs[idx], leaf);
        }
        else
        {
            BspNode* data = (BspNode*) BinaryTree_GetData(left);

            node->children[LEFT]  = data->buildData.index;
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

static void hardenBSP(GameMap* dest, binarytree_t* rootNode)
{
    dest->numBspNodes = 0;
    BinaryTree_PostOrder(rootNode, countNode, &dest->numBspNodes);
    if(dest->numBspNodes != 0)
        dest->bspNodes = Z_Calloc(dest->numBspNodes * sizeof(BspNode), PU_MAPSTATIC, 0);
    else
        dest->bspNodes = 0;

    dest->numBspLeafs = 0;
    BinaryTree_PostOrder(rootNode, countSSec, &dest->numBspLeafs);
    dest->bspLeafs = Z_Calloc(dest->numBspLeafs * sizeof(BspLeaf), PU_MAPSTATIC, 0);

    if(!rootNode) return;

    if(BinaryTree_IsLeaf(rootNode))
    {
        hardenLeaf(dest, &dest->bspLeafs[0], (bspleafdata_t*) BinaryTree_GetData(rootNode));
        return;
    }

    { hardenbspparams_t p;
    p.dest = dest;
    p.leafCurIndex = 0;
    p.nodeCurIndex = 0;
    BinaryTree_PostOrder(rootNode, hardenNode, &p);
    }
}

void BSP_InitForNodeBuild(GameMap* map)
{
    uint i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* l = &map->lineDefs[i];
        Vertex* start = l->v[0];
        Vertex* end   = l->v[1];

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

static void hardenVertexes(GameMap* dest, Vertex*** vertexes, uint* numVertexes)
{
    uint i;

    dest->numVertexes = *numVertexes;
    dest->vertexes = Z_Calloc(dest->numVertexes * sizeof(Vertex), PU_MAPSTATIC, 0);

    for(i = 0; i < dest->numVertexes; ++i)
    {
        Vertex* destV = &dest->vertexes[i];
        Vertex* srcV = (*vertexes)[i];

        destV->header.type = DMU_VERTEX;
        destV->numLineOwners = srcV->numLineOwners;
        destV->lineOwners = srcV->lineOwners;

        destV->V_pos[VX] = (float) srcV->buildData.pos[VX];
        destV->V_pos[VY] = (float) srcV->buildData.pos[VY];
    }
}

static void updateVertexLinks(GameMap* dest)
{
    uint i;

    for(i = 0; i < dest->numLineDefs; ++i)
    {
        LineDef* line = &dest->lineDefs[i];

        line->L_v1 = &dest->vertexes[line->L_v1->buildData.index - 1];
        line->L_v2 = &dest->vertexes[line->L_v2->buildData.index - 1];
    }
}

void SaveMap(GameMap* dest, void* rootNode, Vertex*** vertexes, uint* numVertexes)
{
    uint startTime = Sys_GetRealTime();
    binarytree_t* rn = (binarytree_t*) rootNode;

    hardenVertexes(dest, vertexes, numVertexes);
    updateVertexLinks(dest);
    buildHEdgesFromBSPHEdges(dest, rn);
    hardenBSP(dest, rn);

    // How much time did we spend?
    VERBOSE( Con_Message("SaveMap: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}
