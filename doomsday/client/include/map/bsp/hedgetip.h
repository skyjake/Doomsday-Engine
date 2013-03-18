/** @file hedgetip.h BSP builder half-edge tip.
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

#ifndef LIBDENG_BSP_HEDGETIP
#define LIBDENG_BSP_HEDGETIP

#include "dd_types.h"
#include "map/p_mapdata.h"

namespace de {

class HEdge;

namespace bsp {

/**
 * A "hedgetip" is where a half-edge meets a vertex.
 *
 * @ingroup bsp
 */
class HEdgeTip
{
public:
    enum Side
    {
        Front = 0,
        Back
    };

public:
    explicit HEdgeTip(coord_t angle = 0, HEdge *front = 0, HEdge *back = 0)
        : _angle(angle), _front(front), _back(back)
    {}

    inline coord_t angle() const { return _angle; }

    inline HEdgeTip &setAngle(coord_t newAngle)
    {
        _angle = newAngle;
        return *this;
    }

    inline HEdge &front() const { return *_front; }

    inline HEdge &back() const { return *_back; }

    inline HEdge &side(Side sid) const
    {
        return sid == Front? front() : back();
    }

    inline bool hasFront() const { return _front != 0; }

    inline bool hasBack() const { return _back != 0; }

    inline bool hasSide(Side sid) const
    {
        return sid == Front? hasFront() : hasBack();
    }

    inline HEdgeTip &setFront(HEdge *hedge)
    {
        _front = hedge;
        return *this;
    }

    inline HEdgeTip &setBack(HEdge *hedge)
    {
        _back = hedge;
        return *this;
    }

    inline HEdgeTip &setSide(Side sid, HEdge *hedge)
    {
        return sid == Front? setFront(hedge) : setBack(hedge);
    }

private:
    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    coord_t _angle;

    /// Half-edge on each side of the tip. Front is the side of increasing
    /// angles, back is the side of decreasing angles. Either can be @c NULL
    HEdge *_front, *_back;
};

} // namespace bsp
} // namespace de

#endif // LIBDENG_BSP_HEDGETIP
