/**
 * @file hedgeinfo.h
 * BSP Builder half-edge info. @ingroup bsp
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

#ifndef LIBDENG_BSP_HEDGEINFO
#define LIBDENG_BSP_HEDGEINFO

#include "m_misc.h"
#include "p_mapdata.h"

namespace de {
namespace bsp {

class SuperBlock;

/**
 * Plain old data (POD) structure storing additional information about a
 * half-edge produced by BspBuilder.
 */
struct HEdgeInfo
{
    // Precomputed data for faster calculations.
    coord_t pSX, pSY;
    coord_t pEX, pEY;
    coord_t pDX, pDY;

    coord_t pLength;
    coord_t pAngle;
    coord_t pPara;
    coord_t pPerp;

    HEdge* nextOnSide;
    HEdge* prevOnSide;

    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g. now in a leaf).
    class SuperBlock* bmapBlock;

    // Linedef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    LineDef* sourceLineDef;

    HEdgeInfo()
        : pSX(0), pSY(0), pEX(0), pEY(0), pDX(0), pDY(0), pLength(0), pAngle(0), pPara(0), pPerp(0),
          nextOnSide(0), prevOnSide(0), bmapBlock(0), sourceLineDef(0)
    {}

    HEdgeInfo& initFromHEdge(const HEdge& hedge)
    {
        pSX = hedge.v[0]->buildData.pos[VX];
        pSY = hedge.v[0]->buildData.pos[VY];
        pEX = hedge.v[1]->buildData.pos[VX];
        pEY = hedge.v[1]->buildData.pos[VY];
        pDX = pEX - pSX;
        pDY = pEY - pSY;

        pLength = M_Length(pDX, pDY);
        pAngle  = M_SlopeToAngle(pDX, pDY);

        pPerp =  pSY * pDX - pSX * pDY;
        pPara = -pSX * pDX - pSY * pDY;

        Q_ASSERT(pLength > 0);
        return *this;
    }
};

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSP_HEDGEINFO
