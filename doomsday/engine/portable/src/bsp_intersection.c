/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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
 * bsp_intersection.c: Intersections and cutlists (lists of intersections).
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cnode_s {
    void       *data;
    struct cnode_s *next;
    struct cnode_s *prev;
} cnode_t;

// The intersection list is kept sorted by along_dist, in ascending order.
typedef struct clist_s {
    cnode_t    *headPtr;
} clist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static clist_t* UnusedIntersectionList;
static boolean initedOK = false;

// CODE --------------------------------------------------------------------

static cnode_t *allocCNode(void)
{
    return M_Malloc(sizeof(cnode_t));
}

static void freeCNode(cnode_t *node)
{
    M_Free(node);
}

static clist_t *allocCList(void)
{
    return M_Malloc(sizeof(clist_t));
}

static void freeCList(clist_t *list)
{
    M_Free(list);
}

static intersection_t *allocIntersection(void)
{
    return M_Calloc(sizeof(intersection_t));
}

static void freeIntersection(intersection_t *cut)
{
    M_Free(cut);
}

static intersection_t *quickAllocIntersection(void)
{
    intersection_t *cut;

    if(initedOK && UnusedIntersectionList->headPtr)
    {
        cnode_t    *node = UnusedIntersectionList->headPtr;

        // Unlink from the unused list.
        UnusedIntersectionList->headPtr = node->next;

        // Grab the intersection.
        cut = node->data;

        // Release the list node.
        freeCNode(node);
    }
    else
    {
        cut = allocIntersection();
    }

    return cut;
}

static void emptyCList(clist_t *list)
{
    cnode_t    *node;

    node = list->headPtr;
    while(node)
    {
        cnode_t    *p = node->next;

        BSP_IntersectionDestroy(node->data);
        freeCNode(node);

        node = p;
    }

    list->headPtr = NULL;
}

void BSP_InitIntersectionAllocator(void)
{
    if(!initedOK)
    {
        UnusedIntersectionList = M_Calloc(sizeof(clist_t));
        initedOK = true;
    }
}

void BSP_ShutdownIntersectionAllocator(void)
{
    if(UnusedIntersectionList)
    {
        cnode_t    *node;

        node = UnusedIntersectionList->headPtr;
        while(node)
        {
            cnode_t    *p = node->next;

            freeIntersection(node->data);
            freeCNode(node);

            node = p;
        }

        M_Free(UnusedIntersectionList);
    }
    UnusedIntersectionList = NULL;

    initedOK = false;
}

/**
 * Create a new intersection.
 */
intersection_t *BSP_IntersectionCreate(mvertex_t *vert, hedge_t *part,
                                       boolean selfRef)
{
    intersection_t *cut = quickAllocIntersection();

    cut->vertex = vert;
    cut->alongDist = ParallelDist(part, vert->V_pos[VX], vert->V_pos[VY]);
    cut->selfRef = selfRef;

    cut->before = BSP_VertexCheckOpen(vert, -part->pDX, -part->pDY);
    cut->after  = BSP_VertexCheckOpen(vert,  part->pDX,  part->pDY);

    return cut;
}

/**
 * Destroy the specified intersection.
 *
 * @param cut           Ptr to the intersection to be destroyed.
 */
void BSP_IntersectionDestroy(intersection_t *cut)
{
    if(initedOK)
    {   // If the allocator is initialized, move the intersection to the
        // unused list for reuse.
        cnode_t    *node = allocCNode();

        node->data = cut;
        node->next = UnusedIntersectionList->headPtr;
        node->prev = NULL;
        UnusedIntersectionList->headPtr = node;
    }
    else
    {   // Just free it.
        freeIntersection(cut);
    }
}

#if _DEBUG
void BSP_IntersectionPrint(intersection_t *cut)
{
    Con_Message("  Vertex %8X (%1.1f,%1.1f)  Along %1.2f  [%d/%d]  %s\n",
                cut->vertex->index, cut->vertex->V_pos[VX],
                cut->vertex->V_pos[VY], cut->alongDist,
                (cut->before? cut->before->index : -1),
                (cut->after? cut->after->index : -1),
                (cut->selfRef? "SELFREF" : ""));
}
#endif

