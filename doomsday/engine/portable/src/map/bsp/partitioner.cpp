/**
 * @file partitioner.cpp
 * BSP Partitioner. Recursive node creation and sorting. @ingroup map
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
#include "bspleaf.h"
#include "bspnode.h"
#include "hedge.h"
#include "p_mapdata.h"
#include "p_maputil.h"
#include "m_misc.h"

#include "map/bsp/hedgeintercept.h"
#include "map/bsp/hplane.h"
#include "map/bsp/partitioner.h"
#include "map/bsp/superblockmap.h"

using namespace de::bsp;

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

Partitioner::Partitioner(GameMap* _map, uint* _numEditableVertexes,
    Vertex*** _editableVertexes, int _splitCostFactor) :
    splitCostFactor(_splitCostFactor),
    map(_map),
    numEditableVertexes(_numEditableVertexes), editableVertexes(_editableVertexes),
    rootNode(0), partition(0),
    unclosedSectors(), migrantHEdges(),
    builtOK(false)
{
    initPartitionInfo();
}

static int clearBspObject(BspTreeNode& tree, void* /*parameters*/)
{
    if(tree.isLeaf())
    {
        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
        if(leaf)
        {
            LOG_DEBUG("Partitioner: Clearing unclaimed leaf %p.") << leaf;
            BspLeaf_Delete(leaf);
        }
    }
    else
    {
        BspNode* node = reinterpret_cast<BspNode*>(tree.userData());
        if(node)
        {
            LOG_DEBUG("Partitioner: Clearing unclaimed node %p.") << node;
            BspNode_Delete(node);
        }
    }
    return false; // Continue iteration.
}

Partitioner::~Partitioner()
{
    for(uint i = 0; i < *numEditableVertexes; ++i)
    {
        deleteHEdgeTips((*editableVertexes)[i]);
    }

    for(Vertexes::iterator it = vertexes.begin(); it != vertexes.end(); ++it)
    {
        Vertex* vtx = *it;
        // Has ownership of this vertex been claimed?
        if(!vtx) continue;

        deleteHEdgeTips(vtx);
        free(vtx);
    }

    // We are finished with the BSP data.
    if(rootNode)
    {
        // If ownership of the BSP data has been claimed this should be a no-op.
        BspTreeNode::PostOrder(*rootNode, clearBspObject, NULL/*no parameters*/);

        // Destroy our private BSP tree.
        delete rootNode;
    }
}

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

typedef struct {
    Partitioner* partitioner;
    HEdgeSortBuffer sortBuffer;
} clockwiseleafparams_t;

static int clockwiseLeaf(BspTreeNode& tree, void* parameters)
{
    if(tree.isLeaf())
    {
        clockwiseleafparams_t* p = static_cast<clockwiseleafparams_t*>(parameters);
        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
        double midPoint[2] = { 0, 0 };
        HEdge* hedge;

        getAveragedCoords(leaf, &midPoint[VX], &midPoint[VY]);

        // Count half-edges.
        leaf->hedgeCount = 0;
        for(hedge = leaf->hedge; hedge; hedge = hedge->next)
            leaf->hedgeCount++;

        clockwiseOrder(p->sortBuffer, &leaf->hedge, leaf->hedgeCount, midPoint[VX], midPoint[VY]);

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
            LOG_WARNING("BspLeaf %p is orphan.") << leaf;
        }

        if(verbose)
        {
            p->partitioner->registerMigrantHEdges(leaf);
            logUnclosed(leaf);
        }

        if(!sanityCheckHasRealhedge(leaf))
        {
            Con_Error("BSP Leaf #%p has no linedef-linked half-edge!", leaf);
        }
    }

    return false; // Continue traversal.
}

