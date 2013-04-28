/**
 * @file vertexinfo.h
 * BSP Builder Vertex info. @ingroup bsp
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
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

#ifndef LIBDENG_BSP_VERTEXINFO
#define LIBDENG_BSP_VERTEXINFO

#include "map/bsp/hedgetip.h"

namespace de {
namespace bsp {

/**
 * Data type used to record additional information and precalculated
 * values for a Vertex in the current map.
 */
class VertexInfo
{
public:
    VertexInfo() : oneSidedOwnerCount(0), twoSidedOwnerCount(0) {}

    // Total number of one and two-sided line owners.
    uint oneSidedOwnerCount;
    uint twoSidedOwnerCount;

    LineSegmentTips &edgeTips() { return tips; }

private:
    LineSegmentTips tips;
};

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSP_VERTEXINFO
