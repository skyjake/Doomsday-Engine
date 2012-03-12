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
#include "de_misc.h"

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

    for(cur = headPtr; cur; cur = cur->next)
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
        angle_g angle1, angle2;

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
    for(hEdge = *headPtr, i = 0; hEdge; hEdge = hEdge->next, ++i)
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

        hEdgeSortBuf[j]->next = *headPtr;
        *headPtr = hEdgeSortBuf[j];
    }

/*#if _DEBUG
    Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

    for(hEdge = sub->hEdges; hEdge; hEdge = hEdge->next)
    {
        angle_g angle = M_SlopeToAngle(hEdge->v[0]->V_pos[VX] - x,
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

    for(cur = leaf->hEdges; cur; cur = cur->next)
    {
        next = (cur->next? cur->next : leaf->hEdges);

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
    for(compare = leaf->hEdges; compare; compare = compare->next)
    {
        if(compare->sector) break;
    }

    if(!compare) return;

    for(cur = compare->next; cur; cur = cur->next)
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

    for(cur = leaf->hEdges; cur; cur = cur->next)
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
    for(cur = leaf->hEdges; cur; cur = cur->next)
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
        hEdgeSortBuf =
            M_Realloc(hEdgeSortBuf, hEdgeSortBufSize * sizeof(bsp_hedge_t*));
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
        for(hEdge = leaf->hEdges; hEdge; hEdge = hEdge->next)
            total++;

        // Ensure the sort buffer is large enough.
        prepareHEdgeSortBuffer(total);

        clockwiseOrder(&leaf->hEdges, total, midPoint[VX], midPoint[VY]);
        renumberLeafHEdges(leaf, data);

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

void ClockwiseBspTree(binarytree_t* rootNode)
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
    return M_Malloc(sizeof(bspleafdata_t));
}

static __inline void freeBSPLeaf(bspleafdata_t* leaf)
{
    M_Free(leaf);
}

bspleafdata_t* BSPLeaf_Create(void)
{
    bspleafdata_t* leaf = allocBSPLeaf();

    leaf->hEdges = NULL;

    return leaf;
}

void BSPLeaf_Destroy(bspleafdata_t* leaf)
{
    bsp_hedge_t* cur, *np;

    if(!leaf) return;

    cur = leaf->hEdges;
    while(cur)
    {
        np = cur->next;
        BSP_HEdge_Destroy(cur);
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
    result = SuperBlock_IterateHEdges(hEdgeList, evalPartitionWorker2, (void*)&parm);
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
    return SuperBlock_IterateHEdges(partList, pickHEdgeWorker2, parameters);
}

boolean BSP_PickPartition(SuperBlock* hEdgeList, size_t depth, bspartition_t* partition)
{
    pickhedgeworkerparams_t parm;
    int bestCost = INT_MAX;
    bsp_hedge_t* best = NULL;

    //DEBUG_Message(("BSP_PickPartition: Begun (depth %lu)\n", (unsigned long) depth));

    parm.hEdgeList = hEdgeList;
    parm.best = &best;
    parm.bestCost = &bestCost;

    validCount++;
    if(SuperBlock_Traverse(hEdgeList, pickHEdgeWorker, (void*)&parm))
    {
        /// @kludge BuildNodes() will detect the cancellation.
        return false;
    }

    // Finished, return the best partition.
    if(best)
    {
        //DEBUG_Message(("BSP_PickPartition: Best has score %d.%02d  (%1.1f,%1.1f) -> "
        //               "(%1.1f,%1.1f)\n", bestCost / 100, bestCost % 100,
        //               best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
        //               best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]));

        assert(best->lineDef);

        partition->x  = best->lineDef->v[best->side]->buildData.pos[VX];
        partition->y  = best->lineDef->v[best->side]->buildData.pos[VY];
        partition->dX = best->lineDef->v[best->side^1]->buildData.pos[VX] - partition->x;
        partition->dY = best->lineDef->v[best->side^1]->buildData.pos[VY] - partition->y;
        partition->lineDef = best->lineDef;
        partition->sourceLineDef = best->sourceLineDef;

        partition->pDX = best->pDX;
        partition->pDY = best->pDY;
        partition->pSX = best->pSX;
        partition->pSY = best->pSY;
        partition->pPara = best->pPara;
        partition->pPerp = best->pPerp;
        partition->length = best->pLength;
        return true;
    }

    //DEBUG_Message(("BSP_PickPartition: No best found!\n"));
    return false;
}

static void makeIntersection(cutlist_t* cutList, Vertex* vert, const bspartition_t* part,
    boolean selfRef)
{
    intersection_t* cut = BSP_CutListFindIntersection(cutList, vert);

    if(!cut)
    {
        cut = BSP_IntersectionCreate(vert, part, selfRef);
        BSP_CutListInsertIntersection(cutList, cut);
    }
}

/**
 * Calculate the intersection location between the current half-edge and the partition.
 * Takes advantage of some common situations like horizontal and vertical lines to
 * choose a 'nicer' intersection point.
 */