/**
 * Create a new cutlist.
 */
cutlist_t *BSP_CutListCreate(void)
{
    clist_t    *list = allocCList();

    list->headPtr = NULL;

    return (cutlist_t*) list;
}

/**
 * Destroy a cutlist.
 */
void BSP_CutListDestroy(cutlist_t *cutList)
{
    if(cutList)
    {
        clist_t    *list = (clist_t*) cutList;

        emptyCList(list);
        freeCList(list);
    }
}

/**
 * Empty all intersections from the specified cutlist.
 */
void BSP_CutListEmpty(cutlist_t *cutList)
{
    if(cutList)
    {
        clist_t    *list = (clist_t*) cutList;
        emptyCList(list);
    }
}

/**
 * Search the given list for an intersection, if found; return it.
 *
 * @param list          The list to be searched.
 * @param vert          Ptr to the intersection vertex to look for.
 *
 * @return              Ptr to the found intersection, else @c NULL;
 */
intersection_t *BSP_CutListFindIntersection(cutlist_t *cutList, mvertex_t *v)
{
    clist_t    *list = (clist_t*) cutList;
    cnode_t    *node;

    node = list->headPtr;
    while(node)
    {
        intersection_t *cut = node->data;

        if(cut->vertex == v)
            return cut;

        node = node->next;
    }

    return NULL;
}

/**
 * Insert the given intersection into the specified cutlist.
 *
 * @return          @c true, if successful.
 */
boolean BSP_CutListInsertIntersection(cutlist_t *cutList, intersection_t *cut)
{
    if(cutList && cut)
    {
        clist_t    *list = (clist_t*) cutList;
        cnode_t    *newNode = allocCNode();
        cnode_t    *after;

        /**
         * Enqueue the new intersection into the list.
         */
        after = list->headPtr;
        while(after && after->next)
            after = after->next;

        while(after &&
              cut->alongDist < ((intersection_t *)after->data)->alongDist)
            after = after->prev;

        newNode->data = cut;

        // Link it in.
        newNode->next = (after? after->next : list->headPtr);
        newNode->prev = after;

        if(after)
        {
            if(after->next)
                after->next->prev = newNode;

            after->next = newNode;
        }
        else
        {
            if(list->headPtr)
                list->headPtr->prev = newNode;

            list->headPtr = newNode;
        }

        return true;
    }

    return false;
}

/**
 * Analyze the intersection list, and add any needed minihedges to the given
 * half-edge lists (one minihedge on each side).
 *
 * \note All the intersections in the cutList will be freed back into the
 * quick-alloc list for re-use!
 *
 * \todo Does this belong in here?
 */