void Partitioner::windLeafs()
{
    clockwiseleafparams_t parm;
    parm.partitioner = this;
    BspTreeNode::PostOrder(*rootNode, clockwiseLeaf, static_cast<void*>(&parm));
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
       evalPartitionCostForSuperBlock(*block.right(), splitCostFactor,
                                      hedgeInfo, bestCost, cost)) return true;

    if(block.hasLeft() &&
       evalPartitionCostForSuperBlock(*block.left(), splitCostFactor,
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

void Partitioner::clearPartitionIntercepts()
{
    for(HPlane::Intercepts::iterator it = partition->begin(); it != partition->end(); ++it)
    {
        HEdgeIntercept* intercept = static_cast<HEdgeIntercept*>((*it).userData());
        deleteHEdgeIntercept(*intercept);
    }
    partition->clear();
}

bool Partitioner::configurePartition(const HEdge* hedge)
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
    partition->setOrigin(from->buildData.pos);

    vec2d_t angle; V2d_Subtract(angle, to->buildData.pos, from->buildData.pos);
    partition->setAngle(angle);

    //LOG_DEBUG("Partitioner::configureHPlane: hedge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f].")
    //    << best << from->buildData.pos[VX] << from->buildData.pos[VY]
    //    << angle[VX] << angle[VY];

    return true;
}

void Partitioner::chooseHEdgeFromSuperBlock(SuperBlock* partList, SuperBlock& hedgeList,
    HEdge** best, int* bestCost)
{
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
        LineDefInfo& lInfo = lineDefInfo(*lineDef);
        if(lInfo.validCount == validCount) continue;
        lInfo.validCount = validCount;

        // Unsuitable or too costly?
        int cost = evalPartition(hedgeList, splitCostFactor, hedge->bspBuildInfo, *bestCost);
        if(cost >= 0 && cost < *bestCost)
        {
            // We have a new better choice.
            *bestCost = cost;

            // Remember which half-edge.
            *best = hedge;
        }
    }
}

bool Partitioner::chooseNextPartition(SuperBlock& hedgeList)
{
    HEdge* best = NULL;
    int bestCost = DDMAXINT;

    // Increment valid count so we can avoid testing the half edges produced
    // from a single linedef more than once per round of partition selection.
    validCount++;

    // Iterative pre-order traversal of SuperBlock.
    SuperBlock* cur = &hedgeList;
    SuperBlock* prev = 0;
    while(cur)
    {
        while(cur)
        {
            chooseHEdgeFromSuperBlock(cur, hedgeList, &best, &bestCost);

            if(prev == cur->parent())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->hasRight()) cur = cur->right();
                else                cur = cur->left();
            }
            else if(prev == cur->right())
            {
                // Last moved up the right branch - descend the left.
                prev = cur;
                cur = cur->left();
            }
            else if(prev == cur->left())
            {
                // Last moved up the left branch - continue upward.
                prev = cur;
                cur = cur->parent();
            }
        }

        if(prev)
        {
            // No right child - back up.
            cur = prev->parent();
        }
    }

    /*if(best)
    {
        LOG_DEBUG("Partitioner::choosePartition: best %p score: %d.%02d.")
            << best << bestCost / 100 << bestCost % 100;
    }*/

    // Reconfigure the half plane for the next round of hedge sorting.
    return configurePartition(best);
}

const HPlaneIntercept* Partitioner::makePartitionIntersection(HEdge* hedge, int leftSide)
{
    Q_ASSERT(hedge);

    // Already present on this edge?
    Vertex* vertex = hedge->v[leftSide?1:0];
    const HPlaneIntercept* inter = partitionInterceptByVertex(vertex);
    if(inter) return inter;

    LineDef* line = hedge->bspBuildInfo->lineDef;
    HEdgeIntercept* intercept = newHEdgeIntercept(vertex, line && lineDefInfo(*line).flags.testFlag(LineDefInfo::SELFREF));

    return partition->newIntercept(vertexDistanceFromPartition(vertex), intercept);
}

double Partitioner::vertexDistanceFromPartition(const Vertex* vertex) const
{
    Q_ASSERT(vertex);
    const BspHEdgeInfo& info = partitionInfo;
    return M_ParallelDist(info.pDX, info.pDY, info.pPara, info.pLength,
                          vertex->buildData.pos[VX], vertex->buildData.pos[VY]);
}

double Partitioner::hedgeDistanceFromPartition(const HEdge* hedge, bool end) const
{
    Q_ASSERT(hedge);
    const BspHEdgeInfo& info = partitionInfo;
    return M_PerpDist(info.pDX, info.pDY, info.pPerp, info.pLength,
                      end? hedge->bspBuildInfo->pEX : hedge->bspBuildInfo->pSX,
                      end? hedge->bspBuildInfo->pEY : hedge->bspBuildInfo->pSY);
}

