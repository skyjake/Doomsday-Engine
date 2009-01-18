/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
    void*       data;
    struct cnode_s* next;
    struct cnode_s* prev;
} cnode_t;

// The intersection list is kept sorted by along_dist, in ascending order.
typedef struct clist_s {
    cnode_t*    headPtr;
} clist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static clist_t* UnusedIntersectionList;
static cnode_t* unusedCNodes;

static boolean initedOK = false;

// CODE --------------------------------------------------------------------

static cnode_t* allocCNode(void)
{
    return M_Malloc(sizeof(cnode_t));
}

static void freeCNode(cnode_t* node)
{
    M_Free(node);
}

static cnode_t* quickAllocCNode(void)
{
    cnode_t*            node;

    if(initedOK && unusedCNodes)
    {
        node = unusedCNodes;
        unusedCNodes = unusedCNodes->next;
    }
    else
    {   // Need to allocate another.
        node = allocCNode();
    }

    node->data = NULL;
    node->next = node->prev = NULL;

    return node;
}

static clist_t* allocCList(void)
{
    return M_Malloc(sizeof(clist_t));
}

static void freeCList(clist_t* list)
{
    M_Free(list);
}

static intersection_t* allocIntersection(void)
{
    return M_Calloc(sizeof(intersection_t));
}

static void freeIntersection(intersection_t* cut)
{
    M_Free(cut);
}

static intersection_t* quickAllocIntersection(void)
{
    intersection_t*     cut;

    if(initedOK && UnusedIntersectionList->headPtr)
    {
        cnode_t*            node = UnusedIntersectionList->headPtr;

        // Unlink from the unused list.
        UnusedIntersectionList->headPtr = node->next;

        // Grab the intersection.
        cut = node->data;

        // Move the list node to the unused node list.
        node->next = unusedCNodes;
        unusedCNodes = node;
    }
    else
    {
        cut = allocIntersection();
    }

    memset(cut, 0, sizeof(*cut));

    return cut;
}

static void emptyCList(clist_t* list)
{
    cnode_t*            node;

    node = list->headPtr;
    while(node)
    {
        cnode_t*            p = node->next;

        BSP_IntersectionDestroy(node->data);

        // Move the list node to the unused node list.
        node->next = unusedCNodes;
        unusedCNodes = node;

        node = p;
    }

    list->headPtr = NULL;
}

void BSP_InitIntersectionAllocator(void)
{
    if(!initedOK)
    {
        UnusedIntersectionList = M_Calloc(sizeof(clist_t));
        unusedCNodes = NULL;
        initedOK = true;
    }
}

void BSP_ShutdownIntersectionAllocator(void)
{
    if(UnusedIntersectionList)
    {
        cnode_t*            node;

        node = UnusedIntersectionList->headPtr;
        while(node)
        {
            cnode_t*            p = node->next;

            freeIntersection(node->data);
            freeCNode(node);

            node = p;
        }

        M_Free(UnusedIntersectionList);
        UnusedIntersectionList = NULL;
    }

    if(unusedCNodes)
    {
        cnode_t*            node;

        node = unusedCNodes;
        while(node)
        {
            cnode_t*            np = node->next;

            freeCNode(node);
            node = np;
        }

        unusedCNodes = NULL;
    }

    initedOK = false;
}

/**
 * Create a new intersection.
 */