void BSP_AddMiniHEdges(hedge_t *part, superblock_t *leftList,
                       superblock_t *rightList, cutlist_t *cutList)
{
    clist_t    *list;
    cnode_t    *node, *np;

    if(!cutList)
        return;

    list = (clist_t*) cutList;
/*
#if _DEBUG
BSP_CutListPrint(cutList);
Con_Message("PARTITION: (%1.1f,%1.1f) += (%1.1f,%1.1f)\n",
            part->pSX, part->pSY, part->pDX, part->pDY);
*/

    /**
     * Step 1: Fix problems in the intersection list...
     */
    node = list->headPtr;
    np = node->next;
    while(node && np)
    {
        intersection_t *cur = node->data;
        intersection_t *next = np->data;
        double      len = next->alongDist - cur->alongDist;

        if(len < -0.1)
            Con_Error("BSP_AddMiniHEdges: Bad order in intersect list - "
                      "%1.3f > %1.3f\n",
                      cur->alongDist, next->alongDist);

        if(len > 0.2)
        {
            node = np;
            np = node->next;
            continue;
        }

        if(len > DIST_EPSILON)
        {
/*
#if _DEBUG
Con_Message("Skipping very short half-edge (len=%1.3f) near "
            "(%1.1f,%1.1f)\n", len, cur->vertex->V_pos[VX],
            cur->vertex->V_pos[VY]);
#endif
*/
        }

        // Merge the two intersections into one.
/*
#if _DEBUG
Con_Message("Merging intersections:\n");
BSP_IntersectionPrint(cur);
BSP_IntersectionPrint(next);
#endif
*/
        if(cur->selfRef && !next->selfRef)
        {
            if(cur->before && next->before)
                cur->before = next->before;

            if(cur->after && next->after)
                cur->after = next->after;

            cur->selfRef = false;
        }

        if(!cur->before && next->before)
            cur->before = next->before;

        if(!cur->after && next->after)
            cur->after = next->after;
/*
#if _DEBUG
Con_Message("Result:\n");
BSP_IntersectionPrint(cur);
#endif
*/
        // Free the unused cut.
        node->next = np->next;

        BSP_IntersectionDestroy(next);

        np = node->next;
    }

    // Step 2: Find connections in the intersection list...
    node = list->headPtr;
    while(node && node->next)
    {
        intersection_t *cur = node->data;
        intersection_t *next = (node->next? node->next->data : NULL);

        if(!(!cur->after && !next->before))
        {
            // Check for some nasty open/closed or close/open cases.
            if(cur->after && !next->before)
            {
                if(!cur->selfRef && !cur->after->warnedUnclosed)
                {
                    VERBOSE(
                    Con_Message("Sector #%d is unclosed near (%1.1f,%1.1f)\n",
                                cur->after->index,
                                (cur->vertex->V_pos[VX] + next->vertex->V_pos[VX]) / 2.0,
                                (cur->vertex->V_pos[VY] + next->vertex->V_pos[VY]) / 2.0));
                    cur->after->warnedUnclosed = true;
                }
            }
            else if(!cur->after && next->before)
            {
                if(!next->selfRef && !next->before->warnedUnclosed)
                {
                    VERBOSE(
                    Con_Message("Sector #%d is unclosed near (%1.1f,%1.1f)\n",
                                next->before->index,
                                (cur->vertex->V_pos[VX] + next->vertex->V_pos[VX]) / 2.0,
                                (cur->vertex->V_pos[VY] + next->vertex->V_pos[VY]) / 2.0));
                    next->before->warnedUnclosed = true;
                }
            }
            else
            {   // This is definitetly open space.

                // Do a sanity check on the sectors (just for good measure).
                if(cur->after != next->before)
                {
                    if(!cur->selfRef && !next->selfRef)
                    {
                        VERBOSE(
                        Con_Message("Sector mismatch: #%d (%1.1f,%1.1f) != #%d "
                                    "(%1.1f,%1.1f)\n",
                                    cur->after->index, cur->vertex->V_pos[VX],
                                    cur->vertex->V_pos[VY], next->before->index,
                                    next->vertex->V_pos[VX], next->vertex->V_pos[VY]));
                    }

                    // Choose the non-self-referencing sector when we can.
                    if(cur->selfRef && !next->selfRef)
                    {
                        cur->after = next->before;
                    }
                }

                {
                hedge_t    *right, *left;

                BSP_BuildEdgeBetweenIntersections(part, cur, next, &right, &left);

                // Add the new half-edges to the appropriate lists.
                BSP_AddHEdgeToSuperBlock(rightList, right);
                BSP_AddHEdgeToSuperBlock(leftList, left);
                }
            }
        }

        node = node->next;
    }

    BSP_CutListEmpty(cutList);
}

#if _DEBUG
void BSP_CutListPrint(cutlist_t *cutList)
{
    if(cutList)
    {
        clist_t    *list = (clist_t*) cutList;
        cnode_t    *node;

        Con_Message("CutList %p:\n", list);
        node = list->headPtr;
        while(node)
        {
            intersection_t *cut = node->data;
            BSP_IntersectionPrint(cut);
            node = node->next;
        }
    }
}
#endif