void Partitioner::interceptHEdgePartition(const HEdge* hedge, double perpC, double perpD,
    pvec2d_t point) const
{
    if(!hedge || !point) return;

    BspHEdgeInfo* hedgeInfo = hedge->bspBuildInfo;

    // Horizontal partition against vertical half-edge.
    if(partitionInfo.pDY == 0 && hedgeInfo->pDX == 0)
    {
        V2d_Set(point, hedgeInfo->pSX, partitionInfo.pSY);
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(partitionInfo.pDX == 0 && hedgeInfo->pDY == 0)
    {
        V2d_Set(point, partitionInfo.pSX, hedgeInfo->pSY);
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

void Partitioner::divideHEdge(HEdge* hedge, SuperBlock& rightList, SuperBlock& leftList)
{
#define RIGHT 0
#define LEFT  1

    // Determine the relationship between this half-edge and the partition plane.
    double a = hedgeDistanceFromPartition(hedge, false/*start vertex*/);
    double b = hedgeDistanceFromPartition(hedge, true/*end vertex*/);

    /// @kludge Half-edges produced from the same source linedef must always
    ///         be treated as collinear.
    /// @todo   Why is this override necessary?
    if(hedge->bspBuildInfo->sourceLineDef == partitionInfo.sourceLineDef)
        a = b = 0;
    // kludge end

    // Collinear with the partition plane?
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makePartitionIntersection(hedge, RIGHT);
        makePartitionIntersection(hedge, LEFT);

        // Direction (vs that of the partition plane) determines in which subset
        // this half-edge belongs.
        if(hedge->bspBuildInfo->pDX * partitionInfo.pDX +
           hedge->bspBuildInfo->pDY * partitionInfo.pDY < 0)
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
    interceptHEdgePartition(hedge, a, b, point);

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

#undef LEFT
#undef RIGHT
}

void Partitioner::partitionHEdges(SuperBlock& hedgeList, SuperBlock& rights, SuperBlock& lefts)
{
    // Iterative pre-order traversal of SuperBlock.
    SuperBlock* cur = &hedgeList;
    SuperBlock* prev = 0;
    while(cur)
    {
        while(cur)
        {
            HEdge* hedge;
            while((hedge = cur->pop()))
            {
                divideHEdge(hedge, rights, lefts);
            }

            if(prev == cur->parent())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->hasRight()) cur = cur->right();
                else                cur = cur->left();
            }
            else if(prev == cur->right())
            {
                // Last moved up the right branch - descend the left.
                prev = cur;
                cur = cur->left();
            }
            else if(prev == cur->left())
            {
                // Last moved up the left branch - continue upward.
                prev = cur;
                cur = cur->parent();
            }
        }

        if(prev)
        {
            // No right child - back up.
            cur = prev->parent();
        }
    }

    // Sanity checks...
    if(!rights.totalHEdgeCount())
        Con_Error("Partitioner::partitionhedges: Separated half-edge has no right side.");

    if(!lefts.totalHEdgeCount())
        Con_Error("Partitioner::partitionhedges: Separated half-edge has no left side.");
}

BspLeaf* Partitioner::createBSPLeaf(SuperBlock& hedgeList)
{
    BspLeaf* leaf = BspLeaf_New();

    // Iterative pre-order traversal of SuperBlock.
    SuperBlock* cur = &hedgeList;
    SuperBlock* prev = 0;
    while(cur)
    {
        while(cur)
        {
            HEdge* hedge;
            while((hedge = cur->pop()))
            {
                // Link it into head of the leaf's list.
                hedge->next = leaf->hedge;
                leaf->hedge = hedge;
            }

            if(prev == cur->parent())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->hasRight()) cur = cur->right();
                else                cur = cur->left();
            }
            else if(prev == cur->right())
            {
                // Last moved up the right branch - descend the left.
                prev = cur;
                cur = cur->left();
            }
            else if(prev == cur->left())
            {
                // Last moved up the left branch - continue upward.
                prev = cur;
                cur = cur->parent();
            }
        }

        if(prev)
        {
            // No right child - back up.
            cur = prev->parent();
        }
    }

    return leaf;
}

