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

#include <cmath>
#include <vector>
#include <algorithm>

#include <de/Log>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "m_misc.h"
#include "m_binarytree.h"

#include "bspbuilder/bspbuilder.hh"
#include "bspbuilder/hplane.h"
#include "bspbuilder/superblockmap.h"

using namespace de;

struct PartitionCost
{
    int total;
    int splits;
    int iffy;
    int nearMiss;
    int realRight;
    int realLeft;
    int miniRight;
    int miniLeft;

    PartitionCost::PartitionCost() :
        total(0), splits(0), iffy(0), nearMiss(0), realRight(0),
        realLeft(0), miniRight(0), miniLeft(0)
    {}

    PartitionCost& operator += (const PartitionCost& other)
    {
        total     += other.total;
        splits    += other.splits;
        iffy      += other.iffy;
        nearMiss  += other.nearMiss;
        realLeft  += other.realLeft;
        realRight += other.realRight;
        miniLeft  += other.miniLeft;
        miniRight += other.miniRight;
        return *this;
    }
};

// Used when sorting BSP leaf half-edges by angle around midpoint.
typedef std::vector<HEdge*>HEdgeSortBuffer;

static boolean getAveragedCoords(BspLeaf* leaf, double* x, double* y)
{
    if(!leaf || !x || !y) return false;

    vec2d_t avg;
    V2d_Set(avg, 0, 0);
    size_t numPoints = 0;

    for(HEdge* hedge = leaf->hedge; hedge; hedge = hedge->next)
    {
        V2d_Sum(avg, avg, hedge->v[0]->buildData.pos);
        V2d_Sum(avg, avg, hedge->v[1]->buildData.pos);
        numPoints += 2;
    }

    if(numPoints)
    {
        if(x) *x = avg[VX] / numPoints;
        if(y) *y = avg[VY] / numPoints;
    }

    return true;
}

/**
 * Sort half-edges by angle (from the middle point to the start vertex).
 * The desired order (clockwise) means descending angles.
 *
 * @algorithm "double bubble"
 */
