/**
 * @file node.cpp
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
#include "de_play.h"
#include "m_misc.h"
#include "m_binarytree.h"

#include "bspbuilder/intersection.hh"
#include "bspbuilder/superblockmap.hh"
#include "bspbuilder/bspbuilder.hh"

using namespace de;

typedef struct partitioncost_s {
    int total;
    int splits;
    int iffy;
    int nearMiss;
    int realLeft;
    int realRight;
    int miniLeft;
    int miniRight;

    struct partitioncost_s* initialize()
    {
        total = 0;
        splits = 0;
        iffy = 0;
        nearMiss = 0;
        realLeft = 0;
        realRight = 0;
        miniLeft = 0;
        miniRight = 0;
        return this;
    }

    struct partitioncost_s* applyDelta(const struct partitioncost_s& other)
    {
        total += other.total;
        splits += other.splits;
        iffy += other.iffy;
        nearMiss += other.nearMiss;
        realLeft += other.realLeft;
        realRight += other.realRight;
        miniLeft += other.miniLeft;
        miniRight += other.miniRight;
        return this;
    }
} PartitionCost;

// Used when sorting BSP leaf half-edges by angle around midpoint.
static size_t hedgeSortBufSize;
static HEdge** hedgeSortBuf;

#if _DEBUG
void BSP_PrintSuperBlockhedges(SuperBlock* superblock);
#endif

static __inline int pointOnhedgeSide(double x, double y, const HEdge* part)
{
    return P_PointOnLinedefSide2(x, y, part->buildData.info.pDX, part->buildData.info.pDY,
                                 part->buildData.info.pPerp, part->buildData.info.pLength,
                                 DIST_EPSILON);
}

static boolean getAveragedCoords(HEdge* headPtr, double* x, double* y)
{
    size_t total = 0;
    double avg[2];
    HEdge* cur;

    if(!x || !y) return false;

    avg[VX] = avg[VY] = 0;

    for(cur = headPtr; cur; cur = cur->buildData.nextInLeaf)
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
static void sorthedgesByAngleAroundPoint(HEdge** hedges, size_t total,
    double x, double y)
{
    size_t i;

    i = 0;
    while(i + 1 < total)
    {
        HEdge* a = hedges[i];
        HEdge* b = hedges[i+1];
        double angle1, angle2;

        angle1 = M_SlopeToAngle(a->v[0]->buildData.pos[VX] - x,
                                a->v[0]->buildData.pos[VY] - y);
        angle2 = M_SlopeToAngle(b->v[0]->buildData.pos[VX] - x,
                                b->v[0]->buildData.pos[VY] - y);

        if(angle1 + ANG_EPSILON < angle2)
        {
            // Swap them.
            hedges[i] = b;
            hedges[i + 1] = a;

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
static void clockwiseOrder(HEdge** headPtr, size_t num, double x, double y)
{
    HEdge* hedge;
    size_t i;

    // Insert ptrs to the hedges into the sort buffer.
    for(hedge = *headPtr, i = 0; hedge; hedge = hedge->buildData.nextInLeaf, ++i)
    {
        hedgeSortBuf[i] = hedge;
    }
    hedgeSortBuf[i] = NULL; // Terminate.

    if(i != num)
        Con_Error("clockwiseOrder: Miscounted?");

    sorthedgesByAngleAroundPoint(hedgeSortBuf, num, x, y);

    // Re-link the half-edge list in the order of the sorted array.
    *headPtr = NULL;
    for(i = 0; i < num; ++i)
    {
        size_t idx = (num - 1) - i;
        size_t j = idx % num;

        hedgeSortBuf[j]->buildData.nextInLeaf = *headPtr;
        *headPtr = hedgeSortBuf[j];
    }

/*#if _DEBUG
    Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

    for(hedge = sub->hedges; hedge; hedge = hedge->next)
    {
        double angle = M_SlopeToAngle(hedge->v[0]->V_pos[VX] - x,
                                       hedge->v[0]->V_pos[VY] - y);

        Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                    hedge, angle, hedge->v[0]->V_pos[VX], hedge->v[0]->V_pos[VY],
                    hedge->v[1]->V_pos[VX], hedge->v[1]->V_pos[VY]);
    }
#endif*/
}