DENG_DEBUG_ONLY(
static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/)
{
    block->DebugPrint();
    return false; // Continue iteration.
})

bool Partitioner::buildNodes(SuperBlock& hedgeList, BspTreeNode** parent)
{
    *parent = NULL;

/*#if _DEBUG
    hedgeList.traverse(printSuperBlockHEdgesWorker);
#endif*/

    // Pick a half-edge to use as the next partition plane.
    if(!chooseNextPartition(hedgeList))
    {
        // No partition required, already convex.
        //LOG_TRACE("Partitioner::buildNodes: Convex.");

        BspLeaf* leaf = createBSPLeaf(hedgeList);
        *parent = new BspTreeNode(reinterpret_cast<runtime_mapdata_header_t*>(leaf));
        return true;
    }

    //LOG_TRACE("Partitioner::buildNodes: Partition %p [%1.0f, %1.0f] -> [%1.0f, %1.0f].")
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

    BspNode* node = BspNode_New(partition->origin(), partition->angle());
    BspNode_SetRightBounds(node, &rightHEdgesBounds);
    BspNode_SetLeftBounds(node, &leftHEdgesBounds);

    *parent = new BspTreeNode(reinterpret_cast<runtime_mapdata_header_t*>(node));

    // Recurse on the right subset.
    BspTreeNode* subTree;
    bool builtOK = buildNodes(rightHEdges->root(), &subTree);
    (*parent)->setRight(subTree);
    delete rightHEdges;

    if(builtOK)
    {
        // Recurse on the left subset.
        builtOK = buildNodes(leftHEdges->root(), &subTree);
        (*parent)->setLeft(subTree);
    }

    delete leftHEdges;

    return builtOK;
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

void Partitioner::findMapBounds(AABoxf* aaBox) const
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

void Partitioner::createInitialHEdges(SuperBlock& hedgeList)
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
                hedgeList.push(front);
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
                hedgeList.push(back);

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

                    hedgeList.push(other);

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

        addHEdgeTip(line->v[0], M_SlopeToAngle(x2 - x1, y2 - y1), back, front);
        addHEdgeTip(line->v[1], M_SlopeToAngle(x1 - x2, y1 - y2), front, back);
    }
}

void Partitioner::initForMap()
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

    vertexInfos.resize(*numEditableVertexes);
}

static int linkBspTreeNode(BspTreeNode& tree, void* /*parameters*/)
{
    // We are only interested in BspNodes at this level.
    if(tree.isLeaf()) return false; // Continue iteration.

    BspNode* node = reinterpret_cast<BspNode*>(tree.userData());

    if(BspTreeNode* right = tree.right())
    {
        BspNode_SetRight(node, right->userData());
    }

    if(BspTreeNode* left = tree.left())
    {
        BspNode_SetLeft(node, left->userData());
    }

    return false; // Continue iteration.
}

static int clearHEdgeInfo(BspTreeNode& tree, void* /*parameters*/)
{
    if(tree.isLeaf())
    {
        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
        HEdge* hedge = leaf->hedge;
        do
        {
            Z_Free(HEdge_DetachBspBuildInfo(hedge));
        } while((hedge = hedge->next) != leaf->hedge);
    }
    return false; // Continue iteration.
}

void Partitioner::initHEdgesAndBuildBsp(SuperBlockmap& blockmap)
{
    Q_ASSERT(map);
    // It begins...
    rootNode = NULL;

    createInitialHEdges(blockmap.root());
    builtOK = buildNodes(blockmap.root(), &rootNode);

    if(rootNode)
    {
        // Wind leafs.
        windLeafs();

        // Link up the BSP object tree.
        /// @todo Do this earlier.
        BspTreeNode::PostOrder(*rootNode, linkBspTreeNode);

        // We're done with the build info.
        BspTreeNode::PreOrder(*rootNode, clearHEdgeInfo);
    }
}

bool Partitioner::build()
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
    partition = new HPlane();

    initHEdgesAndBuildBsp(*blockmap);

    delete partition;
    delete blockmap;

    return builtOK;
}

BspTreeNode* Partitioner::root() const
{
    return rootNode;
}