static __inline void calcIntersection(bsp_hedge_t* cur, const bspartition_t* part,
    double perpC, double perpD, double* x, double* y)
{
    double ds;

    // Horizontal partition against vertical half-edge.
    if(part->pDY == 0 && cur->pDX == 0)
    {
        *x = cur->pSX;
        *y = part->pSY;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(part->pDX == 0 && cur->pDY == 0)
    {
        *x = part->pSX;
        *y = cur->pSY;
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(cur->pDX == 0)
        *x = cur->pSX;
    else
        *x = cur->pSX + (cur->pDX * ds);

    if(cur->pDY == 0)
        *y = cur->pSY;
    else
        *y = cur->pSY + (cur->pDY * ds);
}

void BSP_DivideOneHEdge(bsp_hedge_t* cur, const bspartition_t* part, SuperBlock* rightList,
    SuperBlock* leftList, cutlist_t* cutList)
{
    bsp_hedge_t* newHEdge;
    double x, y;
    double a, b;
    boolean selfRef = (cur->lineDef? (cur->lineDef->buildData.mlFlags & MLF_SELFREF) : false);

    // Get state of lines' relation to each other.
    a = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->length, cur->pSX, cur->pSY);
    b = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->length, cur->pEX, cur->pEY);

    if(cur->sourceLineDef == part->sourceLineDef)
        a = b = 0;

    // Check for being on the same line.
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makeIntersection(cutList, cur->v[0], part, selfRef);
        makeIntersection(cutList, cur->v[1], part, selfRef);

        // This hedge runs along the same line as the partition. Check whether it goes in
        // the same direction or the opposite.
        if(cur->pDX * part->pDX + cur->pDY * part->pDY < 0)
        {
            SuperBlock_HEdgePush(leftList, cur);
        }
        else
        {
            SuperBlock_HEdgePush(rightList, cur);
        }

        return;
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        if(a < DIST_EPSILON)
            makeIntersection(cutList, cur->v[0], part, selfRef);
        else if(b < DIST_EPSILON)
            makeIntersection(cutList, cur->v[1], part, selfRef);

        SuperBlock_HEdgePush(rightList, cur);
        return;
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        if(a > -DIST_EPSILON)
            makeIntersection(cutList, cur->v[0], part, selfRef);
        else if(b > -DIST_EPSILON)
            makeIntersection(cutList, cur->v[1], part, selfRef);

        SuperBlock_HEdgePush(leftList, cur);
        return;
    }

    // When we reach here, we have a and b non-zero and opposite sign, hence this edge
    // will be split by the partition line.

    calcIntersection(cur, part, a, b, &x, &y);
    newHEdge = BSP_HEdge_Split(cur, x, y);
    makeIntersection(cutList, cur->v[1], part, selfRef);

    if(a < 0)
    {
        SuperBlock_HEdgePush(leftList,  cur);
        SuperBlock_HEdgePush(rightList, newHEdge);
    }
    else
    {
        SuperBlock_HEdgePush(rightList, cur);
        SuperBlock_HEdgePush(leftList,  newHEdge);
    }
}

typedef struct {
    const bspartition_t* part;
    SuperBlock* rights;
    SuperBlock* lefts;
    cutlist_t* cutList;
} partitionhedgeworkerparams_t;

static int partitionHEdgeWorker(SuperBlock* superblock, void* parameters)
{
    partitionhedgeworkerparams_t* p = (partitionhedgeworkerparams_t*)parameters;
    bsp_hedge_t* hEdge;
    assert(p);

    while((hEdge = SuperBlock_HEdgePop(superblock)))
    {
        hEdge->block = NULL;

        BSP_DivideOneHEdge(hEdge, p->part, p->rights, p->lefts, p->cutList);
    }

    return false; // Continue iteration.
}

