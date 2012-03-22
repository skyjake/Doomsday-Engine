/**
 * @file bsp_node.c
 * BSP Builder Node. Recursive node creation and sorting. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @authors Copyright © 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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
//#include "de_misc.h"

#include "bspbuilder/intersection.hh"
#include "bspbuilder/bspbuilder.hh"

using namespace de;

typedef struct evalinfo_s {
    int cost;
    int splits;
    int iffy;
    int nearMiss;
    int realLeft;
    int realRight;
    int miniLeft;
    int miniRight;
} evalinfo_t;

// Used when sorting BSP leaf half-edges by angle around midpoint.
static size_t hEdgeSortBufSize;
static bsp_hedge_t** hEdgeSortBuf;

#if _DEBUG
void BSP_PrintSuperBlockHEdges(SuperBlock* superblock);
#endif

static __inline int pointOnHEdgeSide(double x, double y, const bsp_hedge_t* part)
{
    return P_PointOnLinedefSide2(x, y, part->pDX, part->pDY, part->pPerp,
                                 part->pLength, DIST_EPSILON);
}

static boolean getAveragedCoords(bsp_hedge_t* headPtr, double* x, double* y)
{
    size_t total = 0;
    double avg[2];
    bsp_hedge_t* cur;

    if(!x || !y) return false;

    avg[VX] = avg[VY] = 0;

    for(cur = headPtr; cur; cur = cur->nextInLeaf)
    {
        avg[VX] += cur->v[0]->buildData.pos[VX];
        avg[VY] += cur->v[0]->buildData.pos[VY];

        avg[VX] += cur->v[1]->buildData.pos[VX];
        avg[VY] += cur->v[1]->buildData.pos[VY];

        total += 2;
    }

    if(total > 0)
    {
        *x = avg[VX] / total;
        *y = avg[VY] / total;
        return true;
    }

    return false;
}

/**
 * Sort half-edges by angle (from the middle point to the start vertex).
 * The desired order (clockwise) means descending angles.
 *
 * @algorithm "double bubble"
 */
static void sortHEdgesByAngleAroundPoint(bsp_hedge_t** hEdges, size_t total,
    double x, double y)
{
    size_t i;

    i = 0;
    while(i + 1 < total)
    {
        bsp_hedge_t* a = hEdges[i];
        bsp_hedge_t* b = hEdges[i+1];
        double angle1, angle2;

        angle1 = M_SlopeToAngle(a->v[0]->buildData.pos[VX] - x,
                                a->v[0]->buildData.pos[VY] - y);
        angle2 = M_SlopeToAngle(b->v[0]->buildData.pos[VX] - x,
                                b->v[0]->buildData.pos[VY] - y);

        if(angle1 + ANG_EPSILON < angle2)
        {
            // Swap them.
            hEdges[i] = b;
            hEdges[i + 1] = a;

            // Bubble down.
            if(i > 0)
                i--;
        }
        else
        {
            // Bubble up.
            i++;
        }
    }
}

/**
 * Sort the given list of half-edges into clockwise order based on their
 * position/orientation compared to the specified point.
 *
 * @param headPtr       Ptr to the address of the headPtr to the list
 *                      of hedges to be sorted.
 * @param num           Number of half edges in the list.
 * @param x             X coordinate of the point to order around.
 * @param y             Y coordinate of the point to order around.
 */
static void clockwiseOrder(bsp_hedge_t** headPtr, size_t num, double x, double y)
{
    bsp_hedge_t* hEdge;
    size_t i;

    // Insert ptrs to the hEdges into the sort buffer.
    for(hEdge = *headPtr, i = 0; hEdge; hEdge = hEdge->nextInLeaf, ++i)
    {
        hEdgeSortBuf[i] = hEdge;
    }
    hEdgeSortBuf[i] = NULL; // Terminate.

    if(i != num)
        Con_Error("clockwiseOrder: Miscounted?");

    sortHEdgesByAngleAroundPoint(hEdgeSortBuf, num, x, y);

    // Re-link the half-edge list in the order of the sorted array.
    *headPtr = NULL;
    for(i = 0; i < num; ++i)
    {
        size_t idx = (num - 1) - i;
        size_t j = idx % num;

        hEdgeSortBuf[j]->nextInLeaf = *headPtr;
        *headPtr = hEdgeSortBuf[j];
    }

/*#if _DEBUG
    Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

    for(hEdge = sub->hEdges; hEdge; hEdge = hEdge->next)
    {
        double angle = M_SlopeToAngle(hEdge->v[0]->V_pos[VX] - x,
                                       hEdge->v[0]->V_pos[VY] - y);

        Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                    hEdge, angle, hEdge->v[0]->V_pos[VX], hEdge->v[0]->V_pos[VY],
                    hEdge->v[1]->V_pos[VX], hEdge->v[1]->V_pos[VY]);
    }
#endif*/
}

