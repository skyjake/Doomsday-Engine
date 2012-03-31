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

#include "bspbuilder/hedges.hh" /// @todo Remove me.

static void hardenSidedefHEdgeList(GameMap* map, SideDef* side, HEdge* bspHEdge)
{
    HEdge* first, *other;
    uint count;

    if(!side) return;

    // Have we already processed this side?
    if(side->hedges) return;

    // Find the first hedge.
    first = bspHEdge;
    while(first->buildData.prevOnSide)
        first = first->buildData.prevOnSide;

    // Count the hedges for this side.
    count = 0;
    other = first;
    while(other)
    {
        other = other->buildData.nextOnSide;
        count++;
    }

    // Allocate the final side hedge table.
    side->hedgeCount = count;
    side->hedges = (HEdge**)Z_Malloc(sizeof(*side->hedges) * (side->hedgeCount + 1), PU_MAPSTATIC, 0);

    count = 0;
    other = first;
    while(other)
    {
        side->hedges[count++] = other;
        other = other->buildData.nextOnSide;
    }
    side->hedges[count] = NULL; // Terminate.
}

typedef struct {
    size_t curIdx;
    HEdge*** hedgeLUT;
} hedgecollectorparams_t;

static int hedgeCollector(BinaryTree* tree, void* parameters)
{
    if(BinaryTree_IsLeaf(tree))
    {
        hedgecollectorparams_t* p = (hedgecollectorparams_t*) parameters;
        BspLeaf* leaf = (BspLeaf*) BinaryTree_UserData(tree);
        HEdge* hedge = leaf->hedge;
        do
        {
            if(p->hedgeLUT)
            {
                // Write mode.
                (*p->hedgeLUT)[p->curIdx++] = hedge;
            }
            else
            {
                // Count mode.
                p->curIdx++;
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }
    return false; // Continue traversal.
}

static void buildHEdgeLut(GameMap* map, BinaryTree* rootNode)
{
    hedgecollectorparams_t parm;

    if(map->hedges)
    {
        Z_Free(map->hedges);
        map->hedges = 0;
    }

    // Count the number of used hedges.
    parm.curIdx = 0;
    parm.hedgeLUT = 0;
    BinaryTree_InOrder(rootNode, hedgeCollector, &parm);
    map->numHEdges = parm.curIdx;

    if(!map->numHEdges) return; // Should never happen.

    // Allocate the HEdge LUT and collect pointers.
    map->hedges = (HEdge**)Z_Calloc(map->numHEdges * sizeof(HEdge*), PU_MAPSTATIC, 0);
    parm.curIdx = 0;
    parm.hedgeLUT = &map->hedges;
    BinaryTree_InOrder(rootNode, hedgeCollector, &parm);
}

static void finishHEdges(GameMap* map)
{
    uint i;
    assert(map);

    for(i = 0; i < map->numHEdges; ++i)
    {
        HEdge* hedge = map->hedges[i];

        if(hedge->buildData.lineDef)
            hedge->lineDef = &map->lineDefs[hedge->buildData.lineDef->buildData.index - 1];

        if(hedge->lineDef)
        {
            LineDef* ldef = hedge->lineDef;
            Vertex* vtx = hedge->lineDef->L_v(hedge->side);

            if(ldef->L_side(hedge->side))
                hedge->sector = ldef->L_side(hedge->side)->sector;

            hedge->offset = P_AccurateDistance(hedge->HE_v1pos[VX] - vtx->V_pos[VX],
                                               hedge->HE_v1pos[VY] - vtx->V_pos[VY]);

            hardenSidedefHEdgeList(map, HEDGE_SIDEDEF(hedge), hedge);
        }

        hedge->angle = bamsAtan2((int) (hedge->HE_v2pos[VY] - hedge->HE_v1pos[VY]),
                                 (int) (hedge->HE_v2pos[VX] - hedge->HE_v1pos[VX])) << FRACBITS;

        // Calculate the length of the segment.
        hedge->length = P_AccurateDistance(hedge->HE_v2pos[VX] - hedge->HE_v1pos[VX],
                                           hedge->HE_v2pos[VY] - hedge->HE_v1pos[VY]);

        if(hedge->length == 0)
            hedge->length = 0.01f; // Hmm...
    }
}

typedef struct {
    GameMap* dest;
    uint leafCurIndex;
    uint nodeCurIndex;
} hardenbspparams_t;

static int C_DECL hardenNode(BinaryTree* tree, void* data)
{
    BinaryTree* right, *left;
    hardenbspparams_t* params;
    BspNode* node;

    if(BinaryTree_IsLeaf(tree))
        return false; // Continue iteration.

    node =(BspNode*)BinaryTree_UserData(tree);
    params = (hardenbspparams_t*) data;

    // Add this to BspNode LUT.
    params->dest->bspNodes[params->nodeCurIndex++] = node;

    right = BinaryTree_Child(tree, RIGHT);
    if(right)
    {
        if(BinaryTree_IsLeaf(right))
        {
            BspLeaf* leaf = (BspLeaf*) BinaryTree_UserData(right);

            node->children[RIGHT] = (runtime_mapdata_header_t*)leaf;
            params->dest->bspLeafs[params->leafCurIndex++] = leaf;
        }
        else
        {
            BspNode* data = (BspNode*) BinaryTree_UserData(right);
            node->children[RIGHT] = (runtime_mapdata_header_t*)data;
        }
    }

    left = BinaryTree_Child(tree, LEFT);
    if(left)
    {
        if(BinaryTree_IsLeaf(left))
        {
            BspLeaf* leaf = (BspLeaf*) BinaryTree_UserData(left);

            node->children[LEFT] = (runtime_mapdata_header_t*)leaf;
            params->dest->bspLeafs[params->leafCurIndex++] = leaf;
        }
        else
        {
            BspNode* data = (BspNode*) BinaryTree_UserData(left);
            node->children[LEFT] = (runtime_mapdata_header_t*)data;
        }
    }

    return false; // Continue iteration.
}

static int C_DECL countNode(BinaryTree* tree, void* data)
{
    if(!BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;
    return false; // Continue iteration.
}

static int C_DECL countLeaf(BinaryTree* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;
    return false; // Continue iteration.
}

static void hardenBSP(GameMap* dest, BinaryTree* rootNode)
{
    dest->numBspNodes = 0;
    BinaryTree_PostOrder(rootNode, countNode, &dest->numBspNodes);
    if(dest->numBspNodes != 0)
        dest->bspNodes = (BspNode**)Z_Malloc(dest->numBspNodes * sizeof(BspNode*), PU_MAPSTATIC, 0);
    else
        dest->bspNodes = 0;

    dest->numBspLeafs = 0;
    BinaryTree_PostOrder(rootNode, countLeaf, &dest->numBspLeafs);
    dest->bspLeafs = (BspLeaf**)Z_Calloc(dest->numBspLeafs * sizeof(BspLeaf*), PU_MAPSTATIC, 0);

    if(!rootNode) return;

    if(BinaryTree_IsLeaf(rootNode))
    {
        BspLeaf* leaf = (BspLeaf*) BinaryTree_UserData(rootNode);
        dest->bsp = (runtime_mapdata_header_t*)leaf;
        return;
    }

    dest->bsp = (runtime_mapdata_header_t*)BinaryTree_UserData(rootNode);

    { hardenbspparams_t p;
    p.dest = dest;
    p.leafCurIndex = 0;
    p.nodeCurIndex = 0;
    BinaryTree_PostOrder(rootNode, hardenNode, &p);
    }
}

static void hardenVertexes(GameMap* dest, Vertex*** vertexes, uint* numVertexes)
{
    uint i;

    dest->numVertexes = *numVertexes;
    dest->vertexes = (Vertex*)Z_Calloc(dest->numVertexes * sizeof(Vertex), PU_MAPSTATIC, 0);

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

static void updateVertexLinks(GameMap* map)
{
    uint i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* line = &map->lineDefs[i];

        line->L_v1 = &map->vertexes[line->L_v1->buildData.index - 1];
        line->L_v2 = &map->vertexes[line->L_v2->buildData.index - 1];
    }

    for(i = 0; i < map->numHEdges; ++i)
    {
        HEdge* hedge = map->hedges[i];

        hedge->HE_v1 = &map->vertexes[hedge->v[0]->buildData.index - 1];
        hedge->HE_v2 = &map->vertexes[hedge->v[1]->buildData.index - 1];
    }
}

void BspBuilder_Save(GameMap* dest, void* rootNode, Vertex*** vertexes, uint* numVertexes)
{
    uint startTime = Sys_GetRealTime();
    BinaryTree* rn = (BinaryTree*) rootNode;

    buildHEdgeLut(dest, rn);
    hardenVertexes(dest, vertexes, numVertexes);
    updateVertexLinks(dest);

    finishHEdges(dest);
    hardenBSP(dest, rn);

    // How much time did we spend?
    VERBOSE( Con_Message("BspBuilder_Save: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}