void BSP_PartitionHEdges(SuperBlock* hEdgeList, const bspartition_t* part,
    SuperBlock* rights, SuperBlock* lefts, cutlist_t* cutList)
{
    partitionhedgeworkerparams_t parm;

    parm.part = part;
    parm.rights = rights;
    parm.lefts = lefts;
    parm.cutList = cutList;
    SuperBlock_Traverse(hEdgeList, partitionHEdgeWorker, (void*)&parm);

    // Sanity checks...
    if(!SuperBlock_TotalHEdgeCount(rights))
        Con_Error("BSP_PartitionHEdges: Separated half-edge has no right side.");

    if(!SuperBlock_TotalHEdgeCount(lefts))
        Con_Error("BSP_PartitionHEdges: Separated half-edge has no left side.");

    BSP_AddMiniHEdges(part, rights, lefts, cutList);
}

static int createBSPLeafWorker(SuperBlock* superblock, void* parameters)
{
    bspleafdata_t* leaf = (bspleafdata_t*) parameters;
    bsp_hedge_t* hEdge;
    assert(leaf);

    while((hEdge = SuperBlock_HEdgePop(superblock)))
    {
        // Link it into head of the leaf's list.
        hEdge->next = leaf->hEdges;
        hEdge->block = NULL;

        leaf->hEdges = hEdge;
    }

    return false; // Continue iteration.
}

/**
 * Create a new leaf from a list of half-edges.
 */
static bspleafdata_t* createBSPLeaf(SuperBlock* hEdgeList)
{
    bspleafdata_t* leaf = BSPLeaf_Create();

    // Link the half-edges into the new leaf.
    SuperBlock_Traverse(hEdgeList, createBSPLeafWorker, leaf);

    return leaf;
}

boolean BuildNodes(SuperBlock* hEdgeList, binarytree_t** parent, size_t depth,
    cutlist_t* cutList)
{
    binarytree_t* subTree;
    bspnodedata_t* node;
    SuperBlock* hEdgeSet[2];
    bspleafdata_t* leaf;
    boolean builtOK = false;
    bspartition_t partition;

    *parent = NULL;

    /*DEBUG_Message(("Build: Begun @ %lu\n", (unsigned long) depth));
#if _DEBUG
    BSP_PrintSuperblockHEdges(hEdgeList);
#endif*/

    // Pick the next partition to use.
    if(!BSP_PickPartition(hEdgeList, depth, &partition))
    {
        // No partition required, already convex.
        //DEBUG_Message(("BuildNodes: Convex.\n"));

        leaf = createBSPLeaf(hEdgeList);
        *parent = BinaryTree_Create(leaf);
        return true;
    }

    //DEBUG_Message(("BuildNodes: Partition %p (%1.0f,%1.0f) -> (%1.0f,%1.0f).\n",
    //               best, best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
    //               best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]));

    // Create left and right super blocks.
    // Copy the bounding box of the edge list to the superblocks.
    hEdgeSet[RIGHT] = BSP_NewSuperBlock(SuperBlock_Bounds(hEdgeList));
    hEdgeSet[LEFT]  = BSP_NewSuperBlock(SuperBlock_Bounds(hEdgeList));

    // Divide the half-edges into two lists: left & right.
    BSP_PartitionHEdges(hEdgeList, &partition, hEdgeSet[RIGHT], hEdgeSet[LEFT], cutList);
    BSP_CutListEmpty(cutList);

    node = M_Calloc(sizeof(bspnodedata_t));
    *parent = BinaryTree_Create(node);

    SuperBlock_FindHEdgeListBounds(hEdgeSet[LEFT],  &node->aaBox[LEFT]);
    SuperBlock_FindHEdgeListBounds(hEdgeSet[RIGHT], &node->aaBox[RIGHT]);

    node->partition.x = partition.x;
    node->partition.y = partition.y;
    node->partition.dX = partition.dX;
    node->partition.dY = partition.dY;

    builtOK = BuildNodes(hEdgeSet[RIGHT], &subTree, depth + 1, cutList);
    BinaryTree_SetChild(*parent, RIGHT, subTree);
    BSP_RecycleSuperBlock(hEdgeSet[RIGHT]);

    if(builtOK)
    {
        builtOK = BuildNodes(hEdgeSet[LEFT], &subTree, depth + 1, cutList);
        BinaryTree_SetChild(*parent, LEFT, subTree);
    }

    BSP_RecycleSuperBlock(hEdgeSet[LEFT]);

    return builtOK;
}