static void sanityCheckClosed(const bspleafdata_t* leaf)
{
    int total = 0, gaps = 0;
    bsp_hedge_t* cur, *next;

    for(cur = leaf->hEdges; cur; cur = cur->nextInLeaf)
    {
        next = (cur->nextInLeaf? cur->nextInLeaf : leaf->hEdges);

        if(cur->v[1]->buildData.pos[VX] != next->v[0]->buildData.pos[VX] ||
           cur->v[1]->buildData.pos[VY] != next->v[0]->buildData.pos[VY])
        {
            gaps++;
        }

        total++;
    }

    if(gaps > 0)
    {
        VERBOSE( Con_Message("HEdge list for leaf #%p is not closed (%d gaps, %d half-edges)\n", leaf, gaps, total) );

/*#if _DEBUG
    for(cur = leaf->hEdges; cur; cur = cur->next)
    {
        Con_Message("  half-edge %p  (%1.1f,%1.1f) --> (%1.1f,%1.1f)\n", cur,
                    cur->v[0]->pos[VX], cur->v[0]->pos[VY],
                    cur->v[1]->pos[VX], cur->v[1]->pos[VY]);
    }
#endif*/
    }
}

static void sanityCheckSameSector(const bspleafdata_t* leaf)
{
    bsp_hedge_t* cur, *compare;

    // Find a suitable half-edge for comparison.
    for(compare = leaf->hEdges; compare; compare = compare->nextInLeaf)
    {
        if(compare->sector) break;
    }

    if(!compare) return;

    for(cur = compare->nextInLeaf; cur; cur = cur->nextInLeaf)
    {
        if(!cur->sector) continue;

        if(cur->sector == compare->sector) continue;

        // Prevent excessive number of warnings.
        if(compare->sector->buildData.warnedFacing == cur->sector->buildData.index)
            continue;

        compare->sector->buildData.warnedFacing = cur->sector->buildData.index;

        if(verbose >= 1)
        {
            if(cur->lineDef)
                Con_Message("Sector #%d has sidedef facing #%d (line #%d).\n", compare->sector->buildData.index,
                            cur->sector->buildData.index, cur->lineDef->buildData.index);
            else
                Con_Message("Sector #%d has sidedef facing #%d.\n",
                            compare->sector->buildData.index, cur->sector->buildData.index);
        }
    }
}

static boolean sanityCheckHasRealHEdge(const bspleafdata_t* leaf)
{
    bsp_hedge_t* cur;

    for(cur = leaf->hEdges; cur; cur = cur->nextInLeaf)
    {
        if(cur->lineDef)
            return true;
    }

    return false;
}

static void renumberLeafHEdges(bspleafdata_t* leaf, uint* curIndex)
{
    uint n;
    bsp_hedge_t* cur;

    n = 0;
    for(cur = leaf->hEdges; cur; cur = cur->nextInLeaf)
    {
        cur->index = *curIndex;
        (*curIndex)++;
        n++;
    }
}

static void prepareHEdgeSortBuffer(size_t numHEdges)
{
    // Do we need to enlarge our sort buffer?
    if(numHEdges + 1 > hEdgeSortBufSize)
    {
        hEdgeSortBufSize = numHEdges + 1;
        hEdgeSortBuf = (bsp_hedge_t**)M_Realloc(hEdgeSortBuf, hEdgeSortBufSize * sizeof(*hEdgeSortBuf));
    }
}

