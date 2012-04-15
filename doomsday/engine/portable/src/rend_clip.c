/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Angle Clipper (clipnodes and oranges).
 *
 * The idea is to keep track of occluded angles around the camera.
 * Since BSP leafs are rendered front-to-back, the occlusion lists
 * start a frame empty and eventually fill up to cover the whole 360
 * degrees around the camera.
 *
 * Oranges (occlusion ranges) clip a half-space on an angle range.
 * These are produced by horizontal edges that have empty space behind.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef _DEBUG
#  include <assert.h>
#endif

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define OCNF_TOPHALF    0x2     // Otherwise bottom half.

// TYPES -------------------------------------------------------------------

typedef struct rovernode_s {
    struct rovernode_s *next, *prev;
} rovernode_t;

typedef struct rover_s {
    rovernode_t *first;
    rovernode_t *last;
    rovernode_t *rover;
} rover_t;

typedef struct clipnode_s {
    rovernode_t rover;
    struct clipnode_s *prev, *next; // Previous and next nodes.
    binangle_t start, end;      // The start and end angles
} clipnode_t;                   // (start < end).

typedef struct occnode_s {
    rovernode_t rover;
    struct occnode_s *next, *prev;
    byte    flags;              // Used and tophalf.
    binangle_t start, end;      // Start and end angles of the segment (s<e).
    float   normal[3];          // The normal of the occlusion plane.
} occnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int     C_IsAngleVisible(binangle_t bang);
void    C_CutOcclusionRange(binangle_t startAngle, binangle_t endAngle);
static int C_SafeCheckRange(binangle_t startAngle, binangle_t endAngle);
clipnode_t *C_AngleClippedBy(binangle_t bang);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The list of clipnodes.
rover_t clipNodes;

// Head of the clipped regions list.
clipnode_t *clipHead;           // The head node.

// The list of occlusion nodes.
rover_t occNodes;

// Head of the occlusion range list.
occnode_t *occHead;             // The head occlusion node.

int     devNoCulling = 0;    // Set to 1 to disable culling completely

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint  anglistSize = 0;
static binangle_t *anglist;

// CODE --------------------------------------------------------------------

/*
#if _DEBUG
static void C_CountNodes(void)
{
    int         i;
    clipnode_t *ci;

    for(i=0, ci=clipHead; ci; ++i, ci=ci->next);
    if(i > maxnumnodes)
        maxnumnodes = i;
}

static int C_CountUsedOranges(void)
{
    int         cnt;
    occnode_t  *orange;

    for(cnt = 0, orange = occHead; orange; orange = orange->next, cnt++);

    return cnt;
}
#endif
*/

void C_RoverInit(rover_t *rover)
{
    memset(rover, 0, sizeof(*rover));
}

void C_RoverRewind(rover_t *r)
{
    r->rover = r->first;
}

void *C_RoverGet(rover_t *r)
{
    void       *node;

    if(!r->rover)
        return NULL;

    // We'll use this.
    node = r->rover;
    r->rover = r->rover->next;
    return node;
}

void C_RoverAdd(rover_t *r, void *node)
{
    // Link it to the start of the rover's list.
    if(!r->last)
        r->last = node;
    if(r->first)
        r->first->prev = node;

    ((rovernode_t*) node)->next = r->first;
    ((rovernode_t*) node)->prev = NULL;
    r->first = node;
}

void C_RoverRemove(rover_t *r, void *node)
{
#ifdef _DEBUG
    assert(r->last != NULL);
#endif

    if(node == r->last)
    {
#ifdef _DEBUG
        assert(r->rover == NULL);
#endif
        // We can only remove the last if all nodes are already in use.
        r->rover = node;
    }
    else
    {
        // Unlink from the list entirely.
        ((rovernode_t*) node)->next->prev = ((rovernode_t*) node)->prev;
        if(((rovernode_t*) node)->prev)
        {
            ((rovernode_t*) node)->prev->next = ((rovernode_t*) node)->next;
        }
        else
        {
            r->first = r->first->next;
            r->first->prev = NULL;
        }

        // Put it back to the end of the list.
        r->last->next = node;
        ((rovernode_t*) node)->prev = r->last;
        ((rovernode_t*) node)->next = NULL;
        r->last = node;

        // If all were in use, set the rover here. Otherwise the rover
        // can stay where it is.
        if(!r->rover)
            r->rover = r->last;
    }
}

void C_Init(void)
{
    C_RoverInit(&clipNodes);
    C_RoverInit(&occNodes);
}