static void sanityCheckClosed(const BspLeaf* leaf)
{
    int total = 0, gaps = 0;
    HEdge* cur, *next;

    for(cur = leaf->buildData.hedges; cur; cur = cur->buildData.nextInLeaf)
    {
        next = (cur->buildData.nextInLeaf? cur->buildData.nextInLeaf : leaf->buildData.hedges);

        if(cur->v[1]->buildData.pos[VX] != next->v[0]->buildData.pos[VX] ||
           cur->v[1]->buildData.pos[VY] != next->v[0]->buildData.pos[VY])
        {
            gaps++;
        }

        total++;
    }

    if(gaps > 0)
    {
        VERBOSE( Con_Message("hedge list for leaf #%p is not closed (%d gaps, %d half-edges)\n", leaf, gaps, total) );

/*#if _DEBUG
    for(cur = leaf->buildData.hedges; cur; cur = cur->next)
    {
        Con_Message("  half-edge %p  (%1.1f,%1.1f) --> (%1.1f,%1.1f)\n", cur,
                    cur->v[0]->pos[VX], cur->v[0]->pos[VY],
                    cur->v[1]->pos[VX], cur->v[1]->pos[VY]);
    }
#endif*/
    }
}

static void sanityCheckSameSector(const BspLeaf* leaf)
{
    HEdge* cur, *compare;

    // Find a suitable half-edge for comparison.
    for(compare = leaf->buildData.hedges; compare; compare = compare->buildData.nextInLeaf)
    {
        if(compare->sector) break;
    }

    if(!compare) return;

    for(cur = compare->buildData.nextInLeaf; cur; cur = cur->buildData.nextInLeaf)
    {
        if(!cur->sector) continue;

        if(cur->sector == compare->sector) continue;

        // Prevent excessive number of warnings.
        if(compare->sector->buildData.warnedFacing == cur->sector->buildData.index)
            continue;

        compare->sector->buildData.warnedFacing = cur->sector->buildData.index;

        if(verbose >= 1)
        {
            if(cur->buildData.info.lineDef)
                Con_Message("Sector #%d has sidedef facing #%d (line #%d).\n",
                            compare->sector->buildData.index, cur->sector->buildData.index,
                            cur->buildData.info.lineDef->buildData.index);
            else
                Con_Message("Sector #%d has sidedef facing #%d.\n",
                            compare->sector->buildData.index, cur->sector->buildData.index);
        }
    }
}

static boolean sanityCheckHasRealhedge(const BspLeaf* leaf)
{
    HEdge* cur;
    for(cur = leaf->buildData.hedges; cur; cur = cur->buildData.nextInLeaf)
    {
        if(cur->buildData.info.lineDef) return true;
    }
    return false;
}

static void renumberLeafhedges(BspLeaf* leaf, uint* curIndex)
{
    uint n;
    HEdge* cur;

    n = 0;
    for(cur = leaf->buildData.hedges; cur; cur = cur->buildData.nextInLeaf)
    {
        cur->buildData.index = *curIndex;
        (*curIndex)++;
        n++;
    }
}

static void preparehedgeSortBuffer(size_t numhedges)
{
    // Do we need to enlarge our sort buffer?
    if(numhedges + 1 > hedgeSortBufSize)
    {
        hedgeSortBufSize = numhedges + 1;
        hedgeSortBuf = (HEdge**)M_Realloc(hedgeSortBuf, hedgeSortBufSize * sizeof(*hedgeSortBuf));
    }
}