static int countNode(BspTreeNode& tree, void* data)
{
    if(!tree.isLeaf())
        (*((uint*) data))++;
    return false; // Continue iteration.
}

/// @todo Store this as a running total.
uint Partitioner::numNodes()
{
    uint count = 0;
    BspTreeNode::PostOrder(*rootNode, countNode, static_cast<void*>(&count));
    return count;
}

static int countLeaf(BspTreeNode& tree, void* data)
{
    if(tree.isLeaf())
        (*((uint*) data))++;
    return false; // Continue iteration.
}

/// @todo Store this as a running total.
uint Partitioner::numLeafs()
{
    uint count = 0;
    BspTreeNode::PostOrder(*rootNode, countLeaf, static_cast<void*>(&count));
    return count;
}

const HPlaneIntercept* Partitioner::partitionInterceptByVertex(Vertex* vertex)
{
    if(!vertex) return NULL; // Hmm...

    for(HPlane::Intercepts::const_iterator it = partition->begin(); it != partition->end(); ++it)
    {
        const HPlaneIntercept* inter = &*it;
        if(reinterpret_cast<HEdgeIntercept*>(inter->userData())->vertex == vertex) return inter;
    }

    return NULL;
}

HEdgeIntercept* Partitioner::hedgeInterceptByVertex(Vertex* vertex)
{
    const HPlaneIntercept* hpi = partitionInterceptByVertex(vertex);
    if(!hpi) return NULL; // Not found.
    return reinterpret_cast<HEdgeIntercept*>(hpi->userData());
}

void Partitioner::addHEdgesBetweenIntercepts(HEdgeIntercept* start, HEdgeIntercept* end,
    HEdge** right, HEdge** left)
{
    Q_ASSERT(start && end);

    // Create the half-edge pair.
    // Leave 'linedef' field as NULL as these are not linedef-linked.
    // Leave 'side' as zero too.
    (*right) = newHEdge(NULL, partitionInfo.lineDef, start->vertex, end->vertex, start->after, false);
    ( *left) = newHEdge(NULL, partitionInfo.lineDef, end->vertex, start->vertex, start->after, false);

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

HEdgeIntercept& Partitioner::mergeHEdgeIntercepts(HEdgeIntercept& final, HEdgeIntercept& other)
{
    /*
    LOG_TRACE("BspBuilder::mergeHEdgeIntercepts: Merging intersections:");
    HEdgeIntercept::DebugPrint(final);
    HEdgeIntercept::DebugPrint(other);
    */

    if(final.selfRef && !other.selfRef)
    {
        if(final.before && other.before)
            final.before = other.before;

        if(final.after && other.after)
            final.after = other.after;

        final.selfRef = false;
    }

    if(!final.before && other.before)
        final.before = other.before;

    if(!final.after && other.after)
        final.after = other.after;

    /*
    LOG_TRACE("Bsp_MergeHEdgeIntercepts: Result:");
    HEdgeIntercept::DebugPrint(final);
    */

    // Destroy the redundant other.
    deleteHEdgeIntercept(other);
    return final;
}

void Partitioner::mergeIntersections()
{
    HPlane::Intercepts::iterator node = partition->begin();
    while(node != partition->end())
    {
        HPlane::Intercepts::iterator np = node; np++;
        if(np == partition->end()) break;

        double len = *np - *node;
        if(len < -0.1)
        {
            Con_Error("BspBuilder_MergeIntersections: Invalid intercept order - %1.3f > %1.3f\n",
                      node->distance(), np->distance());
        }
        else if(len > 0.2)
        {
            node++;
            continue;
        }

        HEdgeIntercept* cur  = reinterpret_cast<HEdgeIntercept*>(node->userData());
        HEdgeIntercept* next = reinterpret_cast<HEdgeIntercept*>(np->userData());

        /*if(len > DIST_EPSILON)
        {
            LOG_DEBUG("Skipping very short half-edge (len: %1.3f) near [%1.1f, %1.1f]")
                    << len << cur->vertex->V_pos[VX] << cur->vertex->V_pos[VY];
        }*/

        // Merge info for the two intersections into one (next is destroyed).
        mergeHEdgeIntercepts(*cur, *next);

        // Unlink this intercept.
        partition->deleteIntercept(np);
    }
}

void Partitioner::buildHEdgesAtIntersectionGaps(SuperBlock& rightList, SuperBlock& leftList)
{
    HPlane::Intercepts::const_iterator node = partition->begin();
    while(node != partition->end())
    {
        HPlane::Intercepts::const_iterator np = node; np++;
        if(np == partition->end()) break;

        HEdgeIntercept* cur = reinterpret_cast<HEdgeIntercept*>((*node).userData());
        HEdgeIntercept* next = reinterpret_cast<HEdgeIntercept*>((*np).userData());

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

                    registerUnclosedSector(cur->after, pos[VX], pos[VY]);
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

                    registerUnclosedSector(next->before, pos[VX], pos[VY]);
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

                addHEdgesBetweenIntercepts(cur, next, &right, &left);

                // Add the new half-edges to the appropriate lists.
                rightList.push(right);
                leftList.push(left);
            }
        }

        node++;
    }
}

