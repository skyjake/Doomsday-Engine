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

#include "bspbuilder/intersection.hh"
#include "bspbuilder/superblockmap.hh"
#include "bspbuilder/bspbuilder.hh"

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

#if _DEBUG
void BSP_PrintSuperBlockhedges(SuperBlock* superblock);
#endif

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

static int C_DECL clockwiseLeaf(BinaryTree* tree, void* parameters)
{
    if(BinaryTree_IsLeaf(tree))
    {
        HEdgeSortBuffer& sortBuffer = *static_cast<HEdgeSortBuffer*>(parameters);
        BspLeaf* leaf = (BspLeaf*) BinaryTree_UserData(tree);
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
            hedge = leaf->hedge;
            for(;;)
            {
                // Link hedge to this leaf.
                hedge->bspLeaf = leaf;

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

boolean BspBuilder::choosePartition(SuperBlock& hedgeList, size_t /*depth*/, HPlane& hplane)
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

    // Finished, return the best partition.
    HEdge* best = parm.best;
    if(best)
    {
        const LineDef* lineDef = best->bspBuildInfo->lineDef;
        // This must not be a "mini hedge".
        Q_ASSERT(lineDef);

        // Reconfigure the half plane for the next round of hedge sorting.
        hplane.setPartitionHEdgeInfo(*best->bspBuildInfo);

        const Vertex* from = lineDef->L_v(best->side);
        const Vertex* to   = lineDef->L_v(best->side^1);
        hplane.setOrigin(from->buildData.pos);

        vec2d_t angle; V2d_Subtract(angle, to->buildData.pos, from->buildData.pos);
        hplane.setAngle(angle);

        //LOG_DEBUG("BspBuilder::choosePartition: best: %p score: %d.%02d [%1.1f, %1.1f] -> [%1.1f, %1.1f].")
        //    << best << bestCost / 100 << bestCost % 100
        //    << from->buildData.pos[VX] << from->buildData.pos[VY]
        //    << angle[VX] << angle[VY];

        return true;
    }

    //DEBUG_Message(("BspBuilder::choosePartition: No best found!\n"));
    return false;
}

static boolean lineDefHasSelfRef(LineDef* lineDef)
{
    return !!(lineDef->buildData.mlFlags & MLF_SELFREF);
}

const HPlaneIntercept* BspBuilder::makeHPlaneIntersection(HPlane& hplane, HEdge* hedge, int leftSide)
{
    HEdgeIntercept* hedgeIntercept;
    const HPlaneIntercept* inter;
    Vertex* vertex;
    double distance;
    Q_ASSERT(hedge);

    // Already present on this edge?
    vertex = hedge->v[leftSide?1:0];
    inter = hplaneInterceptByVertex(hplane, vertex);
    if(inter) return inter;

    const BspHEdgeInfo& info = hplane.partitionHEdgeInfo();
    distance = M_ParallelDist(info.pDX, info.pDY, info.pPara, info.pLength,
                              vertex->buildData.pos[VX], vertex->buildData.pos[VY]);

    hedgeIntercept = newHEdgeIntercept(vertex, &info, (hedge->bspBuildInfo->lineDef && lineDefHasSelfRef(hedge->bspBuildInfo->lineDef)));
    return hplane.newIntercept(distance, hedgeIntercept);
}

const HPlaneIntercept* BspBuilder::makeIntersection(HPlane& hplane, HEdge* hedge, int leftSide)
{
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

void BspBuilder::divideHEdge(HEdge* hedge, HPlane& hplane, SuperBlock& rightList,
    SuperBlock& leftList)
{
    enum {
       RIGHT = 0,
       LEFT
    };

    // Get state of lines' relation to each other.
    const BspHEdgeInfo& info = hplane.partitionHEdgeInfo();
    double a = M_PerpDist(info.pDX, info.pDY, info.pPerp, info.pLength, hedge->bspBuildInfo->pSX, hedge->bspBuildInfo->pSY);
    double b = M_PerpDist(info.pDX, info.pDY, info.pPerp, info.pLength, hedge->bspBuildInfo->pEX, hedge->bspBuildInfo->pEY);

    if(hedge->bspBuildInfo->sourceLineDef == info.sourceLineDef)
        a = b = 0;

    // Check for being on the same line.
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makeIntersection(hplane, hedge, RIGHT);
        makeIntersection(hplane, hedge, LEFT);

        // This hedge runs along the same line as the partition. Check whether it goes in
        // the same direction or the opposite.
        if(hedge->bspBuildInfo->pDX * info.pDX + hedge->bspBuildInfo->pDY * info.pDY < 0)
        {
            leftList.hedgePush(hedge);
        }
        else
        {
            rightList.hedgePush(hedge);
        }
        return;
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        if(a < DIST_EPSILON)
            makeIntersection(hplane, hedge, RIGHT);
        else if(b < DIST_EPSILON)
            makeIntersection(hplane, hedge, LEFT);

        rightList.hedgePush(hedge);
        return;
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        if(a > -DIST_EPSILON)
            makeIntersection(hplane, hedge, RIGHT);
        else if(b > -DIST_EPSILON)
            makeIntersection(hplane, hedge, LEFT);

        leftList.hedgePush(hedge);
        return;
    }

    // When we reach here, we have a and b non-zero and opposite sign, hence this edge
    // will be split by the partition line.
    double x, y;
    calcIntersection(hedge->bspBuildInfo, &info, a, b, &x, &y);
    HEdge* newhedge = splitHEdge(hedge, x, y);
    makeIntersection(hplane, hedge, LEFT);

    if(a < 0)
    {
        leftList.hedgePush(hedge);
        rightList.hedgePush(newhedge);
    }
    else
    {
        rightList.hedgePush(hedge);
        leftList.hedgePush(newhedge);
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
        p->builder->divideHEdge(hedge, *p->hplane, *p->rights, *p->lefts);
    }

    return false; // Continue iteration.
}

void BspBuilder::partitionHEdges(HPlane& hplane, SuperBlock& hedgeList,
    SuperBlock& rights, SuperBlock& lefts)
{
    partitionhedgeworkerparams_t parm;

    parm.rights = &rights;
    parm.lefts = &lefts;
    parm.hplane = &hplane;
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

    while((hedge = superblock->hedgePop()))
    {
        // Link it into head of the leaf's list.
        hedge->next = leaf->hedge;
        leaf->hedge = hedge;
    }

    return false; // Continue iteration.
}

/**
 * Create a new leaf from a list of half-edges.
 */
BspLeaf* BspBuilder::createBSPLeaf(SuperBlock& hedgeList)
{
    BspLeaf* leaf = BspLeaf_New();
    // Link the half-edges into the new leaf.
    hedgeList.traverse(createBSPLeafWorker, leaf);
    return leaf;
}

boolean BspBuilder::buildNodes(SuperBlock& hedgeList, BinaryTree** parent, size_t depth,
    HPlane& hplane)
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
    if(!choosePartition(hedgeList, depth, hplane))
    {
        // No partition required, already convex.
        //DEBUG_Message(("BspBuilder::buildNodes: Convex.\n"));

        leaf = createBSPLeaf(hedgeList);
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
    rightHEdges = new SuperBlockmap(hedgeList.bounds());
    leftHEdges  = new SuperBlockmap(hedgeList.bounds());

    // Divide the half-edges into two lists: left & right.
    partitionHEdges(hplane, hedgeList, rightHEdges->root(), leftHEdges->root());

    addMiniHEdges(hplane, rightHEdges->root(), leftHEdges->root());
    hplane.clear();

    rightHEdges->findHEdgeBounds(rightHEdgesBounds);
    leftHEdges->findHEdgeBounds(leftHEdgesBounds);

    node = BspNode_New(hplane.origin(), hplane.angle());
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
                (hedge->bspBuildInfo->lineDef? "NORM" : "MINI"), hedge,
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