static int C_DECL clockwiseLeaf(BinaryTree* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        BspLeaf* leaf = (BspLeaf*) BinaryTree_UserData(tree);
        double midPoint[2] = { 0, 0 };
        HEdge* hedge;
        size_t total;

        getAveragedCoords(leaf->buildData.hedges, &midPoint[VX], &midPoint[VY]);

        // Count half-edges.
        total = 0;
        for(hedge = leaf->buildData.hedges; hedge; hedge = hedge->buildData.nextInLeaf)
            total++;

        // Ensure the sort buffer is large enough.
        preparehedgeSortBuffer(total);

        clockwiseOrder(&leaf->buildData.hedges, total, midPoint[VX], midPoint[VY]);
        renumberLeafhedges(leaf, (uint*)data);

        // Do some sanity checks.
        sanityCheckClosed(leaf);
        sanityCheckSameSector(leaf);
        if(!sanityCheckHasRealhedge(leaf))
        {
            Con_Error("BSP Leaf #%p has no linedef-linked half-edge!", leaf);
        }
    }

    return false; // Continue traversal.
}

void BspBuilder::windLeafs(BinaryTree* rootNode)
{
    uint curIndex;

    hedgeSortBufSize = 0;
    hedgeSortBuf = NULL;

    curIndex = 0;
    BinaryTree_PostOrder(rootNode, clockwiseLeaf, &curIndex);

    // Free temporary storage.
    if(hedgeSortBuf)
    {
        M_Free(hedgeSortBuf);
        hedgeSortBuf = NULL;
    }
}

static void evalPartitionCostForHEdge(const BspHEdgeInfo* partInfo,
    int costFactorMultiplier, const HEdge* hedge, PartitionCost& cost)
{
#define ADD_LEFT()  \
    if (hedge->buildData.info.lineDef) cost.realLeft += 1;  \
    else                               cost.miniLeft += 1;  \

#define ADD_RIGHT()  \
    if (hedge->buildData.info.lineDef) cost.realRight += 1;  \
    else                               cost.miniRight += 1;  \

    double qnty, a, b, fa, fb;
    assert(hedge);

    // Get state of lines' relation to each other.
    if(hedge->buildData.info.sourceLineDef == partInfo->sourceLineDef)
    {
        a = b = fa = fb = 0;
    }
    else
    {
        a = M_PerpDist(partInfo->pDX, partInfo->pDY, partInfo->pPerp, partInfo->pLength,
                       hedge->buildData.info.pSX, hedge->buildData.info.pSY);
        b = M_PerpDist(partInfo->pDX, partInfo->pDY, partInfo->pPerp, partInfo->pLength,
                       hedge->buildData.info.pEX, hedge->buildData.info.pEY);

        fa = fabs(a);
        fb = fabs(b);
    }

    // hedge for being on the same line.
    if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
    {
        // This half-edge runs along the same line as the partition.
        // hedge whether it goes in the same direction or the opposite.
        if(hedge->buildData.info.pDX * partInfo->pDX + hedge->buildData.info.pDY * partInfo->pDY < 0)
        {
            ADD_LEFT();
        }
        else
        {
            ADD_RIGHT();
        }

        return;
    }

    // hedge for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        ADD_RIGHT();

        // hedge for a near miss.
        if((a >= IFFY_LEN && b >= IFFY_LEN) ||
           (a <= DIST_EPSILON && b >= IFFY_LEN) ||
           (b <= DIST_EPSILON && a >= IFFY_LEN))
        {
            return;
        }

        cost.nearMiss++;

        /**
         * Near misses are bad, since they have the potential to cause really short
         * minihedges to be created in future processing. Thus the closer the near
         * miss, the higher the cost.
         */

        if(a <= DIST_EPSILON || b <= DIST_EPSILON)
            qnty = IFFY_LEN / MAX_OF(a, b);
        else
            qnty = IFFY_LEN / MIN_OF(a, b);

        cost.total += (int) (100 * costFactorMultiplier * (qnty * qnty - 1.0));
        return;
    }

    // hedge for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        ADD_LEFT();

        // hedge for a near miss.
        if((a <= -IFFY_LEN && b <= -IFFY_LEN) ||
           (a >= -DIST_EPSILON && b <= -IFFY_LEN) ||
           (b >= -DIST_EPSILON && a <= -IFFY_LEN))
        {
            return;
        }

        cost.nearMiss++;

        // The closer the miss, the higher the cost (see note above).
        if(a >= -DIST_EPSILON || b >= -DIST_EPSILON)
            qnty = IFFY_LEN / -MIN_OF(a, b);
        else
            qnty = IFFY_LEN / -MAX_OF(a, b);

        cost.total += (int) (70 * costFactorMultiplier * (qnty * qnty - 1.0));
        return;
    }

    /**
     * When we reach here, we have a and b non-zero and opposite sign,
     * hence this half-edge will be split by the partition line.
     */

    cost.splits++;
    cost.total += 100 * costFactorMultiplier;

    /**
     * If the split point is very close to one end, which is quite an undesirable
     * situation (producing really short edges). This is perhaps _one_ source of those
     * darn slime trails. Hence the name "IFFY segs" and a rather hefty surcharge.
     */
    if(fa < IFFY_LEN || fb < IFFY_LEN)
    {
        cost.iffy++;

        // The closer to the end, the higher the cost.
        qnty = IFFY_LEN / MIN_OF(fa, fb);
        cost.total += (int) (140 * costFactorMultiplier * (qnty * qnty - 1.0));
    }