static void sortHEdgesByAngleAroundPoint(HEdgeSortBuffer& hedges,
    HEdgeSortBuffer::size_type total, double x, double y)
{
    HEdgeSortBuffer::iterator begin = hedges.begin();
    HEdgeSortBuffer::iterator end = begin + total;
    bool done = false;

    while(begin != end && !done)
    {
        done = true;
        HEdgeSortBuffer::iterator it(begin);
        HEdgeSortBuffer::iterator next(begin);
        ++next;
        while(next != end)
        {
            HEdge* a = *it;
            HEdge* b = *next;
            double angle1 = M_SlopeToAngle(a->v[0]->buildData.pos[VX] - x,
                                           a->v[0]->buildData.pos[VY] - y);
            double angle2 = M_SlopeToAngle(b->v[0]->buildData.pos[VX] - x,
                                           b->v[0]->buildData.pos[VY] - y);

            if(angle1 + ANG_EPSILON < angle2)
            {
                // Swap them.
                std::swap(*next, *it);
                done = false;
            }

            // Bubble down.
            ++it;
            ++next;
        }

        // Bubble up.
        --end;
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
static void clockwiseOrder(HEdgeSortBuffer& sortBuffer, HEdge** headPtr,
    uint num, double x, double y)
{
    // Ensure the sort buffer is large enough.
    if(num > sortBuffer.size())
    {
        sortBuffer.resize(num);
    }

    // Insert the hedges into the sort buffer.
    uint i = 0;
    for(HEdge* hedge = *headPtr; hedge; hedge = hedge->next, ++i)
    {
        sortBuffer[i] = hedge;
    }

    sortHEdgesByAngleAroundPoint(sortBuffer, num, x, y);

    // Re-link the half-edge list in the order of the sorted array.
    *headPtr = NULL;
    for(uint i = 0; i < num; ++i)
    {
        uint idx = (num - 1) - i;
        uint j = idx % num;

        sortBuffer[j]->next = *headPtr;
        *headPtr = sortBuffer[j];
    }

    /*
    LOG_DEBUG("Sorted half-edges around [%1.1f, %1.1f]" << x << y;
    for(hedge = *hedgePtr; hedge; hedge = hedge->next)
    {
        double angle = M_SlopeToAngle(hedge->v[0]->V_pos[VX] - x,
                                      hedge->v[0]->V_pos[VY] - y);

        LOG_DEBUG("  half-edge %p: Angle %1.6f [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
            << hedge << angle
            << hedge->v[0]->V_pos[VX] << hedge->v[0]->V_pos[VY]
            << hedge->v[1]->V_pos[VX] << hedge->v[1]->V_pos[VY];
    }
    */
}

static void logUnclosed(const BspLeaf* leaf)
{
    uint total = 0, gaps = 0;
    const HEdge* hedge = leaf->hedge;
    do
    {
        HEdge* next = hedge->next;
        if(hedge->v[1]->buildData.pos[VX] != next->v[0]->buildData.pos[VX] ||
           hedge->v[1]->buildData.pos[VY] != next->v[0]->buildData.pos[VY])
        {
            gaps++;
        }
        total++;

    } while((hedge = hedge->next) != leaf->hedge);

    if(gaps > 0)
    {
        LOG_INFO("HEdge list for BspLeaf #%p is not closed (%u gaps, %u hedges).")
                << leaf << gaps << total;

        /*
        hedge = leaf->hedge;
        do
        {
            LOG_DEBUG("  half-edge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
                << hedge
                << hedge->v[0]->pos[VX] << hedge->v[0]->pos[VY],
                << hedge->v[1]->pos[VX] << hedge->v[1]->pos[VY];

        } while((hedge = hedge->next) != leaf->hedge);
        */
    }
}

static Sector* findFirstSectorInHEdgeList(const BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    HEdge* hedge = leaf->hedge;
    do
    {
        if(hedge->sector)
        {
            return hedge->sector;
        }
    } while((hedge = hedge->next) != leaf->hedge);
    return NULL; // Nothing??
}

static void logMigrantHEdge(Sector* sector, HEdge* migrant)
{
    if(!sector || !migrant) return;

    // Prevent an excessive number of warnings per sector.
    if(sector->buildData.warnedFacing == migrant->sector->buildData.index) return;
    sector->buildData.warnedFacing = migrant->sector->buildData.index;

    if(migrant->bspBuildInfo->lineDef)
        LOG_INFO("Sector #%d has SideDef facing #%d (line #%d).")
                << sector->buildData.index << migrant->sector->buildData.index
                << migrant->bspBuildInfo->lineDef->buildData.index;
    else
        LOG_INFO("Sector #%d has SideDef facing #%d.")
                << sector->buildData.index << migrant->sector->buildData.index;
}

static void logMigrantHEdges(const BspLeaf* leaf)
{
    if(!leaf) return;

    // Find a suitable half-edge for comparison.
    Sector* sector = findFirstSectorInHEdgeList(leaf);
    if(!sector) return;

    // Log migrants.
    HEdge* hedge = leaf->hedge;
    do
    {
        if(hedge->sector && hedge->sector != sector)
        {
            logMigrantHEdge(sector, hedge);
        }
    } while((hedge = hedge->next) != leaf->hedge);
}

static boolean sanityCheckHasRealhedge(const BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    HEdge* hedge = leaf->hedge;
    do
    {
        if(hedge->bspBuildInfo->lineDef) return true;
    } while((hedge = hedge->next) != leaf->hedge);
    return false;
}

static void findSideDefHEdges(SideDef* side, HEdge* hedge)
{
    Q_ASSERT(side && hedge);

    // Already processed?
    if(side->hedgeLeft) return;

    side->hedgeLeft = hedge;
    // Find the left-most hedge.
    while(side->hedgeLeft->bspBuildInfo->prevOnSide)
        side->hedgeLeft = side->hedgeLeft->bspBuildInfo->prevOnSide;

    // Find the right-most hedge.
    side->hedgeRight = hedge;
    while(side->hedgeRight->bspBuildInfo->nextOnSide)
        side->hedgeRight = side->hedgeRight->bspBuildInfo->nextOnSide;
}

static int C_DECL clockwiseLeaf(BinaryTree* tree, void* parameters)
{
    if(BinaryTree_IsLeaf(tree))
    {
        HEdgeSortBuffer& sortBuffer = *static_cast<HEdgeSortBuffer*>(parameters);
        BspLeaf* leaf = static_cast<BspLeaf*>(BinaryTree_UserData(tree));
        double midPoint[2] = { 0, 0 };
        HEdge* hedge;

        getAveragedCoords(leaf, &midPoint[VX], &midPoint[VY]);

        // Count half-edges.
        leaf->hedgeCount = 0;
        for(hedge = leaf->hedge; hedge; hedge = hedge->next)
            leaf->hedgeCount++;

        clockwiseOrder(sortBuffer, &leaf->hedge, leaf->hedgeCount, midPoint[VX], midPoint[VY]);

        if(leaf->hedge)
        {
            /// @todo Construct the leaf's hedge ring as we go.
            hedge = leaf->hedge;
            for(;;)
            {
                // Link hedge to this leaf.
                hedge->bspLeaf = leaf;

                /// @kludge This should not be done here!
                if(hedge->bspBuildInfo->lineDef)
                {
                    // Update LineDef link.
                    hedge->lineDef = hedge->bspBuildInfo->lineDef;
                    SideDef* side = HEDGE_SIDEDEF(hedge);
                    if(side)
                    {
                        findSideDefHEdges(side, hedge);
                    }
                }
                /// kludge end

                if(hedge->next)
                {
                    // Reverse link.
                    hedge->next->prev = hedge;
                    hedge = hedge->next;
                }
                else
                {
                    // Circular link.
                    hedge->next = leaf->hedge;
                    hedge->next->prev = hedge;
                    break;
                }
            }

            // Determine which sector this BSP leaf belongs to.
            hedge = leaf->hedge;
            do
            {
                if(hedge->bspBuildInfo->lineDef &&
                   hedge->bspBuildInfo->lineDef->sideDefs[hedge->side])
                {
                    SideDef* side = hedge->bspBuildInfo->lineDef->sideDefs[hedge->side];
                    leaf->sector = side->sector;
                }
            } while(!leaf->sector && (hedge = hedge->next) != leaf->hedge);
        }

        if(!leaf->sector)
        {
            LOG_DEBUG("BspLeaf %p is orphan.") << leaf;
        }

        if(verbose)
        {
            logMigrantHEdges(leaf);
            logUnclosed(leaf);
        }

        if(!sanityCheckHasRealhedge(leaf))
        {
            Con_Error("BSP Leaf #%p has no linedef-linked half-edge!", leaf);
        }
    }

    return false; // Continue traversal.
}

void BspBuilder::windLeafs(BinaryTree* rootNode)
{
    HEdgeSortBuffer sortBuffer;
    BinaryTree_PostOrder(rootNode, clockwiseLeaf, static_cast<void*>(&sortBuffer));
}

static void evalPartitionCostForHEdge(const BspHEdgeInfo* partInfo,
    int costFactorMultiplier, const HEdge* hedge, PartitionCost& cost)
{
#define ADD_LEFT()  \
    if (hedge->bspBuildInfo->lineDef) cost.realLeft += 1;  \
    else                              cost.miniLeft += 1;  \

#define ADD_RIGHT()  \
    if (hedge->bspBuildInfo->lineDef) cost.realRight += 1;  \
    else                              cost.miniRight += 1;  \

    double qnty, a, b, fa, fb;
    assert(hedge);

    // Get state of lines' relation to each other.
    if(hedge->bspBuildInfo->sourceLineDef == partInfo->sourceLineDef)
    {
        a = b = fa = fb = 0;
    }
    else
    {
        a = M_PerpDist(partInfo->pDX, partInfo->pDY, partInfo->pPerp, partInfo->pLength,
                       hedge->bspBuildInfo->pSX, hedge->bspBuildInfo->pSY);
        b = M_PerpDist(partInfo->pDX, partInfo->pDY, partInfo->pPerp, partInfo->pLength,
                       hedge->bspBuildInfo->pEX, hedge->bspBuildInfo->pEY);

        fa = fabs(a);
        fb = fabs(b);
    }

    // hedge for being on the same line.
    if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
    {
        // This half-edge runs along the same line as the partition.
        // hedge whether it goes in the same direction or the opposite.
        if(hedge->bspBuildInfo->pDX * partInfo->pDX + hedge->bspBuildInfo->pDY * partInfo->pDY < 0)
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
    for(SuperBlock::HEdges::const_iterator it = block.hedgesBegin();
        it != block.hedgesEnd(); ++it)
    {
        // Catch "bad half-edges" early on.
        if(cost.total > bestCost) return true; // Stop iteration.

        // Evaluate the cost delta for this hedge.
        PartitionCost costDelta;
        evalPartitionCostForHEdge(hedgeInfo, splitCostFactor, *it, costDelta);

        // Merge cost result into the cummulative total.
        cost += costDelta;
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

    if(evalPartitionCostForSuperBlock(block, splitCostFactor, hedgeInfo,
                                      bestCost, cost)) return -1;

    // Make sure there is at least one real half-edge on each side.
    if(!cost.realLeft || !cost.realRight)
    {
        //LOG_DEBUG("evalPartition: No real half-edges on %s%sside")
        //    << (cost.realLeft? "" : "left ") << (cost.realRight? "" : "right ");
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

    //LOG_DEBUG("evalPartition: %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d cost=%d.%02d")
    //    << hedgeInfo << cost.splits << cost.iffy << cost.nearMiss
    //    << cost.realLeft << cost.miniLeft << cost.realRight << cost.miniRight
    //    << cost.total / 100 << cost.total % 100;

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

        //LOG_DEBUG("chooseHEdgeFromSuperBlock: %shedge %p sector:%d [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
        //    << (lineDef? "" : "mini-") << hedge
        //    << (hedge->bspBuildInfo->sector? hedge->bspBuildInfo->sector->index : -1)
        //    << hedge->v[0]->V_pos[VX] << hedge->v[0]->V_pos[VY]
        //    << hedge->v[1]->V_pos[VX] << hedge->v[1]->V_pos[VY];

        // "Mini-hedges" are never potential candidates.
        LineDef* lineDef = hedge->bspBuildInfo->lineDef;
        if(!lineDef) continue;

        // Only test half-edges from the same linedef once per round of
        // partition picking (they are collinear).
        if(lineDef->validCount == validCount) continue;
        lineDef->validCount = validCount;

        // Unsuitable or too costly?
        int cost = evalPartition(*p->hedgeList, p->splitCostFactor, hedge->bspBuildInfo, p->bestCost);
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

void BspBuilder::clearPartitionIntercepts()
{
    for(HPlane::Intercepts::iterator it = partition().begin(); it != partition().end(); ++it)
    {
        HEdgeIntercept* intercept = static_cast<HEdgeIntercept*>((*it).userData());
        deleteHEdgeIntercept(intercept);
    }
    partition().clear();
}

boolean BspBuilder::configurePartition(const HEdge* hedge)
{
    if(!hedge) return false;

    const LineDef* lineDef = hedge->bspBuildInfo->lineDef;
    if(!lineDef) return false; // A "mini hedge" is not suitable.

    // Clear the HEdge intercept data associated with points in the half-plane.
    clearPartitionIntercepts();

    setPartitionInfo(*hedge->bspBuildInfo);

    // We can now reconfire the half-plane itself.

    const Vertex* from = lineDef->L_v(hedge->side);
    const Vertex* to   = lineDef->L_v(hedge->side^1);
    partition().setOrigin(from->buildData.pos);

    vec2d_t angle; V2d_Subtract(angle, to->buildData.pos, from->buildData.pos);
    partition().setAngle(angle);

    //LOG_DEBUG("BspBuilder::configureHPlane: hedge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f].")
    //    << best << from->buildData.pos[VX] << from->buildData.pos[VY]
    //    << angle[VX] << angle[VY];

    return true;
}

boolean BspBuilder::chooseNextPartition(SuperBlock& hedgeList)
{
    choosehedgefromsuperblockparams_t parm;
    parm.hedgeList = &hedgeList;
    parm.splitCostFactor = splitCostFactor;
    parm.best = NULL;
    parm.bestCost = DDMAXINT;

    validCount++;
    if(hedgeList.traverse(chooseHEdgeFromSuperBlock, (void*)&parm))
    {
        /// @kludge BspBuilder::buildNodes() will detect the cancellation.
        return false;
    }

    /*if(parm.best)
    {
        LOG_DEBUG("BspBuilder::choosePartition: best %p score: %d.%02d.")
            << best << bestCost / 100 << bestCost % 100;
    }*/

    // Reconfigure the half plane for the next round of hedge sorting.
    return configurePartition(parm.best);
}

const HPlaneIntercept* BspBuilder::makePartitionIntersection(HEdge* hedge, int leftSide)
{
    HEdgeIntercept* hedgeIntercept;
    const HPlaneIntercept* inter;
    Vertex* vertex;
    double distance;
    Q_ASSERT(hedge);

    // Already present on this edge?
    vertex = hedge->v[leftSide?1:0];
    inter = partitionInterceptByVertex(vertex);
    if(inter) return inter;

    const BspHEdgeInfo& info = partitionInfo();
    distance = M_ParallelDist(info.pDX, info.pDY, info.pPara, info.pLength,
                              vertex->buildData.pos[VX], vertex->buildData.pos[VY]);

    LineDef* line = hedge->bspBuildInfo->lineDef;
    hedgeIntercept = newHEdgeIntercept(vertex, &info, line && lineDefInfo(*line).flags.testFlag(LineDefInfo::SELFREF));
    return partition().newIntercept(distance, hedgeIntercept);
}

double BspBuilder::hedgeDistanceFromPartition(const HEdge* hedge, bool end) const
{
    Q_ASSERT(hedge);
    const BspHEdgeInfo& info = partitionInfo();
    return M_PerpDist(info.pDX, info.pDY, info.pPerp, info.pLength,
                      end? hedge->bspBuildInfo->pEX : hedge->bspBuildInfo->pSX,
                      end? hedge->bspBuildInfo->pEY : hedge->bspBuildInfo->pSY);
}

void BspBuilder::hedgePartitionIntersection(const HEdge* hedge, double perpC, double perpD,
    pvec2d_t point) const
{
    if(!hedge || !point) return;

    BspHEdgeInfo* hedgeInfo = hedge->bspBuildInfo;

    // Horizontal partition against vertical half-edge.
    if(partitionInfo().pDY == 0 && hedgeInfo->pDX == 0)
    {
        V2d_Set(point, hedgeInfo->pSX, partitionInfo().pSY);
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(partitionInfo().pDX == 0 && hedgeInfo->pDY == 0)
    {
        V2d_Set(point, partitionInfo().pSX, hedgeInfo->pSY);
        return;
    }

    // 0 = start, 1 = end.
    double ds = perpC / (perpC - perpD);

    if(hedgeInfo->pDX == 0)
        point[VX] = hedgeInfo->pSX;
    else
        point[VX] = hedgeInfo->pSX + (hedgeInfo->pDX * ds);

    if(hedgeInfo->pDY == 0)
        point[VY] = hedgeInfo->pSY;
    else
        point[VY] = hedgeInfo->pSY + (hedgeInfo->pDY * ds);
}

void BspBuilder::divideHEdge(HEdge* hedge, SuperBlock& rightList, SuperBlock& leftList)
{
    enum {
       RIGHT = 0,
       LEFT
    };

    // Determine the relationship between this half-edge and the partition plane.
    double a = hedgeDistanceFromPartition(hedge, false/*start vertex*/);
    double b = hedgeDistanceFromPartition(hedge, true/*end vertex*/);

    /// @kludge Half-edges produced from the same source linedef must always
    ///         be treated as collinear.
    /// @todo   Why is this override necessary?
    if(hedge->bspBuildInfo->sourceLineDef == partitionInfo().sourceLineDef)
        a = b = 0;
    // kludge end

    // Collinear with the partition plane?
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makePartitionIntersection(hedge, RIGHT);
        makePartitionIntersection(hedge, LEFT);

        // Direction (vs that of the partition plane) determines in which subset
        // this half-edge belongs.
        if(hedge->bspBuildInfo->pDX * partitionInfo().pDX +
           hedge->bspBuildInfo->pDY * partitionInfo().pDY < 0)
        {
            leftList.push(hedge);
        }
        else
        {
            rightList.push(hedge);
        }
        return;
    }

    // Right of the partition plane?.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        // Close enough to intersect?
        if(a < DIST_EPSILON)
            makePartitionIntersection(hedge, RIGHT);
        else if(b < DIST_EPSILON)
            makePartitionIntersection(hedge, LEFT);

        rightList.push(hedge);
        return;
    }

    // Left of the partition plane?
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        // Close enough to intersect?
        if(a > -DIST_EPSILON)
            makePartitionIntersection(hedge, RIGHT);
        else if(b > -DIST_EPSILON)
            makePartitionIntersection(hedge, LEFT);

        leftList.push(hedge);
        return;
    }

    /**
     * Straddles the partition plane and must therefore be split.
     */
    vec2d_t point;
    hedgePartitionIntersection(hedge, a, b, point);

    HEdge* newHEdge = splitHEdge(hedge, point);

    // Ensure the new twin is inserted into the same block as the old twin.
    if(hedge->twin && !hedgeIsInLeaf(hedge->twin))
    {
        SuperBlock* block = reinterpret_cast<SuperBlock*>(hedge->twin->bspBuildInfo->block);
        Q_ASSERT(block);
        block->push(newHEdge->twin);
    }

    makePartitionIntersection(hedge, LEFT);

    if(a < 0)
    {
        rightList.push(newHEdge);
        leftList.push(hedge);
    }
    else
    {
        rightList.push(hedge);
        leftList.push(newHEdge);
    }
}

typedef struct {
    SuperBlock* rights;
    SuperBlock* lefts;
    BspBuilder* builder;
} partitionhedgeworkerparams_t;

int C_DECL BspBuilder_PartitionHEdgeWorker(SuperBlock* superblock, void* parameters)
{
    partitionhedgeworkerparams_t* p = (partitionhedgeworkerparams_t*)parameters;
    HEdge* hedge;
    assert(p);

    while((hedge = superblock->pop()))
    {
        p->builder->divideHEdge(hedge, *p->rights, *p->lefts);
    }

    return false; // Continue iteration.
}

void BspBuilder::partitionHEdges(SuperBlock& hedgeList, SuperBlock& rights, SuperBlock& lefts)
{
    partitionhedgeworkerparams_t parm;

    parm.rights = &rights;
    parm.lefts = &lefts;
    parm.builder = this;
    hedgeList.traverse(BspBuilder_PartitionHEdgeWorker, (void*)&parm);

    // Sanity checks...
    if(!rights.totalHEdgeCount())
        Con_Error("BspBuilder::partitionhedges: Separated half-edge has no right side.");

    if(!lefts.totalHEdgeCount())
        Con_Error("BspBuilder::partitionhedges: Separated half-edge has no left side.");
}

static int createBSPLeafWorker(SuperBlock* superblock, void* parameters)
{
    BspLeaf* leaf = (BspLeaf*) parameters;
    HEdge* hedge;
    assert(leaf);

    while((hedge = superblock->pop()))
    {
        // Link it into head of the leaf's list.
        hedge->next = leaf->hedge;
        leaf->hedge = hedge;
    }

    return false; // Continue iteration.
}

BspLeaf* BspBuilder::createBSPLeaf(SuperBlock& hedgeList)
{
    BspLeaf* leaf = BspLeaf_New();
    // Link the half-edges into the new leaf.
    hedgeList.traverse(createBSPLeafWorker, leaf);
    return leaf;
}

DENG_DEBUG_ONLY(
static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/)
{
    block->DebugPrint();
    return false; // Continue iteration.
})

boolean BspBuilder::buildNodes(SuperBlock& hedgeList, BinaryTree** parent)
{
    *parent = NULL;

/*#if _DEBUG
    hedgeList.traverse(printSuperBlockHEdgesWorker);
#endif*/

    // Pick a half-edge to use as the next partition plane.
    if(!chooseNextPartition(hedgeList))
    {
        // No partition required, already convex.
        //LOG_TRACE("BspBuilder::buildNodes: Convex.");

        BspLeaf* leaf = createBSPLeaf(hedgeList);
        *parent = BinaryTree_NewWithUserData(leaf);
        return true;
    }

    //LOG_TRACE("BspBuilder::buildNodes: Partition %p [%1.0f, %1.0f] -> [%1.0f, %1.0f].")
    //      << best << best->v[0]->V_pos[VX] << best->v[0]->V_pos[VY]
    //      << best->v[1]->V_pos[VX] << best->v[1]->V_pos[VY];

    // Create left and right super blockmaps.
    /// @todo There should be no need to construct entirely independent
    ///       data structures to contain these hedge subsets.
    // Copy the bounding box of the edge list to the superblocks.
    SuperBlockmap* rightHEdges = new SuperBlockmap(hedgeList.bounds());
    SuperBlockmap* leftHEdges  = new SuperBlockmap(hedgeList.bounds());

    // Divide the half-edges into two lists: left & right.
    partitionHEdges(hedgeList, rightHEdges->root(), leftHEdges->root());

    addMiniHEdges(rightHEdges->root(), leftHEdges->root());

    clearPartitionIntercepts();

    AABoxf rightHEdgesBounds, leftHEdgesBounds;
    rightHEdges->findHEdgeBounds(rightHEdgesBounds);
    leftHEdges->findHEdgeBounds(leftHEdgesBounds);

    BspNode* node = BspNode_New(partition().origin(), partition().angle());
    BspNode_SetRightBounds(node, &rightHEdgesBounds);
    BspNode_SetLeftBounds(node, &leftHEdgesBounds);
    *parent = BinaryTree_NewWithUserData(node);

    // Recurse on the right subset.
    BinaryTree* subTree;
    boolean builtOK = buildNodes(rightHEdges->root(), &subTree);
    BinaryTree_SetRight(*parent, subTree);
    delete rightHEdges;

    if(builtOK)
    {
        // Recurse on the left subset.
        builtOK = buildNodes(leftHEdges->root(), &subTree);
        BinaryTree_SetLeft(*parent, subTree);
    }

    delete leftHEdges;

    return builtOK;
}