void C_ClearRanges(void)
{
    clipHead = NULL;

    // Rewind the rover.
    C_RoverRewind(&clipNodes);

    occHead = NULL;

    // Rewind the rover.
    C_RoverRewind(&occNodes);
}

/**
 * Finds the first unused clip node.
 */
static clipnode_t *C_NewRange(binangle_t stAng, binangle_t endAng)
{
    clipnode_t *node;

    if((node = C_RoverGet(&clipNodes)) == NULL)
    {
        // Allocate a new node and add it to head the list.
        node = Z_Malloc(sizeof(clipnode_t), PU_APPSTATIC, NULL);
        C_RoverAdd(&clipNodes, node);
    }

    // Initialize the node.
    node->start = stAng;
    node->end = endAng;
    node->prev = node->next = NULL;
    return node;
}

static void C_RemoveRange(clipnode_t *node)
{
    // If this is the head, move it.
    if(clipHead == node)
        clipHead = node->next;

    // Unlink from the clipper.
    if(node->prev)
        node->prev->next = node->next;
    if(node->next)
        node->next->prev = node->prev;
    node->prev = node->next = 0;

    // Move this node to the free node rover.
    C_RoverRemove(&clipNodes, node);
}

static void C_AddRange(binangle_t startAngle, binangle_t endAngle)
{
    clipnode_t *ci, *crange;

    // This range becomes a solid segment: cut everything away from the
    // corresponding occlusion range.
    C_CutOcclusionRange(startAngle, endAngle);

    // If there is no head, this will be the first range.
    if(!clipHead)
    {
        clipHead = C_NewRange(startAngle, endAngle);
/*
#if _DEBUG
printf( "  new head node added, %x => %x\n", clipHead->start, clipHead->end);
#endif
*/
        return;
    }

    // There are previous ranges. Check that the new range isn't contained
    // by any of them.
    for(ci = clipHead; ci; ci = ci->next)
    {
/*
#if _DEBUG
printf( "      %p: %4x => %4x (%d)\n",ci,ci->start,ci->end,ci->used);
#endif
*/
        if(startAngle >= ci->start && endAngle <= ci->end)
        {
/*
#if _DEBUG
printf( "  range already exists\n");
#endif
*/
            return;             // The new range already exists.
        }
/*
#if _DEBUG
printf( "loop1\n");
if(ci == ci->next)
   Con_Error("%p linked to itself: %x => %x\n",ci,ci->start,ci->end);
#endif
*/
    }

    // Now check if any of the old ranges are contained by the new one.
    for(ci = clipHead; ci;)
    {
/*
#if _DEBUG
printf( "loop2\n");
#endif
*/
        if(ci->start >= startAngle && ci->end <= endAngle)
        {
            crange = ci;
/*
#if _DEBUG
//printf( "  removing contained range %x => %x\n",crange->start,crange->end);
#endif
*/
            // We must do this in order to keep the loop from breaking.
            ci = ci->next;
            C_RemoveRange(crange);
            //if(!ci) ci = clipHead;
            //if(!ci) break;
            continue;
        }
        ci = ci->next;
    }

    // Now it is possible that the new range overlaps one or two old ranges.
    // If two are overlapped, they are consecutive. First we'll try to find
    // a range that overlaps the beginning.
    for(crange = 0, ci = clipHead; ci; ci = ci->next)
    {
        // In preparation for the next stage, find a good spot for the range.
        if(ci->start < endAngle)
            crange = ci;        // Add after this one.
/*
#if _DEBUG
printf( "loop3\n");
#endif
*/
        if(ci->start >= startAngle && ci->start <= endAngle)
        {
            // New range's end and ci's beginning overlap. ci's end is outside.
            // Otherwise it would have been already removed.
            // It suffices to adjust ci.
/*
#if _DEBUG
printf( "  overlapping beginning with %x => %x, ",ci->start,ci->end);
#endif
*/
            ci->start = startAngle;
/*
#if _DEBUG
printf( "adjusting ci to %x => %x\n",ci->start,ci->end);
#endif
*/
            return;
        }
        // Check an overlapping end.
        if(ci->end >= startAngle && ci->end <= endAngle)
        {
            // Now it's possible that the ci->next's beginning overlaps the new
            // range's end. In that case there will be a merger.
/*
#if _DEBUG
printf( "  overlapping end with %x => %x:\n",ci->start,ci->end);
#endif
*/
            crange = ci->next;
            if(!crange)
            {
                ci->end = endAngle;
/*
#if _DEBUG
printf( "    no next, adjusting end (now %x => %x)\n",ci->start,ci->end);
#endif
*/
            }
            else
            {
                if(crange->start <= endAngle)
                {
                    // A fusion will commence. Ci will eat the new range
                    // *and* crange.
                    ci->end = crange->end;
/*
#if _DEBUG
printf( "    merging with the next (%x => %x)\n",crange->start,crange->end);
#endif
*/
                    C_RemoveRange(crange);
                }
                else
                {
                    // Not overlapping.
                    ci->end = endAngle;
/*
#if _DEBUG
printf( "    not merger w/next, ci is now %x => %x\n",ci->start,ci->end);
#endif
*/
                }
            }
            return;
        }
    }

    // Still here? Now we know for sure that the range is disconnected from
    // the others. We still need to find a good place for it. Crange will mark
    // the spot.

    if(!crange)
    {
        // We have a new head node.
        crange = clipHead;
        clipHead = C_NewRange(startAngle, endAngle);
        clipHead->next = crange;
        if(crange)
            crange->prev = clipHead;
    }
    else
    {
        // Add the new range after crange.
        ci = C_NewRange(startAngle, endAngle);
        ci->next = crange->next;
        if(ci->next)
            ci->next->prev = ci;
        ci->prev = crange;
        crange->next = ci;
    }
}