#undef ADD_RIGHT
#undef ADD_LEFT
}

/**
 * @param hedgeInfo  Info about the candidate half-edge to be evaluated.
 * @param bestCost  Running cost total result from the best candidate half-edge so far.
 * @param cost  PartitionCost analysis to be completed for this candidate. Must have
 *              been initialized prior to calling this.
 * @return  @c true= iff a "bad half-edge" was found early.
 */
static int evalPartitionCostForSuperBlock(SuperBlock& block, int splitCostFactor,
    const BspHEdgeInfo* hedgeInfo, int bestCost, PartitionCost& cost)
{
    int num;

    /**
     * Test the whole block against the partition line to quickly handle all the
     * half-edges within it at once. Only when the partition line intercepts the
     * box do we need to go deeper into it.
     */
    num = P_BoxOnLineSide3(&block.bounds(), hedgeInfo->pSX, hedgeInfo->pSY,
                           hedgeInfo->pDX, hedgeInfo->pDY, hedgeInfo->pPerp,
                           hedgeInfo->pLength, DIST_EPSILON);

    if(num < 0)
    {
        // Left.
        cost.realLeft += block.realHEdgeCount();
        cost.miniLeft += block.miniHEdgeCount();

        return false;
    }
    else if(num > 0)
    {
        // Right.
        cost.realRight += block.realHEdgeCount();
        cost.miniRight += block.miniHEdgeCount();

        return false;
    }

    // Check partition against all half-edges.
    PartitionCost costDelta;
    for(SuperBlock::HEdges::const_iterator it = block.hedgesBegin();
        it != block.hedgesEnd(); ++it)
    {
        // Catch "bad half-edges" early on.
        if(cost.total > bestCost) return true; // Stop iteration.

        // Evaluate the cost delta for this hedge.
        costDelta.initialize();
        evalPartitionCostForHEdge(hedgeInfo, splitCostFactor, *it, costDelta);

        // Merge cost result into the cummulative total.
        cost.applyDelta(costDelta);
    }

    // Handle sub-blocks recursively.
    if(block.hasRight() &&
       evalPartitionCostForSuperBlock(block.right(), splitCostFactor,
                                      hedgeInfo, bestCost, cost)) return true;

    if(block.hasLeft() &&
       evalPartitionCostForSuperBlock(block.left(), splitCostFactor,
                                      hedgeInfo, bestCost, cost)) return true;

    // No "bad half-edge" was found. Good.
    return false;
}

