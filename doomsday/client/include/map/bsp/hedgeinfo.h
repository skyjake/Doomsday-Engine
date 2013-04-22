/** @file hedgeinfo.h BSP Builder half-edge info.
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

#ifndef DENG_WORLD_MAP_BSP_HEDGEINFO
#define DENG_WORLD_MAP_BSP_HEDGEINFO

#include <de/mathutil.h>
#include <de/vector1.h>
#include "map/p_maptypes.h"

namespace de {
namespace bsp {

class SuperBlock;

/**
 * Plain old data (POD) structure storing additional information about a
 * half-edge produced by BspBuilder.
 *
 * @ingroup bsp
 */
struct HEdgeInfo
{
    coord_t start[2];
    coord_t end[2];
    coord_t direction[2];

    // Precomputed data for faster calculations.
    coord_t pLength;
    coord_t pAngle;
    coord_t pPara;
    coord_t pPerp;
    slopetype_t pSlopeType;

    HEdge *nextOnSide;
    HEdge *prevOnSide;

    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g. now in a leaf).
    SuperBlock *bmapBlock;

    /// Line this half-edge initially comes from else @c NULL if a "mini-edge".
    Line *line;

    // Line that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'line' field
    // above. For "miniedges", this is the line of the partition.
    Line *sourceLine;

    /// Map sector attributed to the half-edge. Can be @c 0 for "mini-edges".
    Sector *sector;

    HEdgeInfo()
        : pLength(0),
          pAngle(0),
          pPara(0),
          pPerp(0),
          pSlopeType(ST_VERTICAL),
          nextOnSide(0),
          prevOnSide(0),
          bmapBlock(0),
          line(0),
          sourceLine(0),
          sector(0)
    {
        V2d_Set(start, 0, 0);
        V2d_Set(end, 0, 0);
        V2d_Set(direction, 0, 0);
    }

    HEdgeInfo(HEdgeInfo const &other)
        : pLength(other.pLength),
          pAngle(other.pAngle),
          pPara(other.pPara),
          pPerp(other.pPerp),
          pSlopeType(other.pSlopeType),
          nextOnSide(other.nextOnSide),
          prevOnSide(other.prevOnSide),
          bmapBlock(other.bmapBlock),
          line(other.line),
          sourceLine(other.sourceLine),
          sector(other.sector)
    {
        V2d_Copy(start, other.start);
        V2d_Copy(end, other.end);
        V2d_Copy(direction, other.direction);
    }

    void initFromHEdge(HEdge const &hedge)
    {
        V2d_Set(start, hedge.fromOrigin().x, hedge.fromOrigin().y);
        V2d_Set(end,   hedge.toOrigin().x, hedge.toOrigin().y);
        V2d_Subtract(direction, end, start);

        pLength    = V2d_Length(direction);
        DENG2_ASSERT(pLength > 0);
        pAngle     = M_DirectionToAngle(direction);
        pSlopeType = M_SlopeType(direction);

        pPerp =  start[VY] * direction[VX] - start[VX] * direction[VY];
        pPara = -start[VX] * direction[VX] - start[VY] * direction[VY];
    }
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_HEDGEINFO