occnode_t *C_NewOcclusionRange(binangle_t stAng, binangle_t endAng,
                               float *normal, boolean topHalf)
{
    occnode_t  *node;

    if((node = C_RoverGet(&occNodes)) == NULL)
    {
        // Allocate a new node.
        node = Z_Malloc(sizeof(occnode_t), PU_APPSTATIC, NULL);
        C_RoverAdd(&occNodes, node);
    }

    node->flags = (topHalf ? OCNF_TOPHALF : 0);
    node->start = stAng;
    node->end = endAng;
    memcpy(node->normal, normal, sizeof(node->normal));
    return node;
}

void C_RemoveOcclusionRange(occnode_t *orange)
{
    // If this is the head, move it to the next one.
    if(occHead == orange)
        occHead = orange->next;

    if(orange->prev)
        orange->prev->next = orange->next;
    if(orange->next)
        orange->next->prev = orange->prev;

    C_RoverRemove(&occNodes, orange);
}

/**
 * The given range must be safe.
 */
void C_AddOcclusionRange(binangle_t start, binangle_t end, float *normal,
                         boolean topHalf)
{
    occnode_t      *orange = NULL, *newor = NULL, *last = NULL;
    boolean         done;

    // Is the range valid?
    if(start > end)
        return;

    // A new range will be added.
    newor = C_NewOcclusionRange(start, end, normal, topHalf);

    // Are there any previous occlusion nodes?
    if(!occHead)
    {
        // No; this is the first.
        occHead = newor;
        occHead->next = occHead->prev = NULL;
        return;
    }

    // \fixme An optimization: remove existing oranges that are fully
    // contained by the new orange. But how to do the check efficiently?

    // Add the new occlusion range to the appropriate position.
    orange = occHead;
    done = false;
    while(orange && !done)
    {
        // The list of oranges is sorted by the start angle.
        // Find the first range whose start is greater than the new one.
        if(orange->start > start)
        {
            // Add before this one.
            newor->next = orange;
            newor->prev = orange->prev;
            orange->prev = newor;
            if(newor->prev)
                newor->prev->next = newor;
            else
                occHead = newor;    // We have a new head.

            done = true;
        }
        else
        {
            last = orange;
            orange = orange->next;
        }
    }
    if(done)
        return;

    // All right, add the new range to the end of the list.
    last->next = newor;
    newor->prev = last;
    newor->next = NULL;
}

#if _DEBUG
void C_OrangeRanger(int mark)
{
    occnode_t  *orange;

    for(orange = occHead; orange; orange = orange->next)
        if(orange->prev && orange->prev->start > orange->start)
            Con_Error("C_OrangeRanger(%i): Orange order has failed.\n", mark);
}

void C_OcclusionLister(char *title)
{
    occnode_t  *orange;

    printf("*** %s:\n", title);
    for(orange = occHead; orange; orange = orange->next)
        printf("  %04x-%04x (%i)\n", orange->start, orange->end,
               (orange->flags & OCNF_TOPHALF) != 0);
}
#endif

/**
 * Attempts to merge the two given occnodes.
 *
 * @return      0: Could not be merged.
 *              1: orange was merged into other.
 *              2: other was merged into orange.
 */
