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

void HPlane::clear()
{
    for(Intercepts::iterator it = intercepts.begin(); it != intercepts.end(); ++it)
    {
        HPlaneIntercept* inter = &*it;
        builder->deleteHEdgeIntercept((HEdgeIntercept*)inter->userData);
    }
    intercepts.clear();
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

void BspBuilder::mergeIntersections(HPlane* hplane)
{
    if(!hplane) return;

    HPlane::Intercepts::const_iterator node = hplane->begin();
    while(node != hplane->end())
    {
        HPlane::Intercepts::const_iterator np = node; np++;
        if(np == hplane->end()) break;

        double len = *np - *node;
        if(len < -0.1)
        {
            Con_Error("BspBuilder_MergeIntersections: Invalid intercept order - %1.3f > %1.3f\n",
                      node->distance, np->distance);
        }
        else if(len > 0.2)
        {
            node++;
            continue;
        }

        HEdgeIntercept* cur  = (HEdgeIntercept*)node->userData;
        HEdgeIntercept* next = (HEdgeIntercept*)np->userData;

        /*if(len > DIST_EPSILON)
        {
            DEBUG_Message((" Skipping very short half-edge (len=%1.3f) near (%1.1f,%1.1f)\n",
                           len, cur->vertex->V_pos[VX], cur->vertex->V_pos[VY]));
        }*/

        // Merge info for the two intersections into one.
        Bsp_MergeHEdgeIntercepts(cur, next);

        // Destroy the orphaned info.
        deleteHEdgeIntercept(next);

        // Unlink this intercept.
        hplane->deleteIntercept(np);
    }
}

void BspBuilder::buildHEdgesAtIntersectionGaps(HPlane* hplane, SuperBlock* rightList,
    SuperBlock* leftList)
{
    HPlane::Intercepts::const_iterator node;

    if(!hplane) return;

    node = hplane->begin();
    while(node != hplane->end())
    {
        HPlane::Intercepts::const_iterator np = node; np++;
        if(np == hplane->end()) break;

        HEdgeIntercept* cur = (HEdgeIntercept*)((*node).userData);
        HEdgeIntercept* next = (HEdgeIntercept*)((*np).userData);

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

                addHEdgesBetweenIntercepts(hplane, cur, next, &right, &left);

                // Add the new half-edges to the appropriate lists.
                SuperBlock_HEdgePush(rightList, right);
                SuperBlock_HEdgePush(leftList, left);
            }
        }

        node++;
    }
}

HPlaneIntercept* HPlane::newIntercept(double distance, void* userData)
{
    Intercepts::const_reverse_iterator after;
    HPlaneIntercept* inter;

    for(after = intercepts.rbegin();
        after != intercepts.rend() && distance < (*after).distance; after++)
    {}

    inter = &*intercepts.insert(after.base(), HPlaneIntercept(distance, userData));
    return inter;
}

HPlane::Intercepts::const_iterator HPlane::deleteIntercept(Intercepts::const_iterator at)
{
    //if(at < intercepts.begin() || at >= intercepts.end()) return at;
    return intercepts.erase(at);
}

#if _DEBUG
void HPlane_Print(HPlane* hplane)
{
    if(!hplane) return;

    Con_Message("HPlane %p:\n", hplane);
    uint n = 0;
    for(HPlane::Intercepts::const_iterator it = hplane->begin(); it != hplane->end(); it++, n++)
    {
        const HPlaneIntercept* inter = &*it;
        Con_Printf(" %u: >%1.2f ", n, inter->distance);
        Bsp_PrintHEdgeIntercept((HEdgeIntercept*)inter->userData);
    }
}
#endif
