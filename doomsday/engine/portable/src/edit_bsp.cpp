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

#include <cstdlib>
#include <cmath>

#include "de_base.h"
#include "de_bsp.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_play.h"

#include <de/Log>
#include <BspBuilder>

using namespace de;

int bspFactor = 7;

struct bspbuilder_c_s {
   BspBuilder* inst;
};

void BspBuilder_Register(void)
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

BspBuilder_c* BspBuilder_New(GameMap* map)
{
    BspBuilder_c* builder = static_cast<BspBuilder_c*>(malloc(sizeof *builder));
    builder->inst = new BspBuilder(map);
    return builder;
}

void BspBuilder_Delete(BspBuilder_c* builder)
{
    Q_ASSERT(builder);
    delete builder->inst;
    free(builder);
}

BspBuilder_c* BspBuilder_SetSplitCostFactor(BspBuilder_c* builder, int factor)
{
    Q_ASSERT(builder);
    builder->inst->setSplitCostFactor(factor);
    return builder;
}

boolean BspBuilder_Build(BspBuilder_c* builder)
{
    Q_ASSERT(builder);
    return CPP_BOOL(builder->inst->build());
}

typedef struct {
    size_t curIdx;
    HEdge*** hedgeLUT;
} hedgecollectorparams_t;

static int hedgeCollector(BspBuilder::TreeNode& tree, void* parameters)
{
    if(tree.isLeaf())
    {
        hedgecollectorparams_t* p = static_cast<hedgecollectorparams_t*>(parameters);
        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
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

static void buildHEdgeLut(GameMap* map, BspBuilder::TreeNode* rootNode)
{
    Q_ASSERT(map);

    if(map->hedges)
    {
        Z_Free(map->hedges);
        map->hedges = 0;
    }

    // Count the number of used hedges.
    hedgecollectorparams_t parm;
    parm.curIdx = 0;
    parm.hedgeLUT = 0;
    if(rootNode)
    {
        BspBuilder::TreeNode::InOrder(*rootNode, hedgeCollector, &parm);
    }
    map->numHEdges = parm.curIdx;

    if(!map->numHEdges) return; // Should never happen.

    // Allocate the HEdge LUT and collect pointers.
    map->hedges = (HEdge**)Z_Calloc(map->numHEdges * sizeof(HEdge*), PU_MAPSTATIC, 0);
    parm.curIdx = 0;
    parm.hedgeLUT = &map->hedges;
    BspBuilder::TreeNode::InOrder(*rootNode, hedgeCollector, &parm);
}

static void finishHEdges(GameMap* map)
{
    Q_ASSERT(map);

    for(uint i = 0; i < map->numHEdges; ++i)
    {
        HEdge* hedge = map->hedges[i];

        if(hedge->lineDef)
        {
            LineDef* ldef = hedge->lineDef;
            Vertex* vtx = hedge->lineDef->L_v(hedge->side);

            if(ldef->L_side(hedge->side))
                hedge->sector = ldef->L_side(hedge->side)->sector;

            hedge->offset = P_AccurateDistance(hedge->HE_v1pos[VX] - vtx->V_pos[VX],
                                               hedge->HE_v1pos[VY] - vtx->V_pos[VY]);
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
} populatebspobjectluts_params_t;

static int populateBspObjectLuts(BspBuilder::TreeNode& tree, void* parameters)
{
    populatebspobjectluts_params_t* p = static_cast<populatebspobjectluts_params_t*>(parameters);
    Q_ASSERT(p);

    // We are only interested in BspNodes at this level.
    if(tree.isLeaf()) return false; // Continue iteration.

    // Take ownership of this BspNode.
    BspNode* node = reinterpret_cast<BspNode*>(tree.userData());
    tree.setUserData(NULL);

    // Add this BspNode to the LUT.
    p->dest->bspNodes[p->nodeCurIndex++] = node;

    if(BspBuilder::TreeNode* right = tree.right())
    {
        if(right->isLeaf())
        {
            // Take ownership of this BspLeaf.
            BspLeaf* leaf = reinterpret_cast<BspLeaf*>(right->userData());
            right->setUserData(NULL);

            // Add this BspLeaf to the LUT.
            p->dest->bspLeafs[p->leafCurIndex++] = leaf;
        }
    }

    if(BspBuilder::TreeNode* left = tree.left())
    {
        if(left->isLeaf())
        {
            // Take ownership of this BspLeaf.
            BspLeaf* leaf = reinterpret_cast<BspLeaf*>(left->userData());
            left->setUserData(NULL);

            // Add this BspLeaf to the LUT.
            p->dest->bspLeafs[p->leafCurIndex++] = leaf;
        }
    }

    return false; // Continue iteration.
}

static int countNode(BspBuilder::TreeNode& tree, void* data)
{
    if(!tree.isLeaf())
        (*((uint*) data))++;
    return false; // Continue iteration.
}

static int countLeaf(BspBuilder::TreeNode& tree, void* data)
{
    if(tree.isLeaf())
        (*((uint*) data))++;
    return false; // Continue iteration.
}

static void hardenBSP(GameMap* dest, BspBuilder::TreeNode* rootNode)
{
    dest->numBspNodes = 0;
    BspBuilder::TreeNode::PostOrder(*rootNode, countNode, &dest->numBspNodes);
    if(dest->numBspNodes != 0)
        dest->bspNodes = static_cast<BspNode**>(Z_Malloc(dest->numBspNodes * sizeof(BspNode*), PU_MAPSTATIC, 0));
    else
        dest->bspNodes = 0;

    dest->numBspLeafs = 0;
    BspBuilder::TreeNode::PostOrder(*rootNode, countLeaf, &dest->numBspLeafs);
    dest->bspLeafs = static_cast<BspLeaf**>(Z_Calloc(dest->numBspLeafs * sizeof(BspLeaf*), PU_MAPSTATIC, 0));

    if(rootNode->isLeaf())
    {
        // Take ownership of this leaf.
        dest->bsp = rootNode->userData();
        rootNode->setUserData(NULL);
        return;
    }

    dest->bsp = rootNode->userData();

    populatebspobjectluts_params_t p;
    p.dest = dest;
    p.leafCurIndex = 0;
    p.nodeCurIndex = 0;
    BspBuilder::TreeNode::PostOrder(*rootNode, populateBspObjectLuts, &p);
}

static void hardenVertexes(GameMap* dest, Vertex*** vertexes, uint* numVertexes)
{
    dest->numVertexes = *numVertexes;
    dest->vertexes = static_cast<Vertex*>(Z_Calloc(dest->numVertexes * sizeof(Vertex), PU_MAPSTATIC, 0));

    for(uint i = 0; i < dest->numVertexes; ++i)
    {
        Vertex* destV = &dest->vertexes[i];
        Vertex* srcV = (*vertexes)[i];

        destV->header.type = DMU_VERTEX;
        destV->numLineOwners = srcV->numLineOwners;
        destV->lineOwners = srcV->lineOwners;

        destV->V_pos[VX] = float(srcV->buildData.pos[VX]);
        destV->V_pos[VY] = float(srcV->buildData.pos[VY]);
    }
}

static void updateVertexLinks(GameMap* map)
{
    for(uint i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* line = &map->lineDefs[i];

        line->L_v1 = &map->vertexes[line->L_v1->buildData.index - 1];
        line->L_v2 = &map->vertexes[line->L_v2->buildData.index - 1];
    }

    for(uint i = 0; i < map->numHEdges; ++i)
    {
        HEdge* hedge = map->hedges[i];

        hedge->HE_v1 = &map->vertexes[hedge->v[0]->buildData.index - 1];
        hedge->HE_v2 = &map->vertexes[hedge->v[1]->buildData.index - 1];
    }
}

void MPE_SaveBsp(BspBuilder_c* builder, GameMap* map, Vertex*** vertexes, uint* numVertexes)
{
    Q_ASSERT(builder);

    BspBuilder::TreeNode* rootNode = builder->inst->root();

    buildHEdgeLut(map, rootNode);
    hardenVertexes(map, vertexes, numVertexes);
    updateVertexLinks(map);

    finishHEdges(map);
    hardenBSP(map, rootNode);

    long rHeight = 0, lHeight = 0;
    if(rootNode && !rootNode->isLeaf())
    {
        if(rootNode->right())
            rHeight = long(rootNode->right()->height());
        else
            rHeight = 0;

        if(rootNode->left())
            lHeight = long(rootNode->left()->height());
        else
            lHeight = 0;
    }

    LOG_INFO("BSP built: %d Nodes, %d Leafs, %d HEdges, %d Vertexes\n Balance %d (l%d - r%d).")
            << map->numBspNodes << map->numBspLeafs << map->numHEdges << map->numVertexes
            << lHeight - rHeight << lHeight << rHeight;
}