static int C_TryMergeOccludes(occnode_t *orange, occnode_t *other)
{
    binangle_t  crossAngle;
    float       cross[3];

    // We can't test this steep planes.
    if(!orange->normal[VZ])
        return 0;

    // Where do they cross?
    M_CrossProduct(orange->normal, other->normal, cross);
    if(!cross[VX] && !cross[VY] && !cross[VZ])
    {
        // These two planes are exactly the same! Remove one.
        C_RemoveOcclusionRange(orange);
        return 1;
    }

    // The cross angle must be outside the range.
    crossAngle = bamsAtan2((int) cross[VY], (int) cross[VX]);
    if(crossAngle >= orange->start && crossAngle <= orange->end)
        return 0;       // Crosses inside the range, can't do a thing.

    // \fixme Isn't it possible to consistently determine which
    // direction the cross vector is pointing to?
    crossAngle += BANG_180;
    if(crossAngle >= orange->start && crossAngle <= orange->end)
        return 0;       // Crosses inside the range, can't do a thing.

    // Now we must determine which plane occludes which.
    // Pick a point in the middle of the range.
    crossAngle = (orange->start + orange->end) >> (1 + BAMS_BITS - 13);
    cross[VX] = 100 * FIX2FLT(fineCosine[crossAngle]);
    cross[VY] = 100 * FIX2FLT(finesine[crossAngle]);
    // z = -(A*x+B*y)/C
    cross[VZ] =
        -(orange->normal[VX] * cross[VX] +
          orange->normal[VY] * cross[VY]) / orange->normal[VZ];

    // Is orange occluded by the other one?
    if(M_DotProduct(cross, other->normal) < 0)
    {
        // No; then the other one is occluded by us.
        // Remove it instead.
        C_RemoveOcclusionRange(other);
        return 2;
    }
    else
    {
        C_RemoveOcclusionRange(orange);
        return 1;
    }
}

/**
 * Try to merge oranges with matching ranges. (Quite a number may be
 * produced as a result of the cuts.)
 */
void C_MergeOccludes(void)
{
    occnode_t  *orange, *next, *other;
    boolean     stopScan, isDone;

    orange = occHead;
    stopScan = false;
    while(!stopScan)
    {
        if(orange && orange->next)
        {
            next = orange->next;

            // Find a good one to test with.
            isDone = false;
            other = orange->next;
            while(!isDone)
            {
                if(other && orange->start == other->start)
                {
                    if(orange->end == other->end &&
                        (other->flags & OCNF_TOPHALF) ==
                        (orange->flags & OCNF_TOPHALF))
                    {
                        // It is a candidate for merging.
                        int result = C_TryMergeOccludes(orange, other);
                        if(result == 2)
                            next = next->next;

                        isDone = true;
                    }
                    else
                    {
                        // Move on to the next candidate.
                        other = other->next;
                    }
                }
                else
                    isDone = true;
            }

            orange = next;
        }
        else
            stopScan = true;
    }
}

/**
 * Everything in the given range is removed from the occlusion nodes.
 */
void C_CutOcclusionRange(binangle_t startAngle, binangle_t endAngle)
{
    occnode_t  *orange, *next, *after, *part;
    boolean     isDone;

#if _DEBUG
C_OrangeRanger(1);
#endif

    // Find the node after which it's OK to add oranges cut in half.
    // (Must preserve the ascending order of the start angles.)
    after = NULL;
    orange = occHead;
    isDone = false;
    while(!isDone)
    {
        // We want the orange with the smallest start angle, but one that
        // starts after the cut range has ended.
        if(orange && orange->start < endAngle)
        {
            after = orange;
        }
        else
            isDone = true;

        if(!isDone)
            orange = orange->next;
    }

    orange = occHead;
    isDone = false;
    while(!isDone)
    {
        if(orange)
        {
            // In case orange is removed, take a copy of the next one.
            next = orange->next;

            // Does the cut range include this orange?
            if(startAngle <= orange->end)
            {
                if(orange->start < endAngle)
                {
#define C_O_RELATIONSHIP(start, startangle, end, endangle) (( \
                    (start >= startAngle && end   <= endAngle)?  0: \
                    (start >= startAngle && start <  endAngle)?  1: \
                    (end    > startAngle && end   <= endAngle)?  2: \
                    (start <= startAngle && end   >= endAngle)?  3: -1))

                    // Four options:
                    switch(C_O_RELATIONSHIP(orange->start, startAngle,
                                            orange->end, endAngle))
                    {
                    case 0: // The cut range completely includes this orange.

                        // Fully contained; this orange will be removed.
                        C_RemoveOcclusionRange(orange);
                        break;

                    case 1: // The cut range contains the beginning of the orange.

                        // Cut away the beginning of this orange.
                        orange->start = endAngle;
                        // Even thought the start angle is modified, we don't need to
                        // move this orange anywhere. This is because after the cut there
                        // will be no oranges beginning inside the cut range.
                        break;

                    case 2: // The cut range contains the end of the orange.

                        // Cut away the end of this orange.
                        orange->end = startAngle;
                        break;

                    case 3: // The orange contains the whole cut range.

                        // The orange gets cut in two parts. Create a new orange that
                        // represents the end, and add it after the 'after' node, or to
                        // the head of the list.
                        part =
                            C_NewOcclusionRange(endAngle, orange->end, orange->normal,
                                                (orange->flags & OCNF_TOPHALF) != 0);
                        part->prev = after;
                        if(after)
                        {
                            part->next = after->next;
                            after->next = part;
                        }
                        else
                        {
                            // Add to the head.
                            part->next = occHead;
                            occHead = part;
                        }

                        if(part->next)
                            part->next->prev = part;

                        // Modify the start part.
                        orange->end = startAngle;
                        break;

                    default: // No meaningful relationship (in this context).
                        break;
                    }
                }
                else
                    isDone = true; // No more possible cuts.
            }

            if(!isDone)
                orange = next;
        }
        else
            isDone = true;
    }