/**
 * Evaluate a partition and determine the cost, taking into account the
 * number of splits and the difference between left and right.
 *
 * To be able to divide the nodes down, evalPartition must decide which
 * is the best half-edge to use as a nodeline. It does this by selecting
 * the line with least splits and has least difference of hald-edges on
 * either side of it.
 *
 * @return  The computed cost, or a negative value (should be skipped).
 */
static int evalPartition(SuperBlock& block, int splitCostFactor,
    BspHEdgeInfo* hedgeInfo, int bestCost)
{
    PartitionCost cost;

    cost.initialize();
    if(evalPartitionCostForSuperBlock(block, splitCostFactor, hedgeInfo,
                                      bestCost, cost)) return -1;

    // Make sure there is at least one real half-edge on each side.
    if(!cost.realLeft || !cost.realRight)
    {
        //DEBUG_Message(("Eval : No real half-edges on %s%sside\n",
        //              (cost.realLeft? "" : "left "), (cost.realRight? "" : "right ")));
        return -1;
    }

    // Increase cost by the difference between left and right.
    cost.total += 100 * ABS(cost.realLeft - cost.realRight);

    // Allow minihedge counts to affect the outcome.
    cost.total += 50 * ABS(cost.miniLeft - cost.miniRight);

    // Another little twist, here we show a slight preference for partition
    // lines that lie either purely horizontally or purely vertically.
    if(!FEQUAL(hedgeInfo->pDX, 0) && !FEQUAL(hedgeInfo->pDY, 0))
        cost.total += 25;

    //DEBUG_Message(("Eval %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d "
    //               "cost=%d.%02d\n", hedgeInfo, cost.splits, cost.iffy, cost.nearMiss,
    //               cost.realLeft, cost.miniLeft, cost.realRight, cost.miniRight,
    //               cost.total / 100, cost.total % 100));

    return cost.total;
}

typedef struct {
    SuperBlock* hedgeList;
    int splitCostFactor;
    HEdge* best;
    int bestCost;
} choosehedgefromsuperblockparams_t;

static int chooseHEdgeFromSuperBlock(SuperBlock* partList, void* parameters)
{
    choosehedgefromsuperblockparams_t* p = (choosehedgefromsuperblockparams_t*)parameters;

    // Test each half-edge as a potential partition.
    for(SuperBlock::HEdges::const_iterator it = partList->hedgesBegin();
        it != partList->hedgesEnd(); ++it)
    {
        HEdge* hedge = *it;

        //DEBUG_Message(("chooseHEdgeFromSuperBlock: %shedge %p sector=%d  (%1.1f,%1.1f) -> "
        //               "(%1.1f,%1.1f)\n", (lineDef? "" : "MINI"), hedge,
        //               (hedge->buildData.sector? hedge->buildData.sector->index : -1),
        //               hedge->v[0]->V_pos[VX], hedge->v[0]->V_pos[VY],
        //               hedge->v[1]->V_pos[VX], hedge->v[1]->V_pos[VY]));

        // "Mini-hedges" are never potential candidates.
        LineDef* lineDef = hedge->buildData.info.lineDef;
        if(!lineDef) continue;

        // Only test half-edges from the same linedef once per round of
        // partition picking (they are collinear).
        if(lineDef->validCount == validCount) continue;
        lineDef->validCount = validCount;

        // Unsuitable or too costly?
        int cost = evalPartition(*p->hedgeList, p->splitCostFactor, &hedge->buildData.info, p->bestCost);
        if(cost >= 0 && cost < p->bestCost)
        {
            // We have a new better choice.
            p->bestCost = cost;

            // Remember which half-edge.
            p->best = hedge;
        }
    }
    return false; // Continue iteration.
}

