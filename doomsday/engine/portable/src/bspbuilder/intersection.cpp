/**
 * @file bsp_intersection.c
 * BSP Builder Intersections. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include "de_base.h"
#include "de_console.h"
#include "de_bsp.h"

#include "m_misc.h"
#include "edit_map.h"

#include "bspbuilder/intersection.hh"
#include "bspbuilder/superblockmap.hh"
#include "bspbuilder/bspbuilder.hh"

using namespace de;

struct hplaneintercept_s {
    struct hplaneintercept_s* next;
    struct hplaneintercept_s* prev;

    // How far along the partition line the vertex is. Zero is at the
    // partition half-edge's start point, positive values move in the same
    // direction as the partition's direction, and negative values move
    // in the opposite direction.
    double distance;

    void* userData;
};

HPlaneIntercept* HPlaneIntercept_Next(HPlaneIntercept* bi)
{
    assert(bi);
    return bi->next;
}

HPlaneIntercept* HPlaneIntercept_Prev(HPlaneIntercept* bi)
{
    assert(bi);
    return bi->prev;
}

void* HPlaneIntercept_UserData(HPlaneIntercept* bi)
{
    assert(bi);
    return bi->userData;
}

HPlaneIntercept* HPlaneIntercept_New(void)
{
    HPlaneIntercept* node;

    if(initedOK && usedIntercepts)
    {
        node = usedIntercepts;
        usedIntercepts = usedIntercepts->next;
    }
    else
    {
        // Need to allocate another.
        node = (HPlaneIntercept*)M_Malloc(sizeof *node);
    }

    node->userData = NULL;
    node->next = node->prev = NULL;

    return node;
}

void HPlane::clear()
{
    HPlaneIntercept* node = headPtr;
    while(node)
    {
        HPlaneIntercept* p = node->next;

        builder->deleteHEdgeIntercept((HEdgeIntercept*)node->userData);

        // Move the bi node to the unused node bi.
        node->next = usedIntercepts;
        usedIntercepts = node;

        node = p;
    }
    headPtr = NULL;
}

HPlane* HPlane::setOrigin(double const newOrigin[2])
{
    if(newOrigin)
    {
        partition.origin[0] = newOrigin[0];
        partition.origin[1] = newOrigin[1];
        clear();
    }
    return this;
}

HPlane* HPlane::setXY(double newX, double newY)
{
    double newOrigin[2];
    newOrigin[0] = newX;
    newOrigin[1] = newY;
    return setOrigin(newOrigin);
}

HPlane* HPlane::setX(double newX)
{
    partition.origin[0] = newX;
    clear();
    return this;
}

HPlane* HPlane::setY(double newY)
{
    partition.origin[1] = newY;
    clear();
    return this;
}

HPlane* HPlane::setAngle(double const newAngle[2])
{
    if(newAngle)
    {
        partition.angle[0] = newAngle[0];
        partition.angle[1] = newAngle[1];
        clear();
    }
    return this;
}

HPlane* HPlane::setDXY(double newDX, double newDY)
{
    double newAngle[2];
    newAngle[0] = newDX;
    newAngle[1] = newDY;
    return setAngle(newAngle);
}

HPlane* HPlane::setDX(double newDX)
{
    partition.angle[0] = newDX;
    clear();
    return this;
}

HPlane* HPlane::setDY(double newDY)
{
    partition.angle[1] = newDY;
    clear();
    return this;
}

int HPlane_IterateIntercepts2(de::HPlane* bi, int (*callback)(HPlaneIntercept*, void*), void* parameters)
{
    assert(bi);
    if(callback)
    {
        HPlaneIntercept* node;
        for(node = bi->headPtr; node; node = node->next)
        {
            int result = callback(node, parameters);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

int HPlane_IterateIntercepts(de::HPlane* bi, int (*callback)(HPlaneIntercept*, void*))
{
    return HPlane_IterateIntercepts2(bi, callback, NULL/*no parameters*/);
}

void Bsp_MergeHEdgeIntercepts(HEdgeIntercept* final, const HEdgeIntercept* other)
{
    if(!final || !other)
        Con_Error("Bsp_MergeIntersections2: Invalid arguments");

/*  DEBUG_Message((" Merging intersections:\n"));
#if _DEBUG
    Bsp_PrintHEdgeIntercept(final);
    Bsp_PrintHEdgeIntercept(other);
#endif*/

    if(final->selfRef && !other->selfRef)
    {
        if(final->before && other->before)
            final->before = other->before;

        if(final->after && other->after)
            final->after = other->after;

        final->selfRef = false;
    }

    if(!final->before && other->before)
        final->before = other->before;

    if(!final->after && other->after)
        final->after = other->after;

/*  DEBUG_Message((" Result:\n"));
#if _DEBUG
    Bsp_PrintHEdgeIntercept(final);
#endif*/
}