void Partitioner::addMiniHEdges(SuperBlock& rightList, SuperBlock& leftList)
{
/*#if _DEBUG
    HPlane::DebugPrint(*partition);
#endif*/

    // Fix any issues with the current intersections.
    mergeIntersections();

    LOG_TRACE("Building HEdges along partition [%1.1f, %1.1f] > [%1.1f, %1.1f]")
            << partitionInfo.pSX << partitionInfo.pSY << partitionInfo.pDX << partitionInfo.pDY;

    // Find connections in the intersections.
    buildHEdgesAtIntersectionGaps(rightList, leftList);
}

uint Partitioner::numVertexes()
{
    return vertexes.size();
}

Vertex const& Partitioner::vertex(uint idx)
{
    Q_ASSERT(idx < vertexes.size());
    return *vertexes[idx];
}

Vertex* Partitioner::newVertex(const_pvec2d_t point)
{
    Vertex* vtx;

    // Allocate with calloc for uniformity with the editable vertexes.
    vtx = static_cast<Vertex*>(calloc(1, sizeof *vtx));
    vtx->header.type = DMU_VERTEX;
    vtx->buildData.index = *numEditableVertexes + uint(vertexes.size() + 1); // 1-based index, 0 = NIL.
    vertexes.push_back(vtx);

    vertexInfos.push_back(VertexInfo());
    if(point)
    {
        V2d_Copy(vtx->buildData.pos, point);
    }
    return vtx;
}

Sector* Partitioner::openSectorAtAngle(Vertex* vtx, double angle)
{
    Q_ASSERT(vtx);

    // First check whether there's a wall_tip that lies in the exact direction of
    // the given direction (which is relative to the vtxex).
    for(HEdgeTip* tip = vertexInfo(*vtx).tipSet; tip; tip = tip->ET_next)
    {
        double diff = fabs(tip->angle - angle);

        if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
        {
            // Yes, found one.
            return NULL;
        }
    }

    // OK, now just find the first wall_tip whose angle is greater than the angle
    // we're interested in. Therefore we'll be on the FRONT side of that tip edge.
    for(HEdgeTip* tip = vertexInfo(*vtx).tipSet; tip; tip = tip->ET_next)
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

    Con_Error("Vertex %d has no hedge tips !", vtx->buildData.index);
    exit(1); // Unreachable.
}

HEdgeIntercept* Partitioner::newHEdgeIntercept(Vertex* vert, bool selfRef)
{
    HEdgeIntercept* inter = new HEdgeIntercept();

    inter->vertex = vert;
    inter->selfRef = selfRef;

    inter->before = openSectorAtAngle(vert, M_SlopeToAngle(-partitionInfo.pDX, -partitionInfo.pDY));
    inter->after  = openSectorAtAngle(vert, M_SlopeToAngle( partitionInfo.pDX,  partitionInfo.pDY));

    return inter;
}

void Partitioner::deleteHEdgeIntercept(HEdgeIntercept& inter)
{
    delete &inter;
}

HEdgeTip* Partitioner::newHEdgeTip(void)
{
    HEdgeTip* tip = new HEdgeTip();
    return tip;
}

void Partitioner::deleteHEdgeTip(HEdgeTip* tip)
{
    Q_ASSERT(tip);
    delete tip;
}