#if _DEBUG
C_OrangeRanger(2);
#endif

    C_MergeOccludes();

#if _DEBUG
C_OrangeRanger(6);
#endif
}

/**
 * A debugging aid: checks if clipnode links are valid.
 */
#if _DEBUG
void C_Ranger(void)
{
    clipnode_t *ci;

    //printf("Ranger:\n");
    for(ci = clipHead; ci; ci = ci->next)
    {
        if(ci == clipHead)
        {
            if(ci->prev != 0)
                Con_Error("Cliphead->prev != 0.\n");
        }
        // Confirm that the links to prev and next are OK.
        if(ci->prev)
        {
            if(ci->prev->next != ci)
                Con_Error("Prev->next != this\n");
        }
        else if(ci != clipHead)
        {
            Con_Error("prev == null, this isn't clipHead.\n");
        }

        if(ci->next)
        {
            if(ci->next->prev != ci)
                Con_Error("Next->prev != this\n");
        }
    }
}
#endif

int C_SafeAddRange(binangle_t startAngle, binangle_t endAngle)
{
    // The range may wrap around.
    if(startAngle > endAngle)
    {
        // The range has to added in two parts.
        C_AddRange(startAngle, BANG_MAX);
        C_AddRange(0, endAngle);
    }
    else
    {
        // Add the range as usual.
        C_AddRange(startAngle, endAngle);
    }
    return true;
}

/**
 * Add a segment relative to the current viewpoint.
 */
void C_AddViewRelSeg(float x1, float y1, float x2, float y2)
{
    float       dx1 = x1 - vx;
    float       dy1 = y1 - vz;
    float       dx2 = x2 - vx;
    float       dy2 = y2 - vz;

    C_SafeAddRange(bamsAtan2((int) (dy2 * 100), (int) (dx2 * 100)),
                   bamsAtan2((int) (dy1 * 100), (int) (dx1 * 100)));
}

/**
 * If necessary, cut the given range in two.
 */
void C_SafeAddOcclusionRange(binangle_t startAngle, binangle_t endAngle,
                             float *normal, boolean tophalf)
{
    // Is this range already clipped?
    if(!C_SafeCheckRange(startAngle, endAngle))
        return;                 // Already clipped.

    if(startAngle > endAngle)
    {
        // The range has to added in two parts.
        C_AddOcclusionRange(startAngle, BANG_MAX, normal, tophalf);
#if _DEBUG
        C_OrangeRanger(3);
#endif
        C_AddOcclusionRange(0, endAngle, normal, tophalf);
#if _DEBUG
        C_OrangeRanger(4);
#endif

    }
    else
    {
        // Add the range as usual.
        C_AddOcclusionRange(startAngle, endAngle, normal, tophalf);
#if _DEBUG
        C_OrangeRanger(5);
#endif

    }
}

/**
 * The point should be view-relative!
 */
binangle_t C_PointToAngle(float *point)
{
    return bamsAtan2((int) (point[VY] * 100), (int) (point[VX] * 100));
}

