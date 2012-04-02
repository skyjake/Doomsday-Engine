/**
 * @file bspbuilder.cpp
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
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_console.h"
#include "edit_map.h"
#include "p_mapdata.h"

#include <de/Log>

#include "bspbuilder/intersection.hh"
#include "bspbuilder/bspbuilder.hh"
#include "bspbuilder/superblockmap.hh"

using namespace de;

BspBuilder::BspBuilder(GameMap* _map) :
    splitCostFactor(BSPBUILDER_PARTITION_COST_HEDGESPLIT),
    map(_map), lineDefInfos(0), rootNode(0), builtOK(false)
{}

static int C_DECL clearBspHEdgeInfo(BinaryTree* tree, void* /*parameters*/)
{
    if(BinaryTree_IsLeaf(tree))
    {
        BspLeaf* leaf = static_cast<BspLeaf*>(BinaryTree_UserData(tree));
        HEdge* hedge = leaf->hedge;
        do
        {
            Z_Free(HEdge_DetachBspBuildInfo(hedge));
        } while((hedge = hedge->next) != leaf->hedge);
    }
    return 0; // Continue iteration.
}

BspBuilder::~BspBuilder()
{
    // We are finished with the BSP build data.
    if(rootNode)
    {
        // We're done with the build info.
        BinaryTree_PreOrder(rootNode, clearBspHEdgeInfo, NULL/*no parameters*/);
        BinaryTree_Delete(rootNode);
    }
    rootNode = NULL;
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

void BspBuilder::findMapBounds(AABoxf* aaBox) const
{
    Q_ASSERT(aaBox);

    AABoxf bounds;
    boolean initialized = false;

    for(uint i = 0; i < GameMap_LineDefCount(map); ++i)
    {
        LineDef* line = GameMap_LineDef(map, i);

        // Do not consider zero-length LineDefs.
        if(lineDefInfo(*line).flags.testFlag(LineDefInfo::ZEROLENGTH)) continue;

        AABoxf lineAABox;
        initAABoxFromEditableLineDefVertexes(&lineAABox, line);

        if(initialized)
        {
            V2f_AddToBox(bounds.arvec2, lineAABox.min);
        }
        else
        {
            V2f_InitBox(bounds.arvec2, lineAABox.min);
            initialized = true;
        }

        V2f_AddToBox(bounds.arvec2, lineAABox.max);
    }

    if(initialized)
    {
        V2f_CopyBox(aaBox->arvec2, bounds.arvec2);
        return;
    }

    // Clear.
    V2f_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2f_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}

void BspBuilder::createInitialHEdges(SuperBlock& hedgeList)
{
    Q_ASSERT(map);

    for(uint i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* line = GameMap_LineDef(map, i);
        HEdge* front = NULL;
        HEdge* back = NULL;

        // Polyobj lines are completely ignored.
        if(line->inFlags & LF_POLYOBJ) continue;

        // Ignore zero-length and polyobj lines.
        if(!lineDefInfo(*line).flags.testFlag(LineDefInfo::ZEROLENGTH)
           /*&& !lineDefInfo(*line).overlap*/)
        {
            // Check for Humungously long lines.
            if(ABS(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX]) >= 10000 ||
               ABS(line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]) >= 10000)
            {
                if(3000 >=
                   M_Length(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX],
                            line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]))
                {
                    LOG_WARNING("LineDef #%d is very long, it may cause problems.") << line->buildData.index;
                }
            }

            if(line->sideDefs[FRONT])
            {
                SideDef* side = line->sideDefs[FRONT];

                if(!side->sector)
                    LOG_INFO("Bad SideDef on LineDef #%d.") << line->buildData.index;

                front = newHEdge(line, line, line->v[0], line->v[1], side->sector, false);
                hedgeList.hedgePush(front);
            }
            else
            {
                LOG_INFO("LineDef #%d has no front SideDef!") << line->buildData.index;
            }

            if(line->sideDefs[BACK])
            {
                SideDef* side = line->sideDefs[BACK];

                if(!side->sector)
                    LOG_INFO("Bad SideDef on LineDef #%d.") << line->buildData.index;

                back = newHEdge(line, line, line->v[1], line->v[0], side->sector, true);
                hedgeList.hedgePush(back);

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
                if(lineDefInfo(*line).flags.testFlag(LineDefInfo::TWOSIDED))
                {
                    LOG_INFO("LineDef #%d is two-sided but has no back SideDef.") << line->buildData.index;
                    lineDefInfo(*line).flags &= ~LineDefInfo::TWOSIDED;
                }

                // Handle the 'One-Sided Window' trick.
                if(line->buildData.windowEffect && front)
                {
                    HEdge* other = newHEdge(front->bspBuildInfo->lineDef, line,
                                            line->v[1], line->v[0], line->buildData.windowEffect, true);

                    hedgeList.hedgePush(other);

                    // Setup the twin-ing (it's very strange to have a mini
                    // and a normal partnered together).
                    other->twin = front;
                    front->twin = other;
                }
            }
        }

        // @todo edge tips should be created when half-edges are created.
        double x1 = line->v[0]->buildData.pos[VX];
        double y1 = line->v[0]->buildData.pos[VY];
        double x2 = line->v[1]->buildData.pos[VX];
        double y2 = line->v[1]->buildData.pos[VY];

        addEdgeTip(line->v[0], x2 - x1, y2 - y1, back, front);
        addEdgeTip(line->v[1], x1 - x2, y1 - y2, front, back);
    }
}

