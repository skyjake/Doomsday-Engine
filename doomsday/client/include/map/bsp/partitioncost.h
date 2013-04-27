/** @file partitioncost.h BSP builder partition cost evaluation.
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

#ifndef LIBDENG_BSP_PARTITIONCOST
#define LIBDENG_BSP_PARTITIONCOST

#include "map/p_mapdata.h"
#include "HEdge"

namespace de {
namespace bsp {

/**
 * @ingroup bsp
 */
struct PartitionCost
{
    enum Side
    {
        Right = 0,
        Left
    };

    int total;
    int splits;
    int iffy;
    int nearMiss;
    int realRight;
    int realLeft;
    int miniRight;
    int miniLeft;

    PartitionCost() :
        total(0), splits(0), iffy(0), nearMiss(0), realRight(0),
        realLeft(0), miniRight(0), miniLeft(0)
    {}

    inline PartitionCost &addHEdgeRight(HEdge const &hedge)
    {
        if(hedge.hasLineSide()) realRight += 1;
        else                miniRight += 1;
        return *this;
    }

    inline PartitionCost &addHEdgeLeft(HEdge const &hedge)
    {
        if(hedge.hasLineSide()) realLeft += 1;
        else                miniLeft += 1;
        return *this;
    }

    PartitionCost &operator += (PartitionCost const &other)
    {
        total     += other.total;
        splits    += other.splits;
        iffy      += other.iffy;
        nearMiss  += other.nearMiss;
        realLeft  += other.realLeft;
        realRight += other.realRight;
        miniLeft  += other.miniLeft;
        miniRight += other.miniRight;
        return *this;
    }

    PartitionCost &operator = (PartitionCost const &other)
    {
        total     = other.total;
        splits    = other.splits;
        iffy      = other.iffy;
        nearMiss  = other.nearMiss;
        realLeft  = other.realLeft;
        realRight = other.realRight;
        miniLeft  = other.miniLeft;
        miniRight = other.miniRight;
        return *this;
    }

    bool operator < (PartitionCost const &rhs) const
    {
        return total < rhs.total;
    }
};

} // namespace bsp
} // namespace de

#endif // LIBDENG_BSP_PARTITIONCOST