void Partitioner::addHEdgeTip(Vertex* vtx, double angle, HEdge* back, HEdge* front)
{
    Q_ASSERT(vtx);

    HEdgeTip* tip = newHEdgeTip();
    tip->angle = angle;
    tip->ET_edge[BACK]  = back;
    tip->ET_edge[FRONT] = front;

    // Find the correct place (order is increasing angle).
    VertexInfo& vtxInfo = vertexInfo(*vtx);
    HEdgeTip* after;
    for(after = vtxInfo.tipSet; after && after->ET_next;
        after = after->ET_next) {}

    while(after && tip->angle + ANG_EPSILON < after->angle)
        after = after->ET_prev;

    // Link it in.
    if(after)
        tip->ET_next = after->ET_next;
    else
        tip->ET_next = vtxInfo.tipSet;
    tip->ET_prev = after;

    if(after)
    {
        if(after->ET_next)
            after->ET_next->ET_prev = tip;

        after->ET_next = tip;
    }
    else
    {
        if(vtxInfo.tipSet)
            vtxInfo.tipSet->ET_prev = tip;

        vtxInfo.tipSet = tip;
    }
}

void Partitioner::deleteHEdgeTips(Vertex* vtx)
{
    if(!vtx) return;

    HEdgeTip* tip = vertexInfo(*vtx).tipSet;
    while(tip)
    {
        HEdgeTip* next = tip->ET_next;
        deleteHEdgeTip(tip);
        tip = next;
    }
}

static void updateHEdgeInfo(const HEdge* hedge, BspHEdgeInfo* info)
{
    assert(hedge);
    if(!info) return;

    info->pSX = hedge->v[0]->buildData.pos[VX];
    info->pSY = hedge->v[0]->buildData.pos[VY];
    info->pEX = hedge->v[1]->buildData.pos[VX];
    info->pEY = hedge->v[1]->buildData.pos[VY];
    info->pDX = info->pEX - info->pSX;
    info->pDY = info->pEY - info->pSY;

    info->pLength = M_Length(info->pDX, info->pDY);
    info->pAngle  = M_SlopeToAngle(info->pDX, info->pDY);

    info->pPerp =  info->pSY * info->pDX - info->pSX * info->pDY;
    info->pPara = -info->pSX * info->pDX - info->pSY * info->pDY;

    if(info->pLength <= 0)
        Con_Error("HEdge {%p} is of zero length.", hedge);
}

HEdge* Partitioner::newHEdge(LineDef* lineDef, LineDef* sourceLineDef,
    Vertex* start, Vertex* end, Sector* sec, bool back)
{
    HEdge* hedge = HEdge_New();

    hedge->v[0] = start;
    hedge->v[1] = end;
    hedge->sector = sec;
    Q_ASSERT(sec == NULL || GameMap_SectorIndex(map, sec) >= 0);
    hedge->side = (back? 1 : 0);

    BspHEdgeInfo* info = static_cast<BspHEdgeInfo*>(Z_Malloc(sizeof *info, PU_MAP, 0));
    HEdge_AttachBspBuildInfo(hedge, info);

    info->lineDef = lineDef;
    info->sourceLineDef = sourceLineDef;
    info->nextOnSide = info->prevOnSide = NULL;
    info->block = NULL;
    updateHEdgeInfo(hedge, info);

    return hedge;
}

HEdge* Partitioner::cloneHEdge(const HEdge& other)
{
    HEdge* hedge = HEdge_NewCopy(&other);
    if(other.bspBuildInfo)
    {
        BspHEdgeInfo* info = static_cast<BspHEdgeInfo*>(Z_Malloc(sizeof *info, PU_MAP, 0));
        memcpy(info, other.bspBuildInfo, sizeof(BspHEdgeInfo));
        HEdge_AttachBspBuildInfo(hedge, info);
    }
    return hedge;
}

bool Partitioner::hedgeIsInLeaf(const HEdge* hedge) const
{
    /// @todo Are we now able to determine this by testing hedge->leaf ?
    return !hedge->bspBuildInfo->block;
}