/**
 * Add an occlusion segment relative to the current viewpoint.
 */
void C_AddViewRelOcclusion(float *v1, float *v2, float height, boolean tophalf)
{
    float       viewtov1[3], viewtov2[3];
    float       normal[3];

    // \fixme Optimization? Check if the given line is already occluded.

    // Calculate the occlusion plane normal. We'll use the game's coordinate
    // system (left-handed, but Y and Z are swapped).
    viewtov1[VX] = v1[VX] - vx;
    viewtov1[VY] = v1[VY] - vz;
    viewtov2[VX] = v2[VX] - vx;
    viewtov2[VY] = v2[VY] - vz;
    viewtov1[VZ] = viewtov2[VZ] = height - vy;
    // The normal points to the half we want to occlude.
    M_CrossProduct(tophalf ? viewtov2 : viewtov1,
                   tophalf ? viewtov1 : viewtov2, normal);

#if _DEBUG
{
    float   testPos[3];

    testPos[0] = 0;
    testPos[1] = 0;
    testPos[2] = (tophalf ? 1000 : -1000);

    if(M_DotProduct(testPos, normal) < 0)
    {
        Con_Error("C_AddViewRelOcclusion: Wrong side - [%g,%g]>[%g,%g] "
                  "view[%g,%g]!\n", v1[VX], v1[VY], v2[VX], v2[VY], vx, vz);
    }
}
#endif

    C_SafeAddOcclusionRange(C_PointToAngle(viewtov2), C_PointToAngle(viewtov1),
                            normal, tophalf);
}

/**
 * Returns true if the view relative point is occluded by an occlusion
 * range.
 */
boolean C_IsPointOccluded(float *viewrelpoint)
{
    binangle_t  angle = C_PointToAngle(viewrelpoint);
    occnode_t  *orange;

    for(orange = occHead; orange; orange = orange->next)
    {
        if(angle >= orange->start && angle <= orange->end)
        {
            if(orange->start > angle)
                return false;       // No more possibilities.

            // On which side of the occlusion plane is it? The positive side
            // is the occluded one.
            if(M_DotProduct(viewrelpoint, orange->normal) > 0)
                return true;        // Occluded!
        }
    }

    // No orange occluded the point.
    return false;
}

/**
 * Returns true if the point is visible after checking both the clipnodes
 * and the occlusion planes. Note that this test can only be done with
 * points that reside in BSP leafs that have not yet been rendered.
 */
boolean C_IsPointVisible(float x, float y, float height)
{
    float       point[3];
    binangle_t  angle;

    point[0] = x - vx;
    point[1] = y - vz;
    point[2] = height - vy;
    angle = C_PointToAngle(point);

    if(!C_IsAngleVisible(angle))
        return false;

    // The point was not clipped by the clipnodes. Perhaps it's occluded
    // by an orange.
    return !C_IsPointOccluded(point);
}

/**
 * NOTE: Unused and untested. Almost certainly doesn't work correctly.
 */