static boolean C_DECL clockwiseLeaf(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        bspleafdata_t* leaf = (bspleafdata_t*) BinaryTree_GetData(tree);
        double midPoint[2] = { 0, 0 };
        bsp_hedge_t* hEdge;
        size_t total;

        getAveragedCoords(leaf->hEdges, &midPoint[VX], &midPoint[VY]);

        // Count half-edges.
        total = 0;
        for(hEdge = leaf->hEdges; hEdge; hEdge = hEdge->nextInLeaf)
            total++;

        // Ensure the sort buffer is large enough.
        prepareHEdgeSortBuffer(total);

        clockwiseOrder(&leaf->hEdges, total, midPoint[VX], midPoint[VY]);
        renumberLeafHEdges(leaf, (uint*)data);

        // Do some sanity checks.
        sanityCheckClosed(leaf);
        sanityCheckSameSector(leaf);
        if(!sanityCheckHasRealHEdge(leaf))
        {
            Con_Error("BSP Leaf #%p has no linedef-linked half-edge!", leaf);
        }
    }

    return true; // Continue traversal.
}

void BspBuilder::windLeafs(binarytree_t* rootNode)
{
    uint curIndex;

    hEdgeSortBufSize = 0;
    hEdgeSortBuf = NULL;

    curIndex = 0;
    BinaryTree_PostOrder(rootNode, clockwiseLeaf, &curIndex);

    // Free temporary storage.
    if(hEdgeSortBuf)
    {
        M_Free(hEdgeSortBuf);
        hEdgeSortBuf = NULL;
    }
}

static __inline bspleafdata_t* allocBSPLeaf(void)
{
    bspleafdata_t* leafData = (bspleafdata_t*)M_Malloc(sizeof *leafData);
    return leafData;
}

static __inline void freeBSPLeaf(bspleafdata_t* leaf)
{
    M_Free(leaf);
}

bspleafdata_t* BspBuilder::newLeaf(void)
{
    bspleafdata_t* leaf = allocBSPLeaf();

    leaf->hEdges = NULL;

    return leaf;
}

void BspBuilder::deleteLeaf(bspleafdata_t* leaf)
{
    bsp_hedge_t* cur, *np;

    if(!leaf) return;

    cur = leaf->hEdges;
    while(cur)
    {
        np = cur->nextInLeaf;
        deleteHEdge(cur);
        cur = np;
    }

    freeBSPLeaf(leaf);
}

typedef struct {
    bsp_hedge_t* part;
    int bestCost;
    evalinfo_t* info;
} evalpartitionworkerparams_t;