void BspBuilder::initForMap()
{
    uint numLineDefs = GameMap_LineDefCount(map);
    lineDefInfos.resize(numLineDefs);

    for(uint i = 0; i < numLineDefs; ++i)
    {
        LineDef* l = GameMap_LineDef(map, i);
        LineDefInfo& info = lineDefInfo(*l);
        const Vertex* start = l->v[0];
        const Vertex* end   = l->v[1];

        // Check for zero-length line.
        if((fabs(start->buildData.pos[VX] - end->buildData.pos[VX]) < DIST_EPSILON) &&
           (fabs(start->buildData.pos[VY] - end->buildData.pos[VY]) < DIST_EPSILON))
            info.flags |= LineDefInfo::ZEROLENGTH;

        if(l->sideDefs[BACK] && l->sideDefs[FRONT])
        {
            info.flags |= LineDefInfo::TWOSIDED;

            if(l->sideDefs[BACK]->sector == l->sideDefs[FRONT]->sector)
                info.flags |= LineDefInfo::SELFREF;
        }
    }
}

void BspBuilder::initHEdgesAndBuildBsp(SuperBlockmap& blockmap, HPlane& hplane)
{
    Q_ASSERT(map);

    createInitialHEdges(blockmap.root());

    // Build the tree.
    rootNode = NULL;
    builtOK = buildNodes(blockmap.root(), &rootNode, hplane);

    // Wind the tree.
    if(builtOK)
    {
        windLeafs(rootNode);
    }
}

boolean BspBuilder::build()
{
    if(!map) return false;

    initForMap();

    // Find maximal vertexes.
    AABoxf mapBounds;
    findMapBounds(&mapBounds);

    LOG_VERBOSE("Map bounds:")
            << " min[x:" << mapBounds.minX << ", y:" << mapBounds.minY << "]"
            << " max[x:" << mapBounds.maxX << ", y:" << mapBounds.maxY << "]";

    AABox mapBoundsi;
    mapBoundsi.minX = (int) floor(mapBounds.minX);
    mapBoundsi.minY = (int) floor(mapBounds.minY);
    mapBoundsi.maxX = (int)  ceil(mapBounds.maxX);
    mapBoundsi.maxY = (int)  ceil(mapBounds.maxY);

    AABox blockBounds;
    blockBounds.minX = mapBoundsi.minX - (mapBoundsi.minX & 0x7);
    blockBounds.minY = mapBoundsi.minY - (mapBoundsi.minY & 0x7);
    int bw = ((mapBoundsi.maxX - blockBounds.minX) / 128) + 1;
    int bh = ((mapBoundsi.maxY - blockBounds.minY) / 128) + 1;

    blockBounds.maxX = blockBounds.minX + 128 * M_CeilPow2(bw);
    blockBounds.maxY = blockBounds.minY + 128 * M_CeilPow2(bh);

    SuperBlockmap* blockmap = new SuperBlockmap(blockBounds);
    HPlane* hplane = new HPlane(this);

    initHEdgesAndBuildBsp(*blockmap, *hplane);

    delete hplane;
    delete blockmap;

    return builtOK;
}

BinaryTree* BspBuilder::root() const
{
    return rootNode;
}

const HPlaneIntercept* BspBuilder::hplaneInterceptByVertex(HPlane& hplane, Vertex* vertex)
{
    if(!vertex) return NULL; // Hmm...

    for(HPlane::Intercepts::const_iterator it = hplane.begin(); it != hplane.end(); ++it)
    {
        const HPlaneIntercept* inter = &*it;
        if(((HEdgeIntercept*)inter->userData)->vertex == vertex) return inter;
    }

    return NULL;
}