boolean BspBuilder::choosePartition(SuperBlock* hedgeList, size_t /*depth*/, HPlane* partition)
{
    choosehedgefromsuperblockparams_t parm;

    //DEBUG_Message(("BspBuilder::choosePartition: Begun (depth %lu)\n", (unsigned long) depth));

    parm.hedgeList = hedgeList;
    parm.splitCostFactor = splitCostFactor;
    parm.best = NULL;
    parm.bestCost = DDMAXINT;

    validCount++;
    if(hedgeList->traverse(chooseHEdgeFromSuperBlock, (void*)&parm))
    {
        /// @kludge BspBuilder::buildNodes() will detect the cancellation.
        return false;
    }

    // Finished, return the best partition.
    HEdge* best = parm.best;
    if(best)
    {
        LineDef* lineDef = best->buildData.info.lineDef;

        // This must not be a "mini hedge".
        assert(lineDef);

        //DEBUG_Message(("BspBuilder::choosePartition: Best has score %d.%02d  (%1.1f,%1.1f) -> "
        //               "(%1.1f,%1.1f)\n", bestCost / 100, bestCost % 100,
        //               best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
        //               best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]));

        // Reconfigure the partition for the next round of hedge sorting.
        partition->setXY(lineDef->L_v(best->side)->buildData.pos[VX],
                         lineDef->L_v(best->side)->buildData.pos[VY]);
        partition->setDXY(lineDef->L_v(best->side^1)->buildData.pos[VX] - lineDef->L_v(best->side)->buildData.pos[VX],
                          lineDef->L_v(best->side^1)->buildData.pos[VY] - lineDef->L_v(best->side)->buildData.pos[VY]);

        BspHEdgeInfo* info = partition->partitionHEdgeInfo();
        memcpy(info, &best->buildData.info, sizeof(*info));

        return true;
    }

    //DEBUG_Message(("BspBuilder::choosePartition: No best found!\n"));
    return false;
}

static boolean lineDefHasSelfRef(LineDef* lineDef)
{
    return !!(lineDef->buildData.mlFlags & MLF_SELFREF);
}

enum {
   RIGHT = 0,
   LEFT
};

const HPlaneIntercept* BspBuilder::makeHPlaneIntersection(HPlane* hplane, HEdge* hedge, int leftSide)
{
    HEdgeIntercept* hedgeIntercept;
    const BspHEdgeInfo* info;
    const HPlaneIntercept* inter;
    Vertex* vertex;
    double distance;
    assert(hplane && hedge);

    // Already present on this edge?
    vertex = hedge->v[leftSide?1:0];
    inter = hplaneInterceptByVertex(hplane, vertex);
    if(inter) return inter;

    info = hplane->partitionHEdgeInfo();
    distance = M_ParallelDist(info->pDX, info->pDY, info->pPara, info->pLength,
                              vertex->buildData.pos[VX], vertex->buildData.pos[VY]);

    hedgeIntercept = newHEdgeIntercept(vertex, info, (hedge->buildData.info.lineDef && lineDefHasSelfRef(hedge->buildData.info.lineDef)));
    return hplane->newIntercept(distance, hedgeIntercept);
}

const HPlaneIntercept* BspBuilder::makeIntersection(HPlane* hplane, HEdge* hedge, int leftSide)
{
    if(!hplane || !hedge)
        Con_Error("BspBuilder::makeHPlaneIntersection: Invalid arguments.");
    return makeHPlaneIntersection(hplane, hedge, leftSide);
}

/**
 * Calculate the intersection location between the current half-edge and the partition.
 * Takes advantage of some common situations like horizontal and vertical lines to
 * choose a 'nicer' intersection point.
 */