boolean C_IsSegOccluded(float relv1[3], float relv2[3], float reltop,
                        float relbottom, binangle_t startAngle,
                        binangle_t endAngle)
{
    // The segment is always fully occluded from startAngle to occAngle.
    float       cross[3], testNormal[3];
    binangle_t  occAngle, crossAngle, trueStart, trueEnd;
    occnode_t  *orange;
    clipnode_t *ci;
    boolean     side1, side2, isSafe;

    // See if the given actual test range is safe. (startAngle and endAngle
    // always are.)
    trueStart = C_PointToAngle(relv2);
    trueEnd = C_PointToAngle(relv1);
    isSafe = (trueStart < trueEnd);

    // startAngle and endAngle are the real, safe range. It's first clipped
    // by any available clipnodes. We already know that no clipnode fully
    // contains the test range.
    for(ci = clipHead; ci; ci = ci->next)
    {
        // Does this clipnode partially overlap the test range?
        if(startAngle >= ci->start && startAngle <= ci->end)
        {
            // Start of the test range gets clipped.
            startAngle = ci->end;
        }
        if(endAngle >= ci->start && endAngle <= ci->end)
        {
            // End of the test range gets clipped.
            endAngle = ci->start;
        }
    }

    // In the beginning we have occluded nothing, i.e. up to the start angle.
    occAngle = startAngle;

    for(orange = occHead; orange; orange = orange->next)
    {
        if(occAngle >= endAngle)
            return true;        // Fully occluded.

        // This is the quickest way out of there: if we come across an
        // occlusion range that begins AFTER the occAngle, the portion
        // in between obviously won't be occluded by anybody (since oranges
        // are sorted by ascending start angles).
        if(orange->start > occAngle)
            return false;
        if(orange->end < occAngle)
            continue;           // Useless...

        /*      if(orange->end < startAngle) continue; // Doesn't overlap.
           if(orange->start > endAngle) break; // The rest are past the end. */

        // This orange overlaps the test range. Let's determine the test
        // plane we will be using.
        if(orange->flags & OCNF_TOPHALF)
        {
            // Tophalf is occluded, so test with the bottom of the seg.
            relv1[VZ] = relv2[VZ] = relbottom;
        }
        else
        {
            // Bottomhalf is occluded, so test with the top of the seg.
            relv1[VZ] = relv2[VZ] = reltop;
        }
        // Side=true means 'occluded'. Note that side2's angle is smaller
        // than side1's.
        side1 = M_DotProduct(relv1, orange->normal) > 0;
        side2 = M_DotProduct(relv2, orange->normal) > 0;
        if(side1 && side2)
        {
            // Does the orange fully contain the remaining portion of the seg?
            if(occAngle >= orange->start && endAngle <= orange->end)
                return true;    // Fully occluded by this orange!

            // Both the start and end vertex of the seg are occluded by
            // this orange, but the orange doesn't cover the whole seg.
            if(orange->end > occAngle)
                occAngle = orange->end;
            // Now we know that the seg has been occluded from the beginning
            // to occAngle.
            continue;           // Find more juicy oranges.
        }
        if(!side1 && !side2)
        {
            // This orange does not fully overlap the seg. Let's hope that
            // some other orange will.
            continue;
        }
        // The test plane crosses the occlusion plane somewhere inside the
        // seg (because the endpoints of the test seg were at different sides
        // of the occlusion plane). Calculate the normal of the test plane.
        M_CrossProduct(relv1, relv2, testNormal);

        // Calculate the angle of the cross line.
        M_CrossProduct(testNormal, orange->normal, cross);
        crossAngle = C_PointToAngle(cross);
        if(isSafe)
        {
            if(crossAngle < trueStart || crossAngle > trueEnd)
            {
                crossAngle += BANG_180; // Flip over.
                if(crossAngle < trueStart || crossAngle > trueEnd)
                    Con_Error("Cross line behaves strangely(1) "
                              "(st=%x end=%x crs=%x).\n", trueStart, trueEnd,
                              crossAngle);
            }
        }
        else
        {
            if(crossAngle < trueStart && crossAngle > trueEnd)
            {
                crossAngle += BANG_180;
                if(crossAngle < trueStart && crossAngle > trueEnd)
                    Con_Error("Cross line behaves strangely(2) "
                              "(st=%x end=%x crs=%x).\n", trueStart, trueEnd,
                              crossAngle);
            }
        }

        // Remember, side2 has a smaller angle.

        // \fixme What about trueStart/trueEnd!!! and isSafe! it must have
        // an effect on this...

        if(side2)
        {
            // We have an occlusion up to the cross point.
            if(crossAngle > occAngle)
                occAngle = crossAngle;
        }
        else                    // if(side1)
        {
            // We have an occlusion starting from crossAngle.
            if(crossAngle <= occAngle)
                // The occlusion begins before or at the currently
                // occluded portion.
                if(orange->end > occAngle)
                    occAngle = orange->end;
        }
    }
    // If the occlusion happened up to the end angle, we're OK.
    return (occAngle >= endAngle);
}

/**
 * Returns true if the segment is visible according to the current
 * clipnode and occlusion information.
 */
boolean C_CheckSeg(float *v1, float *v2, float top, float bottom)
{
    float       relv1[3], relv2[3];
    float       reltop = top - vy, relbottom = bottom - vy;
    binangle_t  start, end;

    relv1[0] = v1[VX] - vx;
    relv1[1] = v1[VY] - vz;
    relv1[2] = 0;

    relv2[0] = v2[VX] - vx;
    relv2[1] = v2[VY] - vz;
    relv2[2] = 0;

    // Determine the range.
    start = C_PointToAngle(relv2);
    end = C_PointToAngle(relv1);
    if(start == end)
        return true;            // Might as well be visible...
    if(!C_SafeCheckRange(start, end))
        return false;           // Entirely clipped.

    // Now the more difficult part... The range may be occluded by a number
    // of occlusion ranges, but we must determine whether these occlude the
    // segment fully, in 3D.
    if(start < end)
    {
        // The range doesn't wrap around.
        return !C_IsSegOccluded(relv1, relv2, reltop, relbottom, start, end);
    }
    // The range wraps around.
    return !C_IsSegOccluded(relv1, relv2, reltop, relbottom, start, BANG_MAX)
        || !C_IsSegOccluded(relv1, relv2, reltop, relbottom, 0, end);
}