static int evalPartitionWorker2(bsp_hedge_t* check, void* parameters)
{
#define ADD_LEFT()  \
      do {  \
        if (check->lineDef) p->info->realLeft += 1;  \
        else                p->info->miniLeft += 1;  \
      } while (0)

#define ADD_RIGHT()  \
      do {  \
        if (check->lineDef) p->info->realRight += 1;  \
        else                p->info->miniRight += 1;  \
      } while (0)

    evalpartitionworkerparams_t* p = (evalpartitionworkerparams_t*) parameters;
    const int factor = bspFactor;
    double qnty, a, b, fa, fb;
    assert(p);

    // Catch "bad half-edges" early on.
    if(p->info->cost > p->bestCost) return true; // Stop iteration.

    // Get state of lines' relation to each other.
    if(check->sourceLineDef == p->part->sourceLineDef)
    {
        a = b = fa = fb = 0;
    }
    else
    {
        a = M_PerpDist(p->part->pDX, p->part->pDY, p->part->pPerp, p->part->pLength,
                       check->pSX, check->pSY);
        b = M_PerpDist(p->part->pDX, p->part->pDY, p->part->pPerp, p->part->pLength,
                       check->pEX, check->pEY);

        fa = fabs(a);
        fb = fabs(b);
    }

    // Check for being on the same line.
    if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
    {
        // This half-edge runs along the same line as the partition.
        // Check whether it goes in the same direction or the opposite.
        if(check->pDX * p->part->pDX + check->pDY * p->part->pDY < 0)
        {
            ADD_LEFT();
        }
        else
        {
            ADD_RIGHT();
        }

        return false; // Continue iteration.
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        ADD_RIGHT();

        // Check for a near miss.
        if((a >= IFFY_LEN && b >= IFFY_LEN) ||
           (a <= DIST_EPSILON && b >= IFFY_LEN) ||
           (b <= DIST_EPSILON && a >= IFFY_LEN))
        {
            return false; // Continue iteration.
        }

        p->info->nearMiss++;

        /**
         * Near misses are bad, since they have the potential to cause really short
         * minihedges to be created in future processing. Thus the closer the near
         * miss, the higher the cost.
         */

        if(a <= DIST_EPSILON || b <= DIST_EPSILON)
            qnty = IFFY_LEN / MAX_OF(a, b);
        else
            qnty = IFFY_LEN / MIN_OF(a, b);

        p->info->cost += (int) (100 * factor * (qnty * qnty - 1.0));
        return false; // Continue iteration.
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        ADD_LEFT();

        // Check for a near miss.
        if((a <= -IFFY_LEN && b <= -IFFY_LEN) ||
           (a >= -DIST_EPSILON && b <= -IFFY_LEN) ||
           (b >= -DIST_EPSILON && a <= -IFFY_LEN))
        {
            return false; // Continue iteration.
        }

        p->info->nearMiss++;

        // The closer the miss, the higher the cost (see note above).
        if(a >= -DIST_EPSILON || b >= -DIST_EPSILON)
            qnty = IFFY_LEN / -MIN_OF(a, b);
        else
            qnty = IFFY_LEN / -MAX_OF(a, b);

        p->info->cost += (int) (70 * factor * (qnty * qnty - 1.0));
        return false; // Continue iteration.
    }

    /**
     * When we reach here, we have a and b non-zero and opposite sign,
     * hence this half-edge will be split by the partition line.
     */

    p->info->splits++;
    p->info->cost += 100 * factor;

    /**
     * Check if the split point is very close to one end, which is quite an undesirable
     * situation (producing really short edges). This is perhaps _one_ source of those
     * darn slime trails. Hence the name "IFFY segs" and a rather hefty surcharge.
     */
    if(fa < IFFY_LEN || fb < IFFY_LEN)
    {
        p->info->iffy++;

        // The closer to the end, the higher the cost.
        qnty = IFFY_LEN / MIN_OF(fa, fb);
        p->info->cost += (int) (140 * factor * (qnty * qnty - 1.0));
    }
    return false; // Continue iteration.

#undef ADD_RIGHT
#undef ADD_LEFT
}

/**
 * @return  @c true= iff a "bad half-edge" was found early.
 */
static int evalPartitionWorker(SuperBlock* hEdgeList, bsp_hedge_t* part,
    int bestCost, evalinfo_t* info)
{
    evalpartitionworkerparams_t parm;
    int result, num;

    /**
     * Test the whole block against the partition line to quickly handle all the
     * half-edges within it at once. Only when the partition line intercepts the
     * box do we need to go deeper into it.
     */
    num = P_BoxOnLineSide3(SuperBlock_Bounds(hEdgeList), part->pSX, part->pSY, part->pDX,
                           part->pDY, part->pPerp, part->pLength, DIST_EPSILON);

    if(num < 0)
    {
        // Left.
        info->realLeft += SuperBlock_RealHEdgeCount(hEdgeList);
        info->miniLeft += SuperBlock_MiniHEdgeCount(hEdgeList);

        return false;
    }
    else if(num > 0)
    {
        // Right.
        info->realRight += SuperBlock_RealHEdgeCount(hEdgeList);
        info->miniRight += SuperBlock_MiniHEdgeCount(hEdgeList);

        return false;
    }

    // Check partition against all half-edges.
    parm.part = part;
    parm.bestCost = bestCost;
    parm.info = info;
    result = SuperBlock_IterateHEdges2(hEdgeList, evalPartitionWorker2, (void*)&parm);
    if(result) return true;

    // Handle sub-blocks recursively.
    for(num = 0; num < 2; ++num)
    {
        SuperBlock* child = SuperBlock_Child(hEdgeList, num);
        if(!child) continue;

        if(evalPartitionWorker(child, part, bestCost, info))
            return true;
    }

    // No "bad half-edge" was found. Good.
    return false;
}

/**
 * Evaluate a partition and determine the cost, taking into account the number of
 * splits and the difference between left and right.
 *
 * To be able to divide the nodes down, evalPartition must decide which is the best
 * half-edge to use as a nodeline. It does this by selecting the line with least
 * splits and has least difference of hald-edges on either side of it.
 *
 * @return  The computed cost, or a negative value if the edge should be skipped.
 */
