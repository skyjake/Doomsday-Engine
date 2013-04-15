/** @file hedgeintercept.h BSP Builder half-edge intercept info.
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

#ifndef LIBDENG_BSP_HEDGEINTERCEPT
#define LIBDENG_BSP_HEDGEINTERCEPT

#include "dd_types.h"
#include "map/p_mapdata.h"

#include <de/Log>

namespace de {
namespace bsp {

/**
 * Plain-old-data structure containing additional information for a half-edge
 * half-plane intercept point where the geometry intersects (an incident vertex
 * can be found here (or at there will be upon insertion.)).
 *
 * There is always a corresponding HPlaneIntercept in the owning HPlane.
 *
 * @ingroup bsp
 */
struct HEdgeIntercept
{
    // Vertex in question.
    Vertex *vertex;

    // True if this intersection was on a self-referencing line.
    bool selfRef;

    // Sector on each side of the vertex (along the partition),
    // or NULL when that direction isn't OPEN.
    Sector *before;
    Sector *after;

    DENG_DEBUG_ONLY(
    static void DebugPrint(HEdgeIntercept const &inst)
    {
        LOG_INFO("Vertex #%i [x:%f, y:%f] beforeSector: #%d afterSector: #%d %s")
            << (inst.vertex->origIndex() - 1)
            << inst.vertex->origin()[VX]
            << inst.vertex->origin()[VY]
            << (inst.before? inst.before->origIndex() : -1)
            << (inst.after? inst.after->origIndex() : -1)
            << (inst.selfRef? "SELFREF" : "");
    })
};

} // namespace bsp
} // namespace de

#endif // LIBDENG_BSP_HEDGEINTERCEPT
