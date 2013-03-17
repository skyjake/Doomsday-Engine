/** @file edit_bsp.cpp BSP Builder. 
 * @ingroup map
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

BspBuilder_c* BspBuilder_New(GameMap* map, uint numEditableVertexes, Vertex const **editableVertexes)
{
    DENG2_ASSERT(map);
    BspBuilder_c* builder = static_cast<BspBuilder_c*>(malloc(sizeof *builder));
    builder->inst = new BspBuilder(*map, numEditableVertexes, editableVertexes);
    return builder;
}

void BspBuilder_Delete(BspBuilder_c* builder)
{
    DENG2_ASSERT(builder);
    delete builder->inst;
    free(builder);
}

BspBuilder_c* BspBuilder_SetSplitCostFactor(BspBuilder_c* builder, int factor)
{
    DENG2_ASSERT(builder);
    builder->inst->setSplitCostFactor(factor);
    return builder;
}

boolean BspBuilder_Build(BspBuilder_c* builder)
{
    DENG2_ASSERT(builder);
    return CPP_BOOL(builder->inst->build());
}

typedef struct {
    GameMap *map;
    BspBuilder *builder;
} HEdgeCollectParms;

static int hedgeCollector(BspTreeNode &tree, void *parameters)
{
    if(tree.isLeaf())
    {
        HEdgeCollectParms* p = static_cast<HEdgeCollectParms*>(parameters);
        BspLeaf *leaf = tree.userData()->castTo<BspLeaf>();
        HEdge *base = leaf->firstHEdge();
        HEdge *hedge = base;
        do
        {
            // Take ownership of this HEdge.
            p->builder->take(hedge);

            // Add this HEdge to the LUT.
            hedge->_origIndex = p->map->hedges.count();
            p->map->hedges.append(hedge);

            if(hedge->hasLine())
            {
                Vertex const &vtx = hedge->line().vertex(hedge->lineSideId());

                hedge->_sector = hedge->line().sectorPtr(hedge->lineSideId());
                hedge->_lineOffset = V2d_Distance(hedge->v1Origin(), vtx.origin());
            }

            hedge->_angle = bamsAtan2(int( hedge->v2Origin()[VY] - hedge->v1Origin()[VY] ),
                                      int( hedge->v2Origin()[VX] - hedge->v1Origin()[VX] )) << FRACBITS;

            // Calculate the length of the segment.
            hedge->_length = V2d_Distance(hedge->v2Origin(), hedge->v1Origin());

            if(hedge->_length == 0)
                hedge->_length = 0.01f; // Hmm...

        } while((hedge = &hedge->next()) != base);
    }
    return false; // Continue traversal.
}

static void collateHEdges(BspBuilder &builder, GameMap *map)
{
    DENG2_ASSERT(map);
    DENG2_ASSERT(map->hedges.isEmpty());

    if(!builder.numHEdges()) return; // Should never happen.
#ifdef DENG2_QT_4_7_OR_NEWER
    map->hedges.reserve(builder.numHEdges());
#endif

    HEdgeCollectParms parms;
    parms.builder = &builder;
    parms.map     = map;
    builder.root()->traverseInOrder(hedgeCollector, &parms);
}

static void collateVertexes(BspBuilder &builder, GameMap *map,
    uint *numEditableVertexes, Vertex const ***editableVertexes)
{
    uint bspVertexCount = builder.numVertexes();

    DENG2_ASSERT(map->vertexes.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->vertexes.reserve(*numEditableVertexes + bspVertexCount);
#endif

    uint n = 0;
    for(; n < *numEditableVertexes; ++n)
    {
        Vertex *vtx = const_cast<Vertex *>((*editableVertexes)[n]);
        map->vertexes.append(vtx);
    }

    for(uint i = 0; i < bspVertexCount; ++i, ++n)
    {
        Vertex &vtx  = builder.vertex(i);

        builder.take(&vtx);
        map->vertexes.append(&vtx);
    }
}

struct CollateBspElementsParms
{
    BspBuilder *builder;
    GameMap *map;
};

static void collateBspElements(BspTreeNode &tree, CollateBspElementsParms const &parms)
{
    // We are only interested in BspNodes at this level.
    if(tree.isLeaf()) return; // Continue iteration.

    // Take ownership of this BspNode.
    DENG2_ASSERT(tree.userData());
    BspNode *node = tree.userData()->castTo<BspNode>();
    parms.builder->take(node);

    // Add this BspNode to the LUT.
    node->_index = parms.map->bspNodes.count();
    parms.map->bspNodes.append(node);

    if(BspTreeNode *right = tree.right())
    {
        if(right->isLeaf())
        {
            // Take ownership of this BspLeaf.
            DENG2_ASSERT(right->userData());
            BspLeaf *leaf = right->userData()->castTo<BspLeaf>();
            parms.builder->take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->_index = parms.map->bspLeafs.count();
            parms.map->bspLeafs.append(leaf);
        }
    }

    if(BspTreeNode *left = tree.left())
    {
        if(left->isLeaf())
        {
            // Take ownership of this BspLeaf.
            DENG2_ASSERT(left->userData());
            BspLeaf *leaf = left->userData()->castTo<BspLeaf>();
            parms.builder->take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->_index = parms.map->bspLeafs.count();
            parms.map->bspLeafs.append(leaf);
        }
    }
}

static int collateBspElementsWorker(BspTreeNode &tree, void *parameters)
{
    collateBspElements(tree, *static_cast<CollateBspElementsParms *>(parameters));
    return false; // Continue iteration.
}

static void collateBSP(BspBuilder &builder, GameMap *map)
{
    DENG2_ASSERT(map);
    DENG2_ASSERT(map->bspLeafs.isEmpty());
    DENG2_ASSERT(map->bspNodes.isEmpty());

#ifdef DENG2_QT_4_7_OR_NEWER
    map->bspNodes.reserve(builder.numNodes());
    map->bspLeafs.reserve(builder.numLeafs());
#endif

    BspTreeNode *rootNode = builder.root();
    map->bsp = rootNode->userData();

    if(rootNode->isLeaf())
    {
        // Take ownership of this leaf.
        DENG2_ASSERT(rootNode->userData());
        BspLeaf *leaf = rootNode->userData()->castTo<BspLeaf>();
        builder.take(leaf);

        // Add this BspLeaf to the LUT.
        leaf->_index = 0;
        map->bspLeafs.append(leaf);

        return;
    }

    CollateBspElementsParms parms;
    parms.builder = &builder;
    parms.map     = map;
    rootNode->traversePostOrder(collateBspElementsWorker, &parms);
}

void MPE_SaveBsp(BspBuilder_c *builder_c, GameMap *map, uint numEditableVertexes,
                 Vertex const **editableVertexes)
{
    DENG2_ASSERT(builder_c);
    BspBuilder &builder = *builder_c->inst;

    dint32 rHeight, lHeight;
    BspTreeNode *rootNode = builder.root();
    if(!rootNode->isLeaf())
    {
        rHeight = dint32(rootNode->right()->height());
        lHeight = dint32(rootNode->left()->height());
    }
    else
    {
        rHeight = lHeight = 0;
    }

    LOG_INFO("BSP built: (%d:%d) %d Nodes, %d Leafs, %d HEdges, %d Vertexes.")
            << rHeight << lHeight << builder.numNodes() << builder.numLeafs()
            << builder.numHEdges() << builder.numVertexes();

    collateHEdges(builder, map);
    collateVertexes(builder, map, &numEditableVertexes, &editableVertexes);
    collateBSP(builder, map);
}