/**
 * The specified range must be safe!
 */
static int C_IsRangeVisible(binangle_t startAngle, binangle_t endAngle)
{
    clipnode_t *ci;

    for(ci = clipHead; ci; ci = ci->next)
        if(startAngle >= ci->start && endAngle <= ci->end)
            return false;
    // No node fully contained the specified range.
    return true;
}

/**
 * Returns 1 if the range is not entirely clipped.
 */
static int C_SafeCheckRange(binangle_t startAngle, binangle_t endAngle)
{
    if(startAngle > endAngle)
    {
        // The range wraps around.
        return (C_IsRangeVisible(startAngle, BANG_MAX) ||
                C_IsRangeVisible(0, endAngle));
    }
    return C_IsRangeVisible(startAngle, endAngle);
}

int C_CheckViewRelSeg(float x1, float y1, float x2, float y2)
{
    float       dx1 = x1 - vx;
    float       dy1 = y1 - vz;
    float       dx2 = x2 - vx;
    float       dy2 = y2 - vz;

    if(devNoCulling)
        return 1;

    return C_SafeCheckRange(bamsAtan2((int) (dy2 * 1000), (int) (dx2 * 1000)) - BANG_45/90,
                            bamsAtan2((int) (dy1 * 1000), (int) (dx1 * 1000)) + BANG_45/90);
}

/**
 * Returns true if the specified angle is visible.
 */
int C_IsAngleVisible(binangle_t bang)
{
    clipnode_t *ci;

    for(ci = clipHead; ci; ci = ci->next)
        if(bang > ci->start && bang < ci->end)
            return false;
    // No one clipped this angle.
    return true;
}

clipnode_t *C_AngleClippedBy(binangle_t bang)
{
    clipnode_t *ci;

    for(ci = clipHead; ci; ci = ci->next)
        if(bang > ci->start && bang < ci->end)
            return ci;
    // No one clipped this angle.
    return 0;
}

/**
 * @return  Non-zero if the BspLeaf might be visible.
 */
int C_CheckBspLeaf(BspLeaf* bspLeaf)
{
    HEdge* hedge;
    uint i;

    if(!bspLeaf || bspLeaf->hedgeCount < 3) return 0;

    if(devNoCulling) return 1;

    // Do we need to resize the angle list buffer?
    if(bspLeaf->hedgeCount > anglistSize)
    {
        anglistSize *= 2;
        if(!anglistSize)
            anglistSize = 64;

        anglist =
         Z_Realloc(anglist, sizeof(binangle_t) * anglistSize, PU_APPSTATIC);
    }

    // Find angles to all corners.
    i = 0;
    hedge = bspLeaf->hedge;
    do
    {
        Vertex* vtx = hedge->HE_v1;
        // Shift for more accuracy.
        anglist[i++] = bamsAtan2((int) ((vtx->pos[VY] - vz) * 100),
                                 (int) ((vtx->pos[VX] - vx) * 100));

    } while((hedge = hedge->next) != bspLeaf->hedge);

    // Check each of the ranges defined by the edges.
    for(i = 0; i < bspLeaf->hedgeCount - 1; ++i)
    {
        uint end = i + 1;
        binangle_t angLen;

        // The last edge won't be checked. This is because the edges
        // define a closed, convex polygon and the last edge's range is
        // always already covered by the previous edges. (Right?)

        // If even one of the edges is not contained by a clipnode,
        // the leaf is at least partially visible.
        angLen = anglist[end] - anglist[i];

        // The viewer is on an edge, the leaf should be visible.
        if(angLen == BANG_180)
            return 1;

        // Choose the start and end points so that length is < 180.
        if(angLen < BANG_180)
        {
            if(C_SafeCheckRange(anglist[i], anglist[end]))
                return 1;
        }
        else
        {
            if(C_SafeCheckRange(anglist[end], anglist[i]))
                return 1;
        }
    }

    // All clipped away, the leaf cannot be seen.
    return 0;
}

/**
 * Returns true if clipnodes cover the whole range [0,360] degrees.
 */
boolean C_IsFull(void)
{
    if(devNoCulling)
        return false;

    return clipHead && clipHead->start == 0 && clipHead->end == BANG_MAX;
}