static int evalPartition(SuperBlock* hEdgeList, bsp_hedge_t* part, int bestCost)
{
    evalinfo_t info;

    // Initialize info structure.
    info.cost   = 0;
    info.splits = 0;
    info.iffy   = 0;
    info.nearMiss  = 0;

    info.realLeft  = 0;
    info.realRight = 0;
    info.miniLeft  = 0;
    info.miniRight = 0;

    if(evalPartitionWorker(hEdgeList, part, bestCost, &info)) return -1;

    // Make sure there is at least one real hedge on each side.
    if(!info.realLeft || !info.realRight)
    {
        //DEBUG_Message(("Eval : No real half-edges on %s%sside\n",
        //              (info.realLeft? "" : "left "), (info.realRight? "" : "right ")));
        return -1;
    }

    // Increase cost by the difference between left and right.
    info.cost += 100 * ABS(info.realLeft - info.realRight);

    // Allow minihedge counts to affect the outcome.
    info.cost += 50 * ABS(info.miniLeft - info.miniRight);

    // Another little twist, here we show a slight preference for partition
    // lines that lie either purely horizontally or purely vertically.
    if(part->pDX != 0 && part->pDY != 0)
        info.cost += 25;

    //DEBUG_Message(("Eval %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d "
    //               "cost=%d.%02d\n", part, info.splits, info.iffy, info.nearMiss,
    //               info.realLeft, info.miniLeft, info.realRight, info.miniRight,
    //               info.cost / 100, info.cost % 100));

    return info.cost;
}

typedef struct {
    SuperBlock* hEdgeList;
    bsp_hedge_t** best;
    int* bestCost;
} pickhedgeworkerparams_t;

/**
 * @return  @c true= cancelled.
 */
static int pickHEdgeWorker2(bsp_hedge_t* part, void* parameters)
{
    pickhedgeworkerparams_t* p = (pickhedgeworkerparams_t*)parameters;
    int cost;

    //DEBUG_Message(("pickHEdgeWorker: %sHEdge %p sector=%d  (%1.1f,%1.1f) -> "
    //               "(%1.1f,%1.1f)\n", (part->lineDef? "" : "MINI"), part,
    //               (part->sector? part->sector->index : -1),
    //               part->v[0]->V_pos[VX], part->v[0]->V_pos[VY],
    //               part->v[1]->V_pos[VX], part->v[1]->V_pos[VY]));

    // Ignore minihedges as partition candidates.
    if(!part->lineDef) return false; // Continue iteration.

    // Only test half-edges from the same linedef once per round of
    // partition picking (they are collinear).
    if(part->lineDef->validCount == validCount) return false; // Continue iteration.

    part->lineDef->validCount = validCount;
    cost = evalPartition(p->hEdgeList, part, *p->bestCost);

    // Unsuitable or too costly?
    if(cost < 0 || cost >= *p->bestCost) return false; // Continue iteration.

    // We have a new better choice.
    (*p->bestCost) = cost;

    // Remember which half-edge.
    (*p->best) = part;

    return false; // Continue iteration.
}

static int pickHEdgeWorker(SuperBlock* partList, void* parameters)
{
    // Test each half-edge as a potential partition.
    return SuperBlock_IterateHEdges2(partList, pickHEdgeWorker2, parameters);
}