void BspBuilder::mergeIntersections(HPlane* hPlane)
{
    HPlaneIntercept* node, *np;

    if(!hPlane) return;

    node = hPlane->headPtr;
    np = node->next;
    while(node && np)
    {
        HEdgeIntercept* cur = (HEdgeIntercept*)node->userData;
        HEdgeIntercept* next = (HEdgeIntercept*)np->userData;
        double len = np->distance - node->distance;

        if(len < -0.1)
        {
            Con_Error("BspBuilder_MergeIntersections: Invalid intercept order - %1.3f > %1.3f\n",
                      node->distance, np->distance);
        }
        else if(len > 0.2)
        {
            node = np;
            np = node->next;
            continue;
        }
        /*else if(len > DIST_EPSILON)
        {
            DEBUG_Message((" Skipping very short half-edge (len=%1.3f) near (%1.1f,%1.1f)\n",
                           len, cur->vertex->V_pos[VX], cur->vertex->V_pos[VY]));
        }*/

        // Unlink this intercept.
        node->next = np->next;

        // Merge info for the two intersections into one.
        Bsp_MergeHEdgeIntercepts(cur, next);

        // Destroy the orphaned info.
        deleteHEdgeIntercept(next);

        np = node->next;
    }
}

void BspBuilder::buildHEdgesAtIntersectionGaps(HPlane* hPlane, SuperBlock* rightList,
    SuperBlock* leftList)
{
    HPlaneIntercept* node;

    if(!hPlane) return;

    node = hPlane->headPtr;
    while(node && node->next)
    {
        HEdgeIntercept* cur = (HEdgeIntercept*)node->userData;
        HEdgeIntercept* next = (HEdgeIntercept*)(node->next? node->next->userData : NULL);

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

                    MPE_RegisterUnclosedSectorNear(cur->after, pos[VX], pos[VY]);
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

                    MPE_RegisterUnclosedSectorNear(next->before, pos[VX], pos[VY]);
                }
            }
            else
            {
                // This is definitetly open space.
                bsp_hedge_t* right, *left;

                // Do a sanity check on the sectors (just for good measure).
                if(cur->after != next->before)
                {
                    if(!cur->selfRef && !next->selfRef)
                    {
                        VERBOSE(
                        Con_Message("Sector mismatch: #%d (%1.1f,%1.1f) != #%d (%1.1f,%1.1f)\n",
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

                addHEdgesBetweenIntercepts(hPlane, cur, next, &right, &left);

                // Add the new half-edges to the appropriate lists.
                SuperBlock_HEdgePush(rightList, right);
                SuperBlock_HEdgePush(leftList, left);
            }
        }

        node = node->next;
    }
}

HPlaneIntercept* HPlane::newIntercept2(double distance, void* userData)
{
    HPlaneIntercept* after, *newNode;

    /**
     * Enqueue the new intercept into the bi.
     */
    after = headPtr;
    while(after && after->next)
        after = after->next;

    while(after && distance < after->distance)
        after = after->prev;

    newNode = HPlaneIntercept_New();
    newNode->distance = distance;
    newNode->userData = userData;

    // Link it in.
    newNode->next = (after? after->next : headPtr);
    newNode->prev = after;

    if(after)
    {
        if(after->next)
            after->next->prev = newNode;

        after->next = newNode;
    }
    else
    {
        if(headPtr)
            headPtr->prev = newNode;

        headPtr = newNode;
    }

    return newNode;
}

HPlaneIntercept* HPlane::newIntercept(double distance)
{
    return newIntercept2(distance, NULL/*no user data*/);
}

#if _DEBUG
void HPlane_Print(HPlane* bi)
{
    if(bi)
    {
        HPlaneIntercept* node;

        Con_Message("HPlane %p:\n", bi);
        node = bi->headPtr;
        while(node)
        {
            HEdgeIntercept* inter = (HEdgeIntercept*)node->userData;
            Con_Printf(" %i: >%1.2f ", node->distance);
            Bsp_PrintHEdgeIntercept(inter);
            node = node->next;
        }
    }
}
#endif

void BspBuilder::initHPlaneInterceptAllocator(void)
{
    if(!initedOK)
    {
        usedIntercepts = NULL;
        initedOK = true;
    }
}

void BspBuilder::shutdownHPlaneInterceptAllocator(void)
{
    if(usedIntercepts)
    {
        HPlaneIntercept* node;

        node = usedIntercepts;
        while(node)
        {
            HPlaneIntercept* np = node->next;

            M_Free(node);
            node = np;
        }

        usedIntercepts = NULL;
    }

    initedOK = false;
}
