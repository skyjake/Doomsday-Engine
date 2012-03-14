/**
 * @file bsp_main.c
 * BSP Builder. @ingroup map
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

#include "de_base.h"
#include "de_console.h"
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"

#include "p_mapdata.h"

int bspFactor = 7;

void BSP_Register(void)
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

static void initAABoxFromEditableLineDefVertexes(AABoxf* aaBox, const LineDef* line)
{
    const double* from = line->L_v1->buildData.pos;
    const double* to   = line->L_v2->buildData.pos;
    aaBox->minX = MIN_OF(from[VX], to[VX]);
    aaBox->minY = MIN_OF(from[VY], to[VY]);
    aaBox->maxX = MAX_OF(from[VX], to[VX]);
    aaBox->maxY = MAX_OF(from[VY], to[VY]);
}

typedef struct {
    AABoxf bounds;
    boolean initialized;
} findmapboundsparams_t;

static int findMapBoundsIterator(LineDef* line, void* parameters)
{
    findmapboundsparams_t* p = (findmapboundsparams_t*) parameters;
    AABoxf lineAABox;
    assert(p);

    // Do not consider zero-length LineDefs.
    if(line->buildData.mlFlags & MLF_ZEROLENGTH) return false; // Continue iteration.

    initAABoxFromEditableLineDefVertexes(&lineAABox, line);
    if(p->initialized)
    {
        V2_AddToBox(p->bounds.arvec2, lineAABox.min);
    }
    else
    {
        V2_InitBox(p->bounds.arvec2, lineAABox.min);
        p->initialized = true;
    }
    V2_AddToBox(p->bounds.arvec2, lineAABox.max);
    return false; // Continue iteration.
}

static void findMapBounds(GameMap* map, AABoxf* aaBox)
{
    assert(map && aaBox);

    if(GameMap_LineDefCount(map))
    {
        findmapboundsparams_t parm;
        parm.initialized = false;
        GameMap_LineDefIterator(map, findMapBoundsIterator, (void*)&parm);
        if(parm.initialized)
        {
            V2_CopyBox(aaBox->arvec2, parm.bounds.arvec2);
            return;
        }
    }

    // Clear.
    V2_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}

/**
 * Initially create all half-edges, one for each side of a linedef.
 *
 * @return  The list of created half-edges.
 */
