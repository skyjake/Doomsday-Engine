/** @file map/bsp/linesegmentintercept.h BSP Builder half-edge intercept info.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_BSP_LINESEGMENTINTERCEPT
#define DENG_WORLD_MAP_BSP_LINESEGMENTINTERCEPT

#include <de/Log>

#include "Sector"
#include "Vertex"

namespace de {
namespace bsp {

/**
 * Plain-old-data structure containing additional information for a line segment
 * half-plane intersection (an incident vertex can be found here (or there will
 * be upon insertion.)).
 *
 * There is always a corresponding Intercept in the owning Intercepts.
 *
 * @ingroup bsp
 */
struct LineSegmentIntercept
{
    // Vertex in question.
    Vertex *vertex;

    // True if this intersection was on a self-referencing line.
    bool selfRef;

    /// Sector on each side of the vertex (along the partition), or @c 0 if that
    /// direction is "closed" (i.e., the intercept point is along a map line that
    /// has no Sector on the relevant side).
    Sector *before;
    Sector *after;

    LineSegmentIntercept()
        : vertex(0),
          selfRef(false),
          before(0),
          after(0)
    {}

    LineSegmentIntercept(LineSegmentIntercept const &other)
        : vertex(other.vertex),
          selfRef(other.selfRef),
          before(other.before),
          after(other.after)
    {}

    void merge(LineSegmentIntercept const &other)
    {
        /*
        LOG_AS("LineSegmentIntercept::merge");
        debugPrint();
        other.debugPrint();
        */

        if(selfRef && !other.selfRef)
        {
            if(before && other.before)
                before = other.before;

            if(after && other.after)
                after = other.after;

            selfRef = false;
        }

        if(!before && other.before)
            before = other.before;

        if(!after && other.after)
            after = other.after;

        /*
        LOG_TRACE("Result:");
        debugPrint();
        */
    }

#ifdef DENG_DEBUG
    void debugPrint() const
    {
        LOG_INFO("Vertex #%i %s beforeSector: #%d afterSector: #%d %s")
            << vertex->indexInMap()
            << vertex->origin().asText()
            << (before? before->indexInMap() : -1)
            << (after? after->indexInMap() : -1)
            << (selfRef? "SELFREF" : "");
    }
#endif
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_LINESEGMENTINTERCEPT