HEdgeIntercept* BspBuilder::hedgeInterceptByVertex(HPlane& hplane, Vertex* vertex)
{
    const HPlaneIntercept* hpi = hplaneInterceptByVertex(hplane, vertex);
    if(!hpi) return NULL; // Not found.
    return (HEdgeIntercept*) hpi->userData;
}

void BspBuilder::addHEdgesBetweenIntercepts(HPlane& hplane,
    HEdgeIntercept* start, HEdgeIntercept* end, HEdge** right, HEdge** left)
{
    Q_ASSERT(start && end);

    // Create the half-edge pair.
    // Leave 'linedef' field as NULL as these are not linedef-linked.
    // Leave 'side' as zero too.
    const BspHEdgeInfo& info = hplane.partitionHEdgeInfo();
    (*right) = newHEdge(NULL, info.lineDef, start->vertex, end->vertex, start->after, false);
    ( *left) = newHEdge(NULL, info.lineDef, end->vertex, start->vertex, start->after, false);

    // Twin the half-edges together.
    (*right)->twin = *left;
    ( *left)->twin = *right;

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

void Bsp_MergeHEdgeIntercepts(HEdgeIntercept* final, const HEdgeIntercept* other)
{
    Q_ASSERT(final && other);

    /*
    LOG_TRACE("Bsp_MergeHEdgeIntercepts: Merging intersections:");
    HEdgeIntercept::DebugPrint(*final);
    HEdgeIntercept::DebugPrint(*other);
    */

    if(final->selfRef && !other->selfRef)
    {
        if(final->before && other->before)
            final->before = other->before;

        if(final->after && other->after)
            final->after = other->after;

        final->selfRef = false;
    }

    if(!final->before && other->before)
        final->before = other->before;

    if(!final->after && other->after)
        final->after = other->after;

    /*
    LOG_TRACE("Bsp_MergeHEdgeIntercepts: Result:");
    HEdgeIntercept::DebugPrint(*final);
    */
}

void BspBuilder::mergeIntersections(HPlane& hplane)
{
    HPlane::Intercepts::iterator node = hplane.begin();
    while(node != hplane.end())
    {
        HPlane::Intercepts::iterator np = node; np++;
        if(np == hplane.end()) break;

        double len = *np - *node;
        if(len < -0.1)
        {
            Con_Error("BspBuilder_MergeIntersections: Invalid intercept order - %1.3f > %1.3f\n",
                      node->distance, np->distance);
        }
        else if(len > 0.2)
        {
            node++;
            continue;
        }

        HEdgeIntercept* cur  = (HEdgeIntercept*)node->userData;
        HEdgeIntercept* next = (HEdgeIntercept*)np->userData;

        /*if(len > DIST_EPSILON)
        {
            LOG_DEBUG("Skipping very short half-edge (len: %1.3f) near [%1.1f, %1.1f]")
                    << len << cur->vertex->V_pos[VX] << cur->vertex->V_pos[VY];
        }*/

        // Merge info for the two intersections into one.
        Bsp_MergeHEdgeIntercepts(cur, next);

        // Destroy the orphaned info.
        deleteHEdgeIntercept(next);

        // Unlink this intercept.
        hplane.deleteIntercept(np);
    }
}

void BspBuilder::buildHEdgesAtIntersectionGaps(HPlane& hplane, SuperBlock& rightList,
    SuperBlock& leftList)
{
    HPlane::Intercepts::const_iterator node = hplane.begin();
    while(node != hplane.end())
    {
        HPlane::Intercepts::const_iterator np = node; np++;
        if(np == hplane.end()) break;

        HEdgeIntercept* cur = (HEdgeIntercept*)((*node).userData);
        HEdgeIntercept* next = (HEdgeIntercept*)((*np).userData);

        if(!(!cur->after && !next->before))
        {
            // Check for some nasty open/closed or close/open cases.
            if(cur->after && !next->before)
            {
                if(!cur->selfRef)
                {
                    double pos[2];

                    pos[VX] = cur->vertex->buildData.pos[VX] + next->vertex->buildData.pos[VX];
                    pos[VY] = cur->vertex->buildData.pos[VY] + next->vertex->buildData.pos[VY];

                    pos[VX] /= 2;
                    pos[VY] /= 2;

                    MPE_RegisterUnclosedSectorNear(cur->after, pos[VX], pos[VY]);
                }
            }
            else if(!cur->after && next->before)
            {
                if(!next->selfRef)
                {
                    double pos[2];

                    pos[VX] = cur->vertex->buildData.pos[VX] + next->vertex->buildData.pos[VX];
                    pos[VY] = cur->vertex->buildData.pos[VY] + next->vertex->buildData.pos[VY];
                    pos[VX] /= 2;
                    pos[VY] /= 2;

                    MPE_RegisterUnclosedSectorNear(next->before, pos[VX], pos[VY]);
                }
            }
            else
            {
                // This is definitetly open space.
                HEdge* right, *left;

                // Do a sanity check on the sectors (just for good measure).
                if(cur->after != next->before)
                {
                    if(!cur->selfRef && !next->selfRef)
                    {
                        LOG_DEBUG("Sector mismatch: #%d (%1.1f,%1.1f) != #%d (%1.1f,%1.1f)")
                                << cur->after->buildData.index << cur->vertex->buildData.pos[VX]
                                << cur->vertex->buildData.pos[VY] << next->before->buildData.index
                                << next->vertex->buildData.pos[VX] << next->vertex->buildData.pos[VY];
                    }

                    // Choose the non-self-referencing sector when we can.
                    if(cur->selfRef && !next->selfRef)
                    {
                        cur->after = next->before;
                    }
                }

                addHEdgesBetweenIntercepts(hplane, cur, next, &right, &left);

                // Add the new half-edges to the appropriate lists.
                rightList.hedgePush(right);
                leftList.hedgePush(left);
            }
        }

        node++;
    }
}

void BspBuilder::addMiniHEdges(HPlane& hplane, SuperBlock& rightList, SuperBlock& leftList)
{
/*#if _DEBUG
    HPlane_Print(hplane);
#endif*/

    // Fix any issues with the current intersections.
    mergeIntersections(hplane);

    const BspHEdgeInfo& info = hplane.partitionHEdgeInfo();
    LOG_TRACE("Building HEdges along partition [%1.1f, %1.1f] > [%1.1f, %1.1f]")
            << info.pSX << info.pSY << info.pDX << info.pDY;

    // Find connections in the intersections.
    buildHEdgesAtIntersectionGaps(hplane, rightList, leftList);
}

HEdgeIntercept* BspBuilder::newHEdgeIntercept(Vertex* vert, const BspHEdgeInfo* part,
    boolean selfRef)
{
    HEdgeIntercept* inter = new HEdgeIntercept();

    inter->vertex = vert;
    inter->selfRef = selfRef;

    inter->before = openSectorAtPoint(vert, -part->pDX, -part->pDY);
    inter->after  = openSectorAtPoint(vert,  part->pDX,  part->pDY);

    return inter;
}

void BspBuilder::deleteHEdgeIntercept(HEdgeIntercept* inter)
{
    Q_ASSERT(inter);
    delete inter;
}

void BspBuilder::addEdgeTip(Vertex* vert, double dx, double dy, HEdge* back,
    HEdge* front)
{
    edgetip_t* tip = MPE_NewEdgeTip();
    edgetip_t* after;

    tip->angle = M_SlopeToAngle(dx, dy);
    tip->ET_edge[BACK]  = back;
    tip->ET_edge[FRONT] = front;

    // Find the correct place (order is increasing angle).
    for(after = vert->buildData.tipSet; after && after->ET_next;
        after = after->ET_next) {}

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

Sector* BspBuilder::openSectorAtPoint(Vertex* vert, double dX, double dY)
{
    edgetip_t* tip;
    double angle = M_SlopeToAngle(dX, dY);

    // First check whether there's a wall_tip that lies in the exact direction of
    // the given direction (which is relative to the vertex).
    for(tip = vert->buildData.tipSet; tip; tip = tip->ET_next)
    {
        double diff = fabs(tip->angle - angle);

        if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
        {   // Yes, found one.
            return NULL;
        }
    }

    // OK, now just find the first wall_tip whose angle is greater than the angle
    // we're interested in. Therefore we'll be on the FRONT side of that tip edge.
    for(tip = vert->buildData.tipSet; tip; tip = tip->ET_next)
    {
        if(angle + ANG_EPSILON < tip->angle)
        {
            // Found it.
            return (tip->ET_edge[FRONT]? tip->ET_edge[FRONT]->sector : NULL);
        }

        if(!tip->ET_next)
        {
            // No more tips, therefore this is the BACK of the tip with the largest angle.
            return (tip->ET_edge[BACK]? tip->ET_edge[BACK]->sector : NULL);
        }
    }

    Con_Error("Vertex %d has no tips !", vert->buildData.index);
    exit(1); // Unreachable.
}