static SuperBlock* createInitialHEdges(GameMap* map)
{
    uint startTime = Sys_GetRealTime();

    bsp_hedge_t* back, *front;
    SuperBlock* block;
    AABoxf mapBoundsf;
    AABox mapBounds, blockBounds;
    int bw, bh;
    uint i;

    // Find maximal vertexes.
    findMapBounds(map, &mapBoundsf);

    mapBounds.minX = (int) floor(mapBoundsf.minX);
    mapBounds.minY = (int) floor(mapBoundsf.minY);
    mapBounds.maxX = (int)  ceil(mapBoundsf.maxX);
    mapBounds.maxY = (int)  ceil(mapBoundsf.maxY);

    VERBOSE2(
    Con_Message("Map goes from [x:%f, y:%f] -> [x:%f, y:%f]\n",
                mapBoundsf.minX,  mapBoundsf.minY, mapBoundsf.maxX, mapBoundsf.maxY) );

    blockBounds.minX = mapBounds.minX - (mapBounds.minX & 0x7);
    blockBounds.minY = mapBounds.minY - (mapBounds.minY & 0x7);
    bw = ((mapBounds.maxX - blockBounds.minX) / 128) + 1;
    bh = ((mapBounds.maxY - blockBounds.minY) / 128) + 1;

    blockBounds.maxX = blockBounds.minX + 128 * M_CeilPow2(bw);
    blockBounds.maxY = blockBounds.minY + 128 * M_CeilPow2(bh);

    block = BSP_NewSuperBlock(&blockBounds);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* line = &map->lineDefs[i];

        if(line->buildData.mlFlags & MLF_POLYOBJ) continue;

        front = back = NULL;

        // Ignore zero-length and polyobj lines.
        if(!(line->buildData.mlFlags & MLF_ZEROLENGTH)
           /*&& !line->buildData.overlap*/)
        {
            // Check for Humungously long lines.
            if(ABS(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX]) >= 10000 ||
               ABS(line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]) >= 10000)
            {
                if(3000 >=
                   M_Length(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX],
                            line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]))
                {
                    Con_Message("Warning: Linedef #%d is VERY long, it may cause problems\n",
                                line->buildData.index);
                }
            }

            if(line->sideDefs[FRONT])
            {
                SideDef* side = line->sideDefs[FRONT];

                if(!side->sector)
                    Con_Message("Warning: Bad sidedef on linedef #%d\n", line->buildData.index);

                front = BSP_HEdge_Create(line, line, line->v[0], line->v[1], side->sector, false);
                SuperBlock_HEdgePush(block, front);
            }
            else
                Con_Message("Warning: Linedef #%d has no front sidedef!\n", line->buildData.index);

            if(line->sideDefs[BACK])
            {
                SideDef* side = line->sideDefs[BACK];

                if(!side->sector)
                    Con_Message("Warning: Bad sidedef on linedef #%d\n", line->buildData.index);

                back = BSP_HEdge_Create(line, line, line->v[1], line->v[0], side->sector, true);
                SuperBlock_HEdgePush(block, back);

                if(front)
                {
                    // Half-edges always maintain a one-to-one relationship with their
                    // twins, so if one gets split, the other must be split also.
                    back->twin = front;
                    front->twin = back;
                }
            }
            else
            {
                if(line->buildData.mlFlags & MLF_TWOSIDED)
                {
                    Con_Message("Warning: Linedef #%d is 2s but has no back sidedef\n", line->buildData.index);
                    line->buildData.mlFlags &= ~MLF_TWOSIDED;
                }

                // Handle the 'One-Sided Window' trick.
                if(line->buildData.windowEffect && front)
                {
                    bsp_hedge_t* other;

                    other = BSP_HEdge_Create(front->lineDef, line, line->v[1], line->v[0],
                                             line->buildData.windowEffect, true);

                    SuperBlock_HEdgePush(block, other);

                    // Setup the twin-ing (it's very strange to have a mini
                    // and a normal partnered together).
                    other->twin = front;
                    front->twin = other;
                }
            }
        }

        // @todo edge tips should be created when half-edges are created.
        {
        double x1 = line->v[0]->buildData.pos[VX];
        double y1 = line->v[0]->buildData.pos[VY];
        double x2 = line->v[1]->buildData.pos[VX];
        double y2 = line->v[1]->buildData.pos[VY];

        BSP_CreateVertexEdgeTip(line->v[0], x2 - x1, y2 - y1, back, front);
        BSP_CreateVertexEdgeTip(line->v[1], x1 - x2, y1 - y2, front, back);
        }
    }

    // How much time did we spend?
    VERBOSE2( Con_Message("createInitialHEdges: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    return block;
}

static boolean C_DECL freeBSPData(binarytree_t* tree, void* data)
{
    void* bspData = BinaryTree_GetData(tree);

    if(bspData)
    {
        if(BinaryTree_IsLeaf(tree))
            BSPLeaf_Destroy(bspData);
        else
            M_Free(bspData);
    }

    BinaryTree_SetData(tree, NULL);

    return true; // Continue iteration.
}

boolean BSP_Build(GameMap* map, Vertex*** vertexes, uint* numVertexes)
{
    boolean builtOK;
    uint startTime;
    SuperBlock* hEdgeList;
    binarytree_t* rootNode;

    VERBOSE( Con_Message("BSP_Build: Processing map using tunable factor of %d...\n", bspFactor) )

    // It begins...
    startTime = Sys_GetRealTime();

    BSP_InitSuperBlockAllocator();
    BSP_InitIntersectionAllocator();
    BSP_InitHEdgeAllocator();

    BSP_InitForNodeBuild(map);

    // Create initial half-edges.
    hEdgeList = createInitialHEdges(map);

    // Build the BSP.
    {
    uint buildStartTime = Sys_GetRealTime();
    BspIntersections* bspIntersections;

    bspIntersections = BspIntersections_New();

    // Recursively create nodes.
    rootNode = NULL;
    builtOK = BuildNodes(hEdgeList, &rootNode, 0, bspIntersections);

    // The intersection list is no longer needed.
    BspIntersections_Delete(bspIntersections);

    // How much time did we spend?
    VERBOSE2( Con_Message("BuildNodes: Done in %.2f seconds.\n", (Sys_GetRealTime() - buildStartTime) / 1000.0f));
    }

    BSP_RecycleSuperBlock(hEdgeList);

    if(builtOK)
    {
        // Success!
        long rHeight, lHeight;

        // Wind the BSP tree and link to the map.
        ClockwiseBspTree(rootNode);
        SaveMap(map, rootNode, vertexes, numVertexes);

        if(rootNode && !BinaryTree_IsLeaf(rootNode))
        {
            rHeight = (long) BinaryTree_GetHeight(BinaryTree_GetChild(rootNode, RIGHT));
            lHeight = (long) BinaryTree_GetHeight(BinaryTree_GetChild(rootNode, LEFT));
        }
        else
        {
            rHeight = lHeight = 0;
        }

        VERBOSE(
        Con_Message("BSP built: %d Nodes, %d BspLeafs, %d HEdges, %d Vertexes\n"
                    "  Balance %+ld (l%ld - r%ld).\n", map->numBspNodes, map->numBspLeafs,
                    map->numHEdges, map->numVertexes, lHeight - rHeight, lHeight, rHeight) );
    }

    // We are finished with the BSP build data.
    if(rootNode)
    {
        BinaryTree_PostOrder(rootNode, freeBSPData, NULL);
        BinaryTree_Destroy(rootNode);
    }
    rootNode = NULL;

    // Free temporary storage.
    BSP_ShutdownHEdgeAllocator();
    BSP_ShutdownIntersectionAllocator();
    BSP_ShutdownSuperBlockAllocator();

    // How much time did we spend?
    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    return builtOK;
}

typedef struct {
    Vertex* vertex;
    HEdgeIntercept* found;
} findintersectionforvertexworkerparams_t;

static int findIntersectionForVertexWorker(BspIntersection* bi, void* parameters)
{
    HEdgeIntercept* inter = (HEdgeIntercept*) BspIntersection_UserData(bi);
    findintersectionforvertexworkerparams_t* p = (findintersectionforvertexworkerparams_t*) parameters;
    assert(p);
    if(inter->vertex == p->vertex)
    {
        p->found = inter;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

HEdgeIntercept* Bsp_HEdgeInterceptByVertex(BspIntersections* intersections, Vertex* vertex)
{
    findintersectionforvertexworkerparams_t parm;

    if(!intersections || !vertex) return NULL; // Hmm...

    parm.vertex = vertex;
    parm.found = NULL;
    BspIntersections_Iterate2(intersections, findIntersectionForVertexWorker, (void*)&parm);
    return parm.found;
}

void Bsp_BuildHEdgesBetweenIntersections(BspIntersections* bspIntersections,
    HEdgeIntercept* start, HEdgeIntercept* end, bsp_hedge_t** right, bsp_hedge_t** left)
{
    BsPartitionInfo* part;

    if(!bspIntersections || !start || !end)
        Con_Error("Bsp_BuildHEdgesBetweenIntersections: Invalid arguments.");

    part = BspIntersections_Info(bspIntersections);

    // Create the half-edge pair.
    // Leave 'linedef' field as NULL as these are not linedef-linked.
    // Leave 'side' as zero too.
    (*right) = BSP_HEdge_Create(NULL, part->lineDef, start->vertex,
                                end->vertex, start->after, false);
    (*left)  = BSP_HEdge_Create(NULL, part->lineDef, end->vertex,
                                start->vertex, start->after, false);

    // Twin the half-edges together.
    (*right)->twin = *left;
    (*left)->twin = *right;

    /*
    DEBUG_Message(("buildHEdgesBetweenIntersections: Capped intersection:\n"));
    DEBUG_Message(("  %p RIGHT sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                   (*right), ((*right)->sector? (*right)->sector->index : -1),
                   (*right)->v[0]->V_pos[VX], (*right)->v[0]->V_pos[VY],
                   (*right)->v[1]->V_pos[VX], (*right)->v[1]->V_pos[VY]));
    DEBUG_Message(("  %p LEFT  sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                   (*left), ((*left)->sector? (*left)->sector->index : -1),
                   (*left)->v[0]->V_pos[VX], (*left)->v[0]->V_pos[VY],
                   (*left)->v[1]->V_pos[VX], (*left)->v[1]->V_pos[VY]));
    */
}

void BSP_AddMiniHEdges(BspIntersections* bspIntersections, SuperBlock* rightList,
    SuperBlock* leftList)
{
    if(!bspIntersections) return;

/*#if _DEBUG
    BspIntersections_Print(bspIntersections);
#endif*/

    // Fix any issues with the current intersections.
    Bsp_MergeIntersections(bspIntersections);

    /*{
    const BsPartitionInfo* part = BspIntersections_Info(bspIntersections);
    DEBUG_Message(("BSP_AddMiniHEdges: Partition (%1.1f,%1.1f) += (%1.1f,%1.1f)\n",
                   part->pSX, part->pSY, part->pDX, part->pDY));
    }*/

    // Find connections in the intersections.
    Bsp_BuildHEdgesAtIntersectionGaps(bspIntersections, rightList, leftList);
}

HEdgeIntercept* Bsp_NewHEdgeIntercept(Vertex* vert, const struct bspartitioninfo_s* part,
    boolean selfRef)
{
    HEdgeIntercept* inter = M_Calloc(sizeof(*inter));

    inter->vertex = vert;
    inter->selfRef = selfRef;

    inter->before = BSP_VertexCheckOpen(vert, -part->pDX, -part->pDY);
    inter->after  = BSP_VertexCheckOpen(vert,  part->pDX,  part->pDY);

    return inter;
}

void Bsp_DeleteHEdgeIntercept(HEdgeIntercept* inter)
{
    assert(inter);
    M_Free(inter);
}

#if _DEBUG
void Bsp_PrintHEdgeIntercept(HEdgeIntercept* inter)
{
    assert(inter);
    Con_Message("  Vertex %8X (%1.1f,%1.1f) beforeSector: %d afterSector:%d %s\n",
                inter->vertex->buildData.index, inter->vertex->buildData.pos[VX],
                inter->vertex->buildData.pos[VY],
                (inter->before? inter->before->buildData.index : -1),
                (inter->after? inter->after->buildData.index : -1),
                (inter->selfRef? "SELFREF" : ""));
}
#endif