boolean BspBuilder::choosePartition(SuperBlock* hEdgeList, size_t /*depth*/, HPlane* partition)
{
    pickhedgeworkerparams_t parm;
    int bestCost = INT_MAX;
    bsp_hedge_t* best = NULL;

    //DEBUG_Message(("BspBuilder::choosePartition: Begun (depth %lu)\n", (unsigned long) depth));

    parm.hEdgeList = hEdgeList;
    parm.best = &best;
    parm.bestCost = &bestCost;

    validCount++;
    if(SuperBlock_Traverse2(hEdgeList, pickHEdgeWorker, (void*)&parm))
    {
        /// @kludge BspBuilder::buildNodes() will detect the cancellation.
        return false;
    }

    // Finished, return the best partition.
    if(best)
    {
        LineDef* line = best->lineDef;
        assert(line);

        //DEBUG_Message(("BspBuilder::choosePartition: Best has score %d.%02d  (%1.1f,%1.1f) -> "
        //               "(%1.1f,%1.1f)\n", bestCost / 100, bestCost % 100,
        //               best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
        //               best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]));

        // Reconfigure the partition for the next round of HEdge sorting.
        HPlane_SetXY(partition, line->L_v(best->side)->buildData.pos[VX],
                                line->L_v(best->side)->buildData.pos[VY], this);
        HPlane_SetDXY(partition, line->L_v(best->side^1)->buildData.pos[VX] - line->L_v(best->side)->buildData.pos[VX],
                                 line->L_v(best->side^1)->buildData.pos[VY] - line->L_v(best->side)->buildData.pos[VY], this);

        { HPlaneBuildInfo* info = HPlane_BuildInfo(partition);
        info->lineDef = best->lineDef;
        info->sourceLineDef = best->sourceLineDef;
        info->pDX = best->pDX;
        info->pDY = best->pDY;
        info->pSX = best->pSX;
        info->pSY = best->pSY;
        info->pPara = best->pPara;
        info->pPerp = best->pPerp;
        info->pLength = best->pLength;
        }

        return true;
    }

    //DEBUG_Message(("BspBuilder::choosePartition: No best found!\n"));
    return false;
}

static boolean lineDefHasSelfRef(LineDef* lineDef)
{
    return !!(lineDef->buildData.mlFlags & MLF_SELFREF);
}

HPlaneIntercept* BspBuilder::makeHPlaneIntersection(HPlane* hPlane, bsp_hedge_t* hEdge, int leftSide)
{
    HEdgeIntercept* hEdgeIntercept;
    const HPlaneBuildInfo* info;
    HPlaneIntercept* inter;
    Vertex* vertex;
    double distance;
    assert(hPlane && hEdge);

    // Already present on this edge?
    vertex = hEdge->v[leftSide?1:0];
    inter = hplaneInterceptByVertex(hPlane, vertex);
    if(inter) return inter;

    info = HPlane_BuildInfo(hPlane);
    distance = M_ParallelDist(info->pDX, info->pDY, info->pPara, info->pLength,
                              vertex->buildData.pos[VX], vertex->buildData.pos[VY]);

    hEdgeIntercept = newHEdgeIntercept(vertex, info, (hEdge->lineDef && lineDefHasSelfRef(hEdge->lineDef)));
    return HPlane_NewIntercept2(hPlane, distance, hEdgeIntercept);
}

HPlaneIntercept* BspBuilder::makeIntersection(HPlane* hPlane, bsp_hedge_t* hEdge, int leftSide)
{
    if(!hPlane || !hEdge)
        Con_Error("BspBuilder::makeHPlaneIntersection: Invalid arguments.");
    return makeHPlaneIntersection(hPlane, hEdge, leftSide);
}

/**
 * Calculate the intersection location between the current half-edge and the partition.
 * Takes advantage of some common situations like horizontal and vertical lines to
 * choose a 'nicer' intersection point.
 */
