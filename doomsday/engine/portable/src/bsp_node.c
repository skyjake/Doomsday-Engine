/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * bsp_node.c: BSP node builder. Recursive node creation and sorting.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

/**
 * \notes
 * Split a list of half-edges into two using the method described at the
 * bottom of the file, this was taken from OBJECTS.C in the DEU5beta source.
 *
 * This is done by scanning all of the half-edges and finding the one that
 * does the least splitting and has the least difference in numbers of
 * half-edges on either side.
 *
 * If the ones on the left side make a SSector, then create another SSector
 * else put the half-edges into the left list.
 * If the ones on the right side make a SSector, then create another SSector
 * else put the half-edges into the right list.
 *
 * Rewritten by Andrew Apted (-AJA-), 1999-2000.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int numCompleteHEdges;

// CODE --------------------------------------------------------------------

/**
 * @return          @c -1, for left +1, for right 0, for intersect.
 */
static int pointOnHEdgeSide(double x, double y, hedge_t *hEdge)
{
    double      perp = PerpDist(hEdge, x, y);

    if(fabs(perp) <= DIST_EPSILON)
        return 0;

    return (perp < 0? -1 : +1);
}

/**
 * Check the relationship between the given box and the partition line.
 * @return          @c -1, if box is on left side, @c 1, if box is on right
 *                  else @c 0, if the line intersects the box.
 */
