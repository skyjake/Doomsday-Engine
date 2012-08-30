/**
 * @file edit_bsp.cpp
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

BspBuilder_c* BspBuilder_New(GameMap* map, uint* numEditableVertexes, Vertex*** editableVertexes)
{
    BspBuilder_c* builder = static_cast<BspBuilder_c*>(malloc(sizeof *builder));
    builder->inst = new BspBuilder(map, numEditableVertexes, editableVertexes);
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
    BspBuilder* builder;
    size_t curIdx;
    HEdge*** hedgeLUT;
} hedgecollectorparams_t;

static int hedgeCollector(BspTreeNode& tree, void* parameters)
{
    if(tree.isLeaf())
    {
        hedgecollectorparams_t* p = static_cast<hedgecollectorparams_t*>(parameters);
        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
        HEdge* hedge = leaf->hedge;
        do
        {
            // Take ownership of this HEdge.
            runtime_mapdata_header_t* hdr = reinterpret_cast<runtime_mapdata_header_t*>(hedge);
            Q_ASSERT(hdr);
            p->builder->releaseOwnership(hdr);

            // Add this HEdge to the LUT.
            hedge->index = p->curIdx++;
            (*p->hedgeLUT)[hedge->index] = hedge;

        } while((hedge = hedge->next) != leaf->hedge);
    }
    return false; // Continue traversal.
}

static void buildHEdgeLut(BspBuilder& builder, GameMap* map)
{
    Q_ASSERT(map);

    if(map->hedges)
    {
        Z_Free(map->hedges);
        map->hedges = 0;
    }

    map->numHEdges = builder.numHEdges();
    if(!map->numHEdges) return; // Should never happen.

    // Allocate the LUT and acquire ownership of the half-edges.
    map->hedges = static_cast<HEdge**>(Z_Calloc(map->numHEdges * sizeof(HEdge*), PU_MAPSTATIC, 0));

    hedgecollectorparams_t parm;
    parm.builder = &builder;
    parm.curIdx = 0;
    parm.hedgeLUT = &map->hedges;
    BspTreeNode::InOrder(*builder.root(), hedgeCollector, &parm);
}

static void finishHEdges(GameMap* map)
{
    Q_ASSERT(map);

    for(uint i = 0; i < map->numHEdges; ++i)
    {
        HEdge* hedge = map->hedges[i];

        if(hedge->lineDef)
        {
            Vertex* vtx = hedge->lineDef->L_v(hedge->side);

            hedge->sector = hedge->lineDef->L_sector(hedge->side);
            hedge->offset = V2d_Distance(hedge->HE_v1origin, vtx->origin);
        }

        hedge->angle = bamsAtan2((int) (hedge->HE_v2origin[VY] - hedge->HE_v1origin[VY]),
                                 (int) (hedge->HE_v2origin[VX] - hedge->HE_v1origin[VX])) << FRACBITS;

        // Calculate the length of the segment.
        hedge->length = V2d_Distance(hedge->HE_v2origin, hedge->HE_v1origin);

        if(hedge->length == 0)
            hedge->length = 0.01f; // Hmm...
    }
}

typedef struct {
    BspBuilder* builder;
    GameMap* dest;
    uint leafCurIndex;
    uint nodeCurIndex;
} populatebspobjectluts_params_t;

static int populateBspObjectLuts(BspTreeNode& tree, void* parameters)
{
    populatebspobjectluts_params_t* p = static_cast<populatebspobjectluts_params_t*>(parameters);
    Q_ASSERT(p);

    // We are only interested in BspNodes at this level.
    if(tree.isLeaf()) return false; // Continue iteration.

    // Take ownership of this BspNode.
    Q_ASSERT(tree.userData());
    BspNode* node = reinterpret_cast<BspNode*>(tree.userData());
    p->builder->releaseOwnership(tree.userData());

    // Add this BspNode to the LUT.
    node->index = p->nodeCurIndex++;
    p->dest->bspNodes[node->index] = node;

    if(BspTreeNode* right = tree.right())
    {
        if(right->isLeaf())
        {
            // Take ownership of this BspLeaf.
            Q_ASSERT(right->userData());
            BspLeaf* leaf = reinterpret_cast<BspLeaf*>(right->userData());
            p->builder->releaseOwnership(right->userData());

            // Add this BspLeaf to the LUT.
            leaf->index = p->leafCurIndex++;
            p->dest->bspLeafs[leaf->index] = leaf;
        }
    }

    if(BspTreeNode* left = tree.left())
    {
        if(left->isLeaf())
        {
            // Take ownership of this BspLeaf.
            Q_ASSERT(left->userData());
            BspLeaf* leaf = reinterpret_cast<BspLeaf*>(left->userData());
            p->builder->releaseOwnership(left->userData());

            // Add this BspLeaf to the LUT.
            leaf->index = p->leafCurIndex++;
            p->dest->bspLeafs[leaf->index] = leaf;
        }
    }

    return false; // Continue iteration.
}

static void hardenBSP(BspBuilder& builder, GameMap* dest)
{
    dest->numBspNodes = builder.numNodes();
    if(dest->numBspNodes != 0)
        dest->bspNodes = static_cast<BspNode**>(Z_Malloc(dest->numBspNodes * sizeof(BspNode*), PU_MAPSTATIC, 0));
    else
        dest->bspNodes = 0;

    dest->numBspLeafs = builder.numLeafs();
    dest->bspLeafs = static_cast<BspLeaf**>(Z_Calloc(dest->numBspLeafs * sizeof(BspLeaf*), PU_MAPSTATIC, 0));

    BspTreeNode* rootNode = builder.root();
    dest->bsp = rootNode->userData();

    if(rootNode->isLeaf())
    {
        // Take ownership of this leaf.
        Q_ASSERT(rootNode->userData());
        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(rootNode->userData());
        builder.releaseOwnership(rootNode->userData());

        // Add this BspLeaf to the LUT.
        leaf->index = 0;
        dest->bspLeafs[0] = leaf;

        return;
    }

    populatebspobjectluts_params_t p;
    p.builder = &builder;
    p.dest = dest;
    p.leafCurIndex = 0;
    p.nodeCurIndex = 0;
    BspTreeNode::PostOrder(*rootNode, populateBspObjectLuts, &p);
}

static void copyVertex(Vertex& vtx, Vertex const& other)
{
    V2d_Copy(vtx.origin, other.origin);
    vtx.numLineOwners = other.numLineOwners;
    vtx.lineOwners = other.lineOwners;

    vtx.buildData.index = other.buildData.index;
    vtx.buildData.refCount = other.buildData.refCount;
    vtx.buildData.equiv = other.buildData.equiv;
}

static void hardenVertexes(BspBuilder& builder, GameMap* map,
    uint* numEditableVertexes, Vertex*** editableVertexes)
{
    uint bspVertexCount = builder.numVertexes();

    map->numVertexes = *numEditableVertexes + bspVertexCount;
    map->vertexes = static_cast<Vertex*>(Z_Calloc(map->numVertexes * sizeof(Vertex), PU_MAPSTATIC, 0));

    uint n = 0;
    for(; n < *numEditableVertexes; ++n)
    {
        Vertex& dest = map->vertexes[n];
        Vertex const& src = *((*editableVertexes)[n]);

        dest.header.type = DMU_VERTEX;
        copyVertex(dest, src);
    }

    for(uint i = 0; i < bspVertexCount; ++i, ++n)
    {
        Vertex& dest = map->vertexes[n];
        Vertex& src  = builder.vertex(i);

        builder.releaseOwnership(reinterpret_cast<runtime_mapdata_header_t*>(&src));

        dest.header.type = DMU_VERTEX;
        copyVertex(dest, src);
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

void MPE_SaveBsp(BspBuilder_c* builder_c, GameMap* map, uint* numEditableVertexes,
    Vertex*** editableVertexes)
{
    Q_ASSERT(builder_c);
    BspBuilder& builder = *builder_c->inst;

    dint32 rHeight, lHeight;
    BspTreeNode* rootNode = builder.root();
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

    buildHEdgeLut(builder, map);
    hardenVertexes(builder, map, numEditableVertexes, editableVertexes);
    updateVertexLinks(map);

    finishHEdges(map);
    hardenBSP(builder, map);
}