static __inline void calcIntersection(bsp_hedge_t* hEdge, const HPlaneBuildInfo* info,
    double perpC, double perpD, double* x, double* y)
{
    double ds;

    // Horizontal partition against vertical half-edge.
    if(info->pDY == 0 && hEdge->pDX == 0)
    {
        *x = hEdge->pSX;
        *y = info->pSY;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(info->pDX == 0 && hEdge->pDY == 0)
    {
        *x = info->pSX;
        *y = hEdge->pSY;
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(hEdge->pDX == 0)
        *x = hEdge->pSX;
    else
        *x = hEdge->pSX + (hEdge->pDX * ds);

    if(hEdge->pDY == 0)
        *y = hEdge->pSY;
    else
        *y = hEdge->pSY + (hEdge->pDY * ds);
}

void BspBuilder::divideHEdge(bsp_hedge_t* hEdge, HPlane* partition, SuperBlock* rightList,
    SuperBlock* leftList)
{
    const HPlaneBuildInfo* info = HPlane_BuildInfo(partition);
    bsp_hedge_t* newHEdge;
    double x, y;
    double a, b;

    // Get state of lines' relation to each other.
    a = M_PerpDist(info->pDX, info->pDY, info->pPerp, info->pLength, hEdge->pSX, hEdge->pSY);
    b = M_PerpDist(info->pDX, info->pDY, info->pPerp, info->pLength, hEdge->pEX, hEdge->pEY);

    if(hEdge->sourceLineDef == info->sourceLineDef)
        a = b = 0;

    // Check for being on the same line.
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makeIntersection(partition, hEdge, RIGHT);
        makeIntersection(partition, hEdge, LEFT);

        // This hedge runs along the same line as the partition. Check whether it goes in
        // the same direction or the opposite.
        if(hEdge->pDX * info->pDX + hEdge->pDY * info->pDY < 0)
        {
            SuperBlock_HEdgePush(leftList, hEdge);
        }
        else
        {
            SuperBlock_HEdgePush(rightList, hEdge);
        }

        return;
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        if(a < DIST_EPSILON)
            makeIntersection(partition, hEdge, RIGHT);
        else if(b < DIST_EPSILON)
            makeIntersection(partition, hEdge, LEFT);

        SuperBlock_HEdgePush(rightList, hEdge);
        return;
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        if(a > -DIST_EPSILON)
            makeIntersection(partition, hEdge, RIGHT);
        else if(b > -DIST_EPSILON)
            makeIntersection(partition, hEdge, LEFT);

        SuperBlock_HEdgePush(leftList, hEdge);
        return;
    }

    // When we reach here, we have a and b non-zero and opposite sign, hence this edge
    // will be split by the partition line.

    calcIntersection(hEdge, info, a, b, &x, &y);
    newHEdge = splitHEdge(hEdge, x, y);
    makeIntersection(partition, hEdge, LEFT);

    if(a < 0)
    {
        SuperBlock_HEdgePush(leftList,  hEdge);
        SuperBlock_HEdgePush(rightList, newHEdge);
    }
    else
    {
        SuperBlock_HEdgePush(rightList, hEdge);
        SuperBlock_HEdgePush(leftList,  newHEdge);
    }
}

typedef struct {
    SuperBlock* rights;
    SuperBlock* lefts;
    HPlane* hPlane;
    BspBuilder* builder;
} partitionhedgeworkerparams_t;

int C_DECL BspBuilder_PartitionHEdgeWorker(SuperBlock* superblock, void* parameters)
{
    partitionhedgeworkerparams_t* p = (partitionhedgeworkerparams_t*)parameters;
    bsp_hedge_t* hEdge;
    assert(p);

    while((hEdge = SuperBlock_HEdgePop(superblock)))
    {
        p->builder->divideHEdge(hEdge, p->hPlane, p->rights, p->lefts);
    }

    return false; // Continue iteration.
}

void BspBuilder::partitionHEdges(SuperBlock* hEdgeList, SuperBlock* rights, SuperBlock* lefts,
    HPlane* hPlane)
{
    partitionhedgeworkerparams_t parm;

    parm.rights = rights;
    parm.lefts = lefts;
    parm.hPlane = hPlane;
    parm.builder = this;
    SuperBlock_Traverse2(hEdgeList, BspBuilder_PartitionHEdgeWorker, (void*)&parm);

    // Sanity checks...
    if(!SuperBlock_TotalHEdgeCount(rights))
        Con_Error("BspBuilder::partitionHEdges: Separated half-edge has no right side.");

    if(!SuperBlock_TotalHEdgeCount(lefts))
        Con_Error("BspBuilder::partitionHEdges: Separated half-edge has no left side.");

    addMiniHEdges(hPlane, rights, lefts);
}

static int createBSPLeafWorker(SuperBlock* superblock, void* parameters)
{
    bspleafdata_t* leaf = (bspleafdata_t*) parameters;
    bsp_hedge_t* hEdge;
    assert(leaf);

    while((hEdge = SuperBlock_HEdgePop(superblock)))
    {
        // Link it into head of the leaf's list.
        hEdge->nextInLeaf = leaf->hEdges;
        leaf->hEdges = hEdge;
    }

    return false; // Continue iteration.
}

/**
 * Create a new leaf from a list of half-edges.
 */
bspleafdata_t* BspBuilder::createBSPLeaf(SuperBlock* hEdgeList)
{
    bspleafdata_t* leaf = newLeaf();

    // Link the half-edges into the new leaf.
    SuperBlock_Traverse2(hEdgeList, createBSPLeafWorker, leaf);

    return leaf;
}

boolean BspBuilder::buildNodes(SuperBlock* superblock, binarytree_t** parent, size_t depth,
    HPlane* hPlane)
{
    SuperBlockmap* hEdgeSet[2];
    binarytree_t* subTree;
    bspleafdata_t* leaf;
    BspNode* node;
    boolean builtOK = false;

    *parent = NULL;

    /*DEBUG_Message(("Build: Begun @ %lu\n", (unsigned long) depth));
#if _DEBUG
    BSP_PrintSuperBlockHEdges(superblock);
#endif*/

    // Pick the next partition to use.
    if(!choosePartition(superblock, depth, hPlane))
    {
        // No partition required, already convex.
        //DEBUG_Message(("BspBuilder::buildNodes: Convex.\n"));

        leaf = createBSPLeaf(superblock);
        *parent = BinaryTree_Create(leaf);
        return true;
    }

    //DEBUG_Message(("BspBuilder::buildNodes: Partition %p (%1.0f,%1.0f) -> (%1.0f,%1.0f).\n",
    //               best, best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
    //               best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]));

    // Create left and right super blockmaps.
    /// @todo There should be no need to construct entirely independent
    ///       data structures to contain these HEdge subsets.
    // Copy the bounding box of the edge list to the superblocks.
    hEdgeSet[RIGHT] = SuperBlockmap_New(SuperBlock_Bounds(superblock));
    hEdgeSet[LEFT]  = SuperBlockmap_New(SuperBlock_Bounds(superblock));

    // Divide the half-edges into two lists: left & right.
    partitionHEdges(superblock, SuperBlockmap_Root(hEdgeSet[RIGHT]),
                                SuperBlockmap_Root(hEdgeSet[LEFT]), hPlane);
    HPlane_Clear(hPlane, this);

    node = (BspNode*)M_Calloc(sizeof *node);
    *parent = BinaryTree_Create(node);

    SuperBlockmap_FindHEdgeBounds(hEdgeSet[LEFT],  &node->aaBox[LEFT]);
    SuperBlockmap_FindHEdgeBounds(hEdgeSet[RIGHT], &node->aaBox[RIGHT]);

    node->partition.x = HPlane_X(hPlane);
    node->partition.y = HPlane_Y(hPlane);
    node->partition.dX = HPlane_DX(hPlane);
    node->partition.dY = HPlane_DY(hPlane);

    builtOK = buildNodes(SuperBlockmap_Root(hEdgeSet[RIGHT]), &subTree, depth + 1, hPlane);
    BinaryTree_SetChild(*parent, RIGHT, subTree);
    SuperBlockmap_Delete(hEdgeSet[RIGHT]);

    if(builtOK)
    {
        builtOK = buildNodes(SuperBlockmap_Root(hEdgeSet[LEFT]), &subTree, depth + 1, hPlane);
        BinaryTree_SetChild(*parent, LEFT, subTree);
    }

    SuperBlockmap_Delete(hEdgeSet[LEFT]);

    return builtOK;
}

#if _DEBUG
static int printSuperBlockHEdgesWorker2(bsp_hedge_t* hEdge, void* /*parameters*/)
{
    Con_Message("Build: %s %p sector=%d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                (hEdge->lineDef? "NORM" : "MINI"), hEdge,
                hEdge->sector->buildData.index,
                hEdge->v[0]->buildData.pos[VX], hEdge->v[0]->buildData.pos[VY],
                hEdge->v[1]->buildData.pos[VX], hEdge->v[1]->buildData.pos[VY]);
    return false; // Continue iteration.
}

static int printSuperBlockHEdgesWorker(SuperBlock* superblock, void* parameters)
{
    return SuperBlock_IterateHEdges2(superblock, printSuperBlockHEdgesWorker2, parameters);
}

void BSP_PrintSuperBlockHEdges(SuperBlock* superblock)
{
    if(!superblock) return;
    SuperBlock_Traverse(superblock, printSuperBlockHEdgesWorker);
}
#endif
