/**
 * @file hedgetip.h
 * BSP builder half-edge tip. @ingroup bsp
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

#ifndef LIBDENG_BSP_HEDGETIP
#define LIBDENG_BSP_HEDGETIP

#include "dd_types.h"
#include "p_mapdata.h"

namespace de {
namespace bsp {

#define ET_prev             link[0]
#define ET_next             link[1]
#define ET_front            hedges[0]
#define ET_back             hedges[1]

// A "hedgetip" is where a half-edge meets a vertex.
struct HEdgeTip
{
    // Link in list. List is kept in ANTI-clockwise order.
    HEdgeTip* link[2]; // {prev, next};

    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    coord_t angle;

    // Half-edge on each side of the edge. Left is the side of increasing
    // angles, right is the side of decreasing angles. Either can be NULL
    // for one sided edges.
    HEdge* hedges[2];

    HEdgeTip() : angle()
    {
        link[0] = 0;
        link[1] = 0;
        hedges[0] = 0;
        hedges[1] = 0;
    }
};

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSP_HEDGETIP
