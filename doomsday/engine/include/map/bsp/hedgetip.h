/**
 * @file hedgetip.h
 * BSP builder half-edge tip. @ingroup bsp
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

#ifndef LIBDENG_BSP_HEDGETIP
#define LIBDENG_BSP_HEDGETIP

#include "dd_types.h"
#include "map/p_mapdata.h"

namespace de {
namespace bsp {

/**
 * A "hedgetip" is where a half-edge meets a vertex.
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
    explicit HEdgeTip(coord_t angle = 0, HEdge* front = 0, HEdge* back = 0)
        : angle_(angle), front_(front), back_(back)
    {}

    inline coord_t angle() const { return angle_; }
    inline HEdgeTip& setAngle(coord_t newAngle) {
        angle_ = newAngle;
        return *this;
    }

    inline HEdge& front() const { return *front_; }
    inline HEdge& back() const { return *back_; }
    inline HEdge& side(Side sid) const {
        return sid == Front? front() : back();
    }

    inline bool hasFront() const { return !!front_; }
    inline bool hasBack() const { return !!back_; }
    inline bool hasSide(Side sid) const {
        return sid == Front? hasFront() : hasBack();
    }

    inline HEdgeTip& setFront(HEdge* hedge) {
        front_ = hedge;
        return *this;
    }

    inline HEdgeTip& setBack(HEdge* hedge) {
        back_ = hedge;
        return *this;
    }

    inline HEdgeTip& setSide(Side sid, HEdge* hedge) {
        return sid == Front? setFront(hedge) : setBack(hedge);
    }

private:
    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    coord_t angle_;

    /// Half-edge on each side of the tip. Front is the side of increasing
    /// angles, back is the side of decreasing angles. Either can be @c NULL
    HEdge* front_, *back_;
};

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSP_HEDGETIP