intersection_t* BSP_IntersectionCreate(vertex_t* vert,
                                       const struct bspartition_s* part,
                                       boolean selfRef)
{
    intersection_t*     cut = quickAllocIntersection();

    cut->vertex = vert;
    cut->alongDist =
        M_ParallelDist(part->pDX, part->pDY, part->pPara, part->length,
                       vert->buildData.pos[VX], vert->buildData.pos[VY]);
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
void BSP_IntersectionDestroy(intersection_t* cut)
{
    if(initedOK)
    {   // If the allocator is initialized, move the intersection to the
        // unused list for reuse.
        cnode_t*            node = quickAllocCNode();

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
void BSP_IntersectionPrint(intersection_t* cut)
{
    Con_Message("  Vertex %8X (%1.1f,%1.1f)  Along %1.2f  [%d/%d]  %s\n",
                cut->vertex->buildData.index, cut->vertex->buildData.pos[VX],
                cut->vertex->buildData.pos[VY], cut->alongDist,
                (cut->before? cut->before->buildData.index : -1),
                (cut->after? cut->after->buildData.index : -1),
                (cut->selfRef? "SELFREF" : ""));
}
#endif

/**
 * Create a new cutlist.
 */
cutlist_t* BSP_CutListCreate(void)
{
    clist_t*            list = allocCList();

    list->headPtr = NULL;

    return (cutlist_t*) list;
}

/**
 * Destroy a cutlist.
 */
void BSP_CutListDestroy(cutlist_t* cutList)
{
    if(cutList)
    {
        clist_t*            list = (clist_t*) cutList;

        emptyCList(list);
        freeCList(list);
    }
}

/**
 * Empty all intersections from the specified cutlist.
 */
void BSP_CutListEmpty(cutlist_t* cutList)
{
    if(cutList)
    {
        clist_t*            list = (clist_t*) cutList;
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
intersection_t* BSP_CutListFindIntersection(cutlist_t* cutList, vertex_t* v)
{
    clist_t*            list = (clist_t*) cutList;
    cnode_t*            node;

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
 * @return              @c true, if successful.
 */
boolean BSP_CutListInsertIntersection(cutlist_t* cutList,
                                      intersection_t* cut)
{
    if(cutList && cut)
    {
        clist_t*            list = (clist_t*) cutList;
        cnode_t*            newNode = quickAllocCNode();
        cnode_t*            after;

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

static void buildEdgeBetweenIntersections(const bspartition_t* part,
                                          intersection_t* start,
                                          intersection_t* end,
                                          hedge_t** right, hedge_t** left)
{
    // Create the half-edge pair.
    // Leave 'linedef' field as NULL as these are not linedef-linked.
    // Leave 'side' as zero too.
    (*right) = HEdge_Create(NULL, part->lineDef, start->vertex,
                            end->vertex, start->after, false);
    (*left)  = HEdge_Create(NULL, part->lineDef, end->vertex,
                            start->vertex, start->after, false);

    // Twin the half-edges together.
    (*right)->twin = *left;
    (*left)->twin = *right;

/*#if _DEBUG
Con_Message("buildEdgeBetweenIntersections: Capped intersection:\n");
Con_Message("  %p RIGHT sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            (*right), ((*right)->sector? (*right)->sector->index : -1),
            (*right)->v[0]->V_pos[VX], (*right)->v[0]->V_pos[VY],
            (*right)->v[1]->V_pos[VX], (*right)->v[1]->V_pos[VY]);

Con_Message("  %p LEFT  sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            (*left), ((*left)->sector? (*left)->sector->index : -1),
            (*left)->v[0]->V_pos[VX], (*left)->v[0]->V_pos[VY],
            (*left)->v[1]->V_pos[VX], (*left)->v[1]->V_pos[VY]);
#endif*/
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
void BSP_AddMiniHEdges(const bspartition_t* part, superblock_t* rightList,
                       superblock_t* leftList, cutlist_t* cutList)
{
    clist_t*            list;
    cnode_t*            node, *np;

    if(!cutList)
        return;

    list = (clist_t*) cutList;
/*
#if _DEBUG
BSP_CutListPrint(cutList);
Con_Message("BSP_AddMiniHEdges: Partition (%1.1f,%1.1f) += (%1.1f,%1.1f)\n",
            part->pSX, part->pSY, part->pDX, part->pDY);
*/

    /**
     * Step 1: Fix problems in the intersection list...
     */
    node = list->headPtr;
    np = node->next;
    while(node && np)
    {
        intersection_t*     cur = node->data;
        intersection_t*     next = np->data;
        double              len = next->alongDist - cur->alongDist;

        if(len < -0.1)
        {
            Con_Error("BSP_AddMiniHEdges: Bad order in intersect list - "
                      "%1.3f > %1.3f\n",
                      cur->alongDist, next->alongDist);
        }
        else if(len > 0.2)
        {
            node = np;
            np = node->next;
            continue;
        }
        else if(len > DIST_EPSILON)
        {
/*
#if _DEBUG
Con_Message(" Skipping very short half-edge (len=%1.3f) near "
            "(%1.1f,%1.1f)\n", len, cur->vertex->V_pos[VX],
            cur->vertex->V_pos[VY]);
#endif
*/
        }

        // Merge the two intersections into one.
/*
#if _DEBUG
Con_Message(" Merging intersections:\n");
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
Con_Message(" Result:\n");
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
        intersection_t*     cur = node->data;
        intersection_t*     next = (node->next? node->next->data : NULL);

        if(!(!cur->after && !next->before))
        {
            // Check for some nasty open/closed or close/open cases.
            if(cur->after && !next->before)
            {
                if(!cur->selfRef)
                {
                    double              pos[2];

                    pos[VX] = cur->vertex->buildData.pos[VX] +
                        next->vertex->buildData.pos[VX];
                    pos[VY] = cur->vertex->buildData.pos[VY] +
                        next->vertex->buildData.pos[VY];

                    pos[VX] /= 2;
                    pos[VY] /= 2;

                    MPE_RegisterUnclosedSectorNear(cur->after,
                                                   pos[VX], pos[VY]);
                }
            }
            else if(!cur->after && next->before)
            {
                if(!next->selfRef)
                {
                    double              pos[2];

                    pos[VX] = cur->vertex->buildData.pos[VX] +
                        next->vertex->buildData.pos[VX];
                    pos[VY] = cur->vertex->buildData.pos[VY] +
                        next->vertex->buildData.pos[VY];
                    pos[VX] /= 2;
                    pos[VY] /= 2;

                    MPE_RegisterUnclosedSectorNear(next->before,
                                                   pos[VX], pos[VY]);
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
                                    cur->after->buildData.index, cur->vertex->buildData.pos[VX],
                                    cur->vertex->buildData.pos[VY], next->before->buildData.index,
                                    next->vertex->buildData.pos[VX], next->vertex->buildData.pos[VY]));
                    }

                    // Choose the non-self-referencing sector when we can.
                    if(cur->selfRef && !next->selfRef)
                    {
                        cur->after = next->before;
                    }
                }

                {
                hedge_t*            right, *left;

                buildEdgeBetweenIntersections(part, cur, next, &right, &left);

                // Add the new half-edges to the appropriate lists.
                BSP_AddHEdgeToSuperBlock(rightList, right);
                BSP_AddHEdgeToSuperBlock(leftList, left);
                }
            }
        }

        node = node->next;
    }
}

#if _DEBUG
void BSP_CutListPrint(cutlist_t* cutList)
{
    if(cutList)
    {
        clist_t*            list = (clist_t*) cutList;
        cnode_t*            node;

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