int BoxOnLineSide(superblock_t *box, hedge_t *part)
{
    int         p1, p2;
    double      x1 = (double)box->x1 - IFFY_LEN * 1.5;
    double      y1 = (double)box->y1 - IFFY_LEN * 1.5;
    double      x2 = (double)box->x2 + IFFY_LEN * 1.5;
    double      y2 = (double)box->y2 + IFFY_LEN * 1.5;

    if(part->pDX == 0)
    {   // Horizontal.
        p1 = (x1 > part->pSX? +1 : -1);
        p2 = (x2 > part->pSX? +1 : -1);

        if(part->pDY < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(part->pDY == 0)
    {   // Vertical.
        p1 = (y1 < part->pSY? +1 : -1);
        p2 = (y2 < part->pSY? +1 : -1);

        if(part->pDX < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(part->pDX * part->pDY > 0)
    {   // Positive slope.
        p1 = pointOnHEdgeSide(x1, y2, part);
        p2 = pointOnHEdgeSide(x2, y1, part);
    }
    else
    {   // Negative slope.
        p1 = pointOnHEdgeSide(x1, y1, part);
        p2 = pointOnHEdgeSide(x2, y2, part);
    }

    if(p1 == p2)
        return p1;

    return 0;
}

/**
 * Add the given half-edge to the specified list.
 */
void BSP_AddHEdgeToSuperBlock(superblock_t *block, hedge_t *hEdge)
{
    for(;;)
    {
        int         p1, p2;
        int         child;
        int         midPoint[2];
        superblock_t *sub;

        midPoint[VX] = (block->x1 + block->x2) / 2;
        midPoint[VY] = (block->y1 + block->y2) / 2;

        // Update half-edge counts.
        if(hEdge->linedef)
            block->realNum++;
        else
            block->miniNum++;

        if(SUPER_IS_LEAF(block))
        {   // Block is a leaf -- no subdivision possible.
            BSP_LinkHEdgeToSuperBlock(block, hEdge);
            return;
        }

        if(block->x2 - block->x1 >= block->y2 - block->y1)
        {   // Block is wider than it is high, or square.
            p1 = hEdge->v[0]->V_pos[VX] >= midPoint[VX];
            p2 = hEdge->v[1]->V_pos[VX] >= midPoint[VX];
        }
        else
        {   // Block is higher than it is wide.
            p1 = hEdge->v[0]->V_pos[VY] >= midPoint[VY];
            p2 = hEdge->v[1]->V_pos[VY] >= midPoint[VY];
        }

        if(p1 && p2)
            child = 1;
        else if(!p1 && !p2)
            child = 0;
        else
        {   // Line crosses midpoint -- link it in and return.
            BSP_LinkHEdgeToSuperBlock(block, hEdge);
            return;
        }

        // The seg lies in one half of this block. Create the block if it
        // doesn't already exist, and loop back to add the seg.
        if(!block->subs[child])
        {
            block->subs[child] = sub = BSP_SuperBlockCreate();
            sub->parent = block;

            if(block->x2 - block->x1 >= block->y2 - block->y1)
            {
                sub->x1 = (child? midPoint[VX] : block->x1);
                sub->y1 = block->y1;

                sub->x2 = (child? block->x2 : midPoint[VX]);
                sub->y2 = block->y2;
            }
            else
            {
                sub->x1 = block->x1;
                sub->y1 = (child? midPoint[VY] : block->y1);

                sub->x2 = block->x2;
                sub->y2 = (child? block->y2 : midPoint[VY]);
            }
        }

        block = block->subs[child];
    }
}

static void determineMiddle(msubsec_t *sub)
{
    int         total = 0;
    double      midPoint[2];
    hedge_t    *cur;

    midPoint[VX] = midPoint[VY] = 0;

    // Compute middle coordinates.
    for(cur = sub->hEdges; cur; cur = cur->next)
    {
        midPoint[VX] += cur->v[0]->V_pos[VX] + cur->v[1]->V_pos[VX];
        midPoint[VY] += cur->v[0]->V_pos[VY] + cur->v[1]->V_pos[VY];

        total += 2;
    }

    sub->midPoint[VX] = midPoint[VX] / total;
    sub->midPoint[VY] = midPoint[VY] / total;
}

/**
 * Sort half-edges by angle (from the middle point to the start vertex).
 * The desired order (clockwise) means descending angles.
 */
static void sortHEdgesByAngleAroundPoint(hedge_t **hEdges, uint total,
                                         double *point)
{
    uint        i;

    i = 0;
    while(i + 1 < total)
    {
        hedge_t    *a = hEdges[i];
        hedge_t    *b = hEdges[i+1];
        angle_g     angle1, angle2;

        angle1 = M_SlopeToAngle(a->v[0]->V_pos[VX] - point[VX],
                                a->v[0]->V_pos[VY] - point[VY]);
        angle2 = M_SlopeToAngle(b->v[0]->V_pos[VX] - point[VX],
                                b->v[0]->V_pos[VY] - point[VY]);

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
 * Put the list of half-edges into clockwise order.
 *
 * \note Algorithm:
 * Uses the now famous "double bubble" sorter :).
 */
static void clockwiseOrder(msubsec_t *sub)
{
    int         i;
    int         total = 0;
    hedge_t    *cur;
    hedge_t   **hEdges;

/*
#if _DEBUG
Con_Message("Subsec: Clockwising %d\n", sub->index);
#endif
*/

    // Count half-edges and create an array to manipulate them.
    for(cur = sub->hEdges; cur; cur = cur->next)
        total++;

    hEdges = M_Malloc((total + 1) * sizeof(hedge_t *));

    for(cur = sub->hEdges, i = 0; cur; cur = cur->next, ++i)
        hEdges[i] = cur;
    hEdges[total] = NULL; // Terminate.

    if(i != total)
        Con_Error("clockwiseOrder: Miscounted?");

    sortHEdgesByAngleAroundPoint(hEdges, total, sub->midPoint);

    // Re-link the half-edge list in the order of the sorted array.
    sub->hEdges = NULL;
    for(i = total - 1; i >= 0; i--)
    {
        int         j = i % total;

        hEdges[j]->next = sub->hEdges;
        sub->hEdges = hEdges[j];
    }

    sub->hEdgeCount = total;

/*
#if _DEBUG
Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", sub->midPoint[VX],
            sub->midPoint[VY]);

{
hedge_t    **ptr = hEdges;
while(*ptr)
{
    hedge_t    *hEdge = *ptr;
    angle_g     angle =
        M_SlopeToAngle(hEdge->v[0]->V_pos[VX] - sub->midPoint[VX],
                       hEdge->v[0]->V_pos[VY] - sub->midPoint[VY]);

    Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                hEdge, angle, hEdge->v[0]->V_pos[VX], hEdge->v[0]->V_pos[VY],
                hEdge->v[1]->V_pos[VX], hEdge->v[1]->V_pos[VY]);
    *ptr++;
}
}
#endif
*/
    M_Free(hEdges);
}

static void sanityCheckClosed(msubsec_t *sub)
{
    int         total = 0, gaps = 0;
    hedge_t    *cur, *next;

    for(cur = sub->hEdges; cur; cur = cur->next)
    {
        next = (cur->next? cur->next : sub->hEdges);

        if(cur->v[1]->V_pos[VX] != next->v[0]->V_pos[VX] ||
           cur->v[1]->V_pos[VY] != next->v[0]->V_pos[VY])
            gaps++;

        total++;
    }

    if(gaps > 0)
    {
        Con_Message("Subsector #%d near (%1.1f,%1.1f) is not closed "
                    "(%d gaps, %d half-edges)\n", sub->index,
                    sub->midPoint[VX], sub->midPoint[VY], gaps, total);
/*
#if _DEBUG
for(cur = sub->hEdges; cur; cur = cur->next)
{
    Con_Message("  half-edge %p  (%1.1f,%1.1f) --> (%1.1f,%1.1f)\n", cur,
                cur->v[0]->V_pos[VX], cur->v[0]->V_pos[VY],
                cur->v[1]->V_pos[VX], cur->v[1]->V_pos[VY]);
}
#endif
*/
    }
}

static void sanityCheckSameSector(msubsec_t *sub)
{
    hedge_t    *cur, *compare;

    // Find a suitable half-edge for comparison.
    for(compare = sub->hEdges; compare; compare = compare->next)
    {
        if(!compare->sector)
            continue;

        break;
    }

    if(!compare)
        return;

    for(cur = compare->next; cur; cur = cur->next)
    {
        if(!cur->sector)
            continue;

        if(cur->sector == compare->sector)
            continue;

        // Prevent excessive number of warnings.
        if(compare->sector->warnedFacing == cur->sector->index)
            continue;

        compare->sector->warnedFacing = cur->sector->index;

        if(verbose >= 1)
        if(cur->linedef)
            Con_Message("Sector #%d has sidedef facing #%d (line #%d) "
                        "near (%1.0f,%1.0f).\n", compare->sector->index,
                        cur->sector->index, cur->linedef->index,
                        sub->midPoint[VX], sub->midPoint[VY]);
        else
            Con_Message("Sector #%d has sidedef facing #%d "
                        "near (%1.0f,%1.0f).\n", compare->sector->index,
                        cur->sector->index, sub->midPoint[VX], sub->midPoint[VY]);
    }
}

static void sanityCheckHasRealHEdge(msubsec_t *sub)
{
    hedge_t    *cur;

    for(cur = sub->hEdges; cur; cur = cur->next)
    {
        if(cur->linedef)
            return;
    }

    Con_Error("SSec #%d near (%1.1f,%1.1f) has no linedef-linked half-edge!",
              sub->index, sub->midPoint[VX], sub->midPoint[VY]);
}

static void renumberSubSectorHEdges(msubsec_t *sub)
{
    uint        n;
    hedge_t    *cur;

/*
#if _DEBUG
Con_Message("Subsec: Renumbering %d\n", sub->index);
#endif
*/
    n = 0;
    for(cur = sub->hEdges; cur; cur = cur->next)
    {
        cur->index = numCompleteHEdges;
        numCompleteHEdges++;
        n++;
/*
#if _DEBUG
Con_Message("Subsec:   %d: half-edge %p  Index %d\n", n, cur, cur->index);
#endif
*/
    }
}

static void createSubSectorWorker(msubsec_t *sub, superblock_t *block)
{
    uint        num;

    while(block->hEdges)
    {
        hedge_t *cur = block->hEdges;

        // Un-link first half-edge from the block.
        block->hEdges = cur->next;

        // Link it into head of the subsector's list.
        cur->next = sub->hEdges;
        cur->block = NULL;

        sub->hEdges = cur;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t *a = block->subs[num];

        if(a)
        {
            createSubSectorWorker(sub, a);

            if(a->realNum + a->miniNum > 0)
                Con_Error("createSubSectorWorker: child %d not empty!", num);

            BSP_SuperBlockDestroy(a);
            block->subs[num] = NULL;
        }
    }

    block->realNum = block->miniNum = 0;
}

/**
 * Create a subsector from a list of half-edges.
 */
static msubsec_t *createSubSector(superblock_t *hEdgeList)
{
    msubsec_t *sub = NewSubsec();

    // Compute subsector's index.
    sub->index = numSubSecs - 1;

    // Link the half-edges into the new subsector.
    createSubSectorWorker(sub, hEdgeList);

    determineMiddle(sub);

/*
#if _DEBUG
Con_Message("createSubSector: Index= %d.\n", sub->index);
#endif
*/
    return sub;
}

/**
 * Compute the height of the bsp tree, starting at 'node'.
 */
int ComputeBspHeight(mnode_t *node)
{
    if(node)
    {
        int         left, right;

        right = ComputeBspHeight(node->children[RIGHT].node);
        left  = ComputeBspHeight(node->children[LEFT].node);

        return MAX_OF(left, right) + 1;
    }

    return 1;
}

/**
 * Takes the half-edge list and determines if it is convex, possibly
 * converting it into a subsector. Otherwise, the list is divided into two
 * halves and recursion will continue on the new sub list.
 *
 * @param s             Ptr to the new subsector is written back here if
 *                      one is created, else will be set to @c NULL.
 * @param n             Ptr to the new node is written back here if one is
 *                      created, else will be set to @c NULL.
 * @return              @c true, if successfull.
 */
boolean BuildNodes(superblock_t *hEdgeList, mnode_t **n, msubsec_t **s,
                   int depth)
{
    mnode_t    *node;
    child_t    *child;
    hedge_t    *best;
    superblock_t *rights;
    superblock_t *lefts;
    cutlist_t  *cutList;
    boolean     builtOK = false;

    *n = NULL;
    *s = NULL;
/*
#if _DEBUG
Con_Message("Build: Begun @ %d\n", depth);
BSP_PrintSuperblockHEdges(hEdgeList);
#endif
*/
    // Pick best node to use. None indicates convexicity.
    best = BSP_PickNode(hEdgeList, depth);

    if(best == NULL)
    {
/*
#if _DEBUG
Con_Message("BuildNodes: Convex.\n");
#endif
*/
        *s = createSubSector(hEdgeList);
        return true;
    }

/*
#if _DEBUG
Con_Message("BuildNodes: Partition %p (%1.0f,%1.0f) -> (%1.0f,%1.0f).\n",
            best, best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
            best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]);
#endif
*/
    // Create left and right super blocks.
    lefts  = (superblock_t *) BSP_SuperBlockCreate();
    rights = (superblock_t *) BSP_SuperBlockCreate();

    lefts->x1 = rights->x1 = hEdgeList->x1;
    lefts->y1 = rights->y1 = hEdgeList->y1;
    lefts->x2 = rights->x2 = hEdgeList->x2;
    lefts->y2 = rights->y2 = hEdgeList->y2;

    // Divide the half-edges into two lists: left & right.
    cutList = BSP_CutListCreate();
    BSP_SeparateHEdges(hEdgeList, best, lefts, rights, cutList);

    // Sanity checks...
    if(rights->realNum + rights->miniNum == 0)
        Con_Error("BuildNodes: Separated halfedge-list has no right side.");

    if(lefts->realNum + lefts->miniNum == 0)
        Con_Error("BuildNodes: Separated halfedge-list has no left side.");

    BSP_AddMiniHEdges(best, lefts, rights, cutList);

    *n = node = NewNode();

    assert(best->linedef);

    // Should we not be doing some rounding here? - DJS.
    if(best->side == 0)
    {   // Right.
        node->x  = best->linedef->v[0]->V_pos[VX];
        node->y  = best->linedef->v[0]->V_pos[VY];
        node->dX = best->linedef->v[1]->V_pos[VX] - node->x;
        node->dY = best->linedef->v[1]->V_pos[VY] - node->y;
    }
    else
    {   // Left.
        node->x  = best->linedef->v[1]->V_pos[VX];
        node->y  = best->linedef->v[1]->V_pos[VY];
        node->dX = best->linedef->v[0]->V_pos[VX] - node->x;
        node->dY = best->linedef->v[0]->V_pos[VY] - node->y;
    }

    // Check for really long partition (overflows dx,dy in NODES).
    if(best->pLength >= 30000)
    {
        if(node->dX && node->dY && ((node->dX & 1) || (node->dY & 1)))
        {
            VERBOSE2(Con_Message("Loss of accuracy on VERY long node: "
                                 "(%d,%d) -> (%d,%d).\n", node->x, node->y,
                                 node->x + node->dX, node->y + node->dY));
        }

        node->tooLong = true;
    }

    // Find limits of vertices.
    BSP_FindNodeBounds(node, rights, lefts);

/*
#if _DEBUG
Con_Message("BuildNodes: Going left.\n");
#endif
*/
    child = &node->children[LEFT];
    builtOK = BuildNodes(lefts, &child->node, &child->subSec, depth + 1);
    BSP_SuperBlockDestroy(lefts);

    if(builtOK)
    {
/*
#if _DEBUG
Con_Message("BuildNodes: Going right.\n");
#endif
*/
        child = &node->children[RIGHT];
        builtOK = BuildNodes(rights, &child->node, &child->subSec, depth + 1);
    }

    BSP_SuperBlockDestroy(rights);
    BSP_CutListDestroy(cutList);
/*
#if _DEBUG
Con_Message("BuildNodes: Done.\n"));
#endif
*/
    return builtOK;
}

/**
 * Traverse the BSP tree and put all the half-edges in each subsector into
 * clockwise order, and renumber their indices.
 *
 * \important This cannot be done during BuildNodes() since splitting a
 * half-edge with a twin may insert another half-edge into that twin's list,
 * usually in the wrong place order-wise.
 */
void ClockwiseBspTree(mnode_t *root)
{
    int         i;

    (void) root; // Shutup compiler.

    numCompleteHEdges = 0;
    for(i = 0; i < numSubSecs; ++i)
    {
        msubsec_t  *sub = LookupSubsec(i);

        clockwiseOrder(sub);
        renumberSubSectorHEdges(sub);

        // Do some sanity checks.
        sanityCheckClosed(sub);
        sanityCheckSameSector(sub);
        sanityCheckHasRealHEdge(sub);
    }
}

//---------------------------------------------------------------------------
//
//    This message has been taken, complete, from OBJECTS.C in DEU5beta
//    source.  It outlines the method used here to pick the nodelines.
//
// IF YOU ARE WRITING A DOOM EDITOR, PLEASE READ THIS:
//
// I spent a lot of time writing the Nodes builder.  There are some bugs in
// it, but most of the code is OK.  If you steal any ideas from this program,
// put a prominent message in your own editor to make it CLEAR that some
// original ideas were taken from DEU.  Thanks.
//
// While everyone was talking about LineDefs, I had the idea of taking only
// the Segs into account, and creating the Segs directly from the SideDefs.
// Also, dividing the list of Segs in two after each call to CreateNodes makes
// the algorithm faster.  I use several other tricks, such as looking at the
// two ends of a Seg to see on which side of the nodeline it lies or if it
// should be split in two.  I took me a lot of time and efforts to do this.
//
// I give this algorithm to whoever wants to use it, but with this condition:
// if your program uses some of the ideas from DEU or the whole algorithm, you
// MUST tell it to the user.  And if you post a message with all or parts of
// this algorithm in it, please post this notice also.  I don't want to speak
// legalese; I hope that you understand me...  I kindly give the sources of my
// program to you: please be kind with me...
//
// If you need more information about this, here is my E-mail address:
// Raphael.Quinet@eed.ericsson.se (Raphael Quinet).
//
// Short description of the algorithm:
//   1 - Create one Seg for each SideDef: pick each LineDef in turn.  If it
//       has a "first" SideDef, then create a normal Seg.  If it has a
//       "second" SideDef, then create a flipped Seg.
//   2 - Call CreateNodes with the current list of Segs.  The list of Segs is
//       the only argument to CreateNodes.
//   3 - Save the Nodes, Segs and SSectors to disk.  Start with the leaves of
//       the Nodes tree and continue up to the root (last Node).
//
// CreateNodes does the following:
//   1 - Pick a nodeline amongst the Segs (minimize the number of splits and
//       keep the tree as balanced as possible).
//   2 - Move all Segs on the right of the nodeline in a list (segs1) and do
//       the same for all Segs on the left of the nodeline (in segs2).
//   3 - If the first list (segs1) contains references to more than one
//       Sector or if the angle between two adjacent Segs is greater than
//       180 degrees, then call CreateNodes with this (smaller) list.
//       Else, create a SubSector with all these Segs.
//   4 - Do the same for the second list (segs2).
//   5 - Return the new node (its two children are already OK).
//
// Each time CreateSSector is called, the Segs are put in a global list.
// When there is no more Seg in CreateNodes' list, then they are all in the
// global list and ready to be saved to disk.
//