HEdge* Partitioner::splitHEdge(HEdge* oldHEdge, const_pvec2d_t point)
{
    Q_ASSERT(oldHEdge);

    //LOG_DEBUG("Splitting hedge %p at [%1.1f, %1.1f].")
    //        << oldHEdge << x << y;

    Vertex* newVert = newVertex(point);
    addHEdgeTip(newVert, M_SlopeToAngle(-oldHEdge->bspBuildInfo->pDX, -oldHEdge->bspBuildInfo->pDY), oldHEdge, oldHEdge->twin);
    addHEdgeTip(newVert, M_SlopeToAngle( oldHEdge->bspBuildInfo->pDX,  oldHEdge->bspBuildInfo->pDY), oldHEdge->twin, oldHEdge);

    HEdge* newHEdge = cloneHEdge(*oldHEdge);

    newHEdge->bspBuildInfo->prevOnSide = oldHEdge;
    oldHEdge->bspBuildInfo->nextOnSide = newHEdge;

    oldHEdge->v[1] = newVert;
    updateHEdgeInfo(oldHEdge, oldHEdge->bspBuildInfo);

    newHEdge->v[0] = newVert;
    updateHEdgeInfo(newHEdge, newHEdge->bspBuildInfo);

    // Handle the twin.
    if(oldHEdge->twin)
    {
        //LOG_DEBUG("Splitting hedge twin %p.") << oldHEdge->twin;

        // Copy the old hedge info.
        newHEdge->twin = cloneHEdge(*oldHEdge->twin);
        newHEdge->twin->twin = newHEdge;

        newHEdge->twin->bspBuildInfo->nextOnSide = oldHEdge->twin;
        oldHEdge->twin->bspBuildInfo->prevOnSide = newHEdge->twin;

        oldHEdge->twin->v[0] = newVert;
        updateHEdgeInfo(oldHEdge->twin, oldHEdge->twin->bspBuildInfo);

        newHEdge->twin->v[1] = newVert;
        updateHEdgeInfo(newHEdge->twin, newHEdge->twin->bspBuildInfo);

        // Has this already been added to a leaf?
        if(hedgeIsInLeaf(oldHEdge->twin))
        {
            // Update the in-leaf references.
            oldHEdge->twin->next = newHEdge->twin;
        }
    }

    return newHEdge;
}

bool Partitioner::registerUnclosedSector(Sector* sector, double x, double y)
{
    if(!sector) return false;

    // Has this sector already been registered?
    for(UnclosedSectors::const_iterator it = unclosedSectors.begin();
        it != unclosedSectors.end(); ++it)
    {
        UnclosedSectorRecord const& record = *it;
        if(record.sector == sector)
            return false;
    }

    // Add a new record.
    unclosedSectors.push_back(UnclosedSectorRecord(sector, x, y));

    // In the absence of a better mechanism, simply log this right away.
    /// @todo Implement something better!
    LOG_WARNING("Sector %p #%d is unclosed near [%1.1f, %1.1f].")
            << sector << sector->buildData.index - 1 << x << y;

    return true;
}

bool Partitioner::registerMigrantHEdge(Sector* sector, HEdge* migrant)
{
    if(!sector || !migrant) return false;

    // Has this pair already been registered?
    for(MigrantHEdges::const_iterator it = migrantHEdges.begin();
        it != migrantHEdges.end(); ++it)
    {
        MigrantHEdgeRecord const& record = *it;
        if(record.facingSector == sector && record.hedge == migrant)
            return false;
    }

    // Add a new record.
    migrantHEdges.push_back(MigrantHEdgeRecord(migrant, sector));

    // In the absence of a better mechanism, simply log this right away.
    /// @todo Implement something better!
    if(migrant->bspBuildInfo->lineDef)
        LOG_WARNING("Sector #%d has HEdge facing #%d (line #%d).")
                << sector->buildData.index << migrant->sector->buildData.index
                << migrant->bspBuildInfo->lineDef->buildData.index;
    else
        LOG_WARNING("Sector #%d has HEdge facing #%d.")
                << sector->buildData.index << migrant->sector->buildData.index;

    return true;
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

void Partitioner::registerMigrantHEdges(const BspLeaf* leaf)
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
            registerMigrantHEdge(sector, hedge);
        }
    } while((hedge = hedge->next) != leaf->hedge);
}