static __inline void calcIntersection(BspHEdgeInfo* hedge, const BspHEdgeInfo* other,
    double perpC, double perpD, double* x, double* y)
{
    double ds;

    // Horizontal partition against vertical half-edge.
    if(other->pDY == 0 && hedge->pDX == 0)
    {
        *x = hedge->pSX;
        *y = other->pSY;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(other->pDX == 0 && hedge->pDY == 0)
    {
        *x = other->pSX;
        *y = hedge->pSY;
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(hedge->pDX == 0)
        *x = hedge->pSX;
    else
        *x = hedge->pSX + (hedge->pDX * ds);

    if(hedge->pDY == 0)
        *y = hedge->pSY;
    else
        *y = hedge->pSY + (hedge->pDY * ds);
}

void BspBuilder::divideHEdge(HEdge* hedge, HPlane* partition, SuperBlock* rightList,
    SuperBlock* leftList)
{
    const BspHEdgeInfo* info = partition->partitionHEdgeInfo();
    HEdge* newhedge;
    double x, y;
    double a, b;

    // Get state of lines' relation to each other.
    a = M_PerpDist(info->pDX, info->pDY, info->pPerp, info->pLength, hedge->buildData.info.pSX, hedge->buildData.info.pSY);
    b = M_PerpDist(info->pDX, info->pDY, info->pPerp, info->pLength, hedge->buildData.info.pEX, hedge->buildData.info.pEY);

    if(hedge->buildData.info.sourceLineDef == info->sourceLineDef)
        a = b = 0;

    // Check for being on the same line.
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makeIntersection(partition, hedge, RIGHT);
        makeIntersection(partition, hedge, LEFT);

        // This hedge runs along the same line as the partition. Check whether it goes in
        // the same direction or the opposite.
        if(hedge->buildData.info.pDX * info->pDX + hedge->buildData.info.pDY * info->pDY < 0)
        {
            leftList->hedgePush(hedge);
        }
        else
        {
            rightList->hedgePush(hedge);
        }
        return;
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        if(a < DIST_EPSILON)
            makeIntersection(partition, hedge, RIGHT);
        else if(b < DIST_EPSILON)
            makeIntersection(partition, hedge, LEFT);

        rightList->hedgePush(hedge);
        return;
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        if(a > -DIST_EPSILON)
            makeIntersection(partition, hedge, RIGHT);
        else if(b > -DIST_EPSILON)
            makeIntersection(partition, hedge, LEFT);

        leftList->hedgePush(hedge);
        return;
    }

    // When we reach here, we have a and b non-zero and opposite sign, hence this edge
    // will be split by the partition line.

    calcIntersection(&hedge->buildData.info, info, a, b, &x, &y);
    newhedge = splitHEdge(hedge, x, y);
    makeIntersection(partition, hedge, LEFT);

    if(a < 0)
    {
        leftList->hedgePush(hedge);
        rightList->hedgePush(newhedge);
    }
    else
    {
        rightList->hedgePush(hedge);
        leftList->hedgePush(newhedge);
    }
}

typedef struct {
    SuperBlock* rights;
    SuperBlock* lefts;
    HPlane* hplane;
    BspBuilder* builder;
} partitionhedgeworkerparams_t;

int C_DECL BspBuilder_PartitionHEdgeWorker(SuperBlock* superblock, void* parameters)
{
    partitionhedgeworkerparams_t* p = (partitionhedgeworkerparams_t*)parameters;
    HEdge* hedge;
    assert(p);

    while((hedge = superblock->hedgePop()))
    {
        p->builder->divideHEdge(hedge, p->hplane, p->rights, p->lefts);
    }

    return false; // Continue iteration.
}

void BspBuilder::partitionHEdges(SuperBlock* hedgeList, SuperBlock* rights, SuperBlock* lefts,
    HPlane* hplane)
{
    partitionhedgeworkerparams_t parm;

    parm.rights = rights;
    parm.lefts = lefts;
    parm.hplane = hplane;
    parm.builder = this;
    hedgeList->traverse(BspBuilder_PartitionHEdgeWorker, (void*)&parm);

    // Sanity checks...
    if(!rights->totalHEdgeCount())
        Con_Error("BspBuilder::partitionhedges: Separated half-edge has no right side.");

    if(!lefts->totalHEdgeCount())
        Con_Error("BspBuilder::partitionhedges: Separated half-edge has no left side.");

    addMiniHEdges(hplane, rights, lefts);
}

static int createBSPLeafWorker(SuperBlock* superblock, void* parameters)
{
    BspLeaf* leaf = (BspLeaf*) parameters;
    HEdge* hedge;
    assert(leaf);

    while((hedge = superblock->hedgePop()))
    {
        // Link it into head of the leaf's list.
        hedge->buildData.nextInLeaf = leaf->buildData.hedges;
        leaf->buildData.hedges = hedge;
    }

    return false; // Continue iteration.
}

/**
 * Create a new leaf from a list of half-edges.
 */
BspLeaf* BspBuilder::createBSPLeaf(SuperBlock* hedgeList)
{
    BspLeaf* leaf = BspLeaf_New();

    // Link the half-edges into the new leaf.
    hedgeList->traverse(createBSPLeafWorker, leaf);

    return leaf;
}

boolean BspBuilder::buildNodes(SuperBlock* superblock, BinaryTree** parent, size_t depth,
    HPlane* hplane)
{
    SuperBlockmap* rightHEdges, *leftHEdges;
    AABoxf rightHEdgesBounds, leftHEdgesBounds;
    BinaryTree* subTree;
    BspLeaf* leaf;
    BspNode* node;
    boolean builtOK = false;

    *parent = NULL;

    /*DEBUG_Message(("Build: Begun @ %lu\n", (unsigned long) depth));
#if _DEBUG
    BSP_PrintSuperBlockhedges(superblock);
#endif*/

    // Pick the next partition to use.
    if(!choosePartition(superblock, depth, hplane))
    {
        // No partition required, already convex.
        //DEBUG_Message(("BspBuilder::buildNodes: Convex.\n"));

        leaf = createBSPLeaf(superblock);
        *parent = BinaryTree_NewWithUserData(leaf);
        return true;
    }

    //DEBUG_Message(("BspBuilder::buildNodes: Partition %p (%1.0f,%1.0f) -> (%1.0f,%1.0f).\n",
    //               best, best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
    //               best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]));

    // Create left and right super blockmaps.
    /// @todo There should be no need to construct entirely independent
    ///       data structures to contain these hedge subsets.
    // Copy the bounding box of the edge list to the superblocks.
    rightHEdges = new SuperBlockmap(superblock->bounds());
    leftHEdges  = new SuperBlockmap(superblock->bounds());

    // Divide the half-edges into two lists: left & right.
    partitionHEdges(superblock, rightHEdges->root(), leftHEdges->root(), hplane);
    hplane->clear();

    rightHEdges->findHEdgeBounds(rightHEdgesBounds);
    leftHEdges->findHEdgeBounds(leftHEdgesBounds);

    node = BspNode_New(hplane->origin(), hplane->angle());
    BspNode_SetRightBounds(node, &rightHEdgesBounds);
    BspNode_SetLeftBounds(node, &leftHEdgesBounds);
    *parent = BinaryTree_NewWithUserData(node);

    builtOK = buildNodes(rightHEdges->root(), &subTree, depth + 1, hplane);
    BinaryTree_SetRight(*parent, subTree);
    delete rightHEdges;

    if(builtOK)
    {
        builtOK = buildNodes(leftHEdges->root(), &subTree, depth + 1, hplane);
        BinaryTree_SetLeft(*parent, subTree);
    }

    delete leftHEdges;

    return builtOK;
}

#if _DEBUG
static void printHEdge(HEdge* hedge)
{
    Con_Message("Build: %s %p sector=%d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                (hedge->buildData.info.lineDef? "NORM" : "MINI"), hedge,
                hedge->sector->buildData.index,
                hedge->v[0]->buildData.pos[VX], hedge->v[0]->buildData.pos[VY],
                hedge->v[1]->buildData.pos[VX], hedge->v[1]->buildData.pos[VY]);
}

static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/)
{
    for(SuperBlock::HEdges::const_iterator it = block->hedgesBegin();
        it != block->hedgesEnd(); ++it)
    {
        HEdge* hedge = *it;
        printHEdge(hedge);
    }
    return false; // Continue iteration.
}

void BSP_PrintSuperBlockhedges(SuperBlock* block)
{
    if(!block) return;
    block->traverse(printSuperBlockHEdgesWorker);
}
#endif
