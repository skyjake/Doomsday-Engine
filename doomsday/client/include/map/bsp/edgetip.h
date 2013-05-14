/** @file map/bsp/edgetip.h BSP Builder Edge Tip.
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

#ifndef DENG_WORLD_MAP_BSP_EDGETIP
#define DENG_WORLD_MAP_BSP_EDGETIP

#include <list>

#include "map/bsp/linesegment.h"

namespace de {
namespace bsp {

/**
 * A "edge tip" is where the edge of a line segment and the relevant
 * vertex meet.
 *
 * @ingroup bsp
 */
class EdgeTip
{
public:
    enum Side
    {
        Front = 0,
        Back
    };

public:
    explicit EdgeTip(coord_t angle = 0, LineSegment::Side *front = 0,
                            LineSegment::Side *back = 0)
        : _angle(angle), _front(front), _back(back)
    {}

    inline coord_t angle() const { return _angle; }

    inline EdgeTip &setAngle(coord_t newAngle)
    {
        _angle = newAngle;
        return *this;
    }

    inline LineSegment::Side &front() const { return *_front; }

    inline LineSegment::Side &back() const { return *_back; }

    inline LineSegment::Side &side(Side sid) const
    {
        return sid == Front? front() : back();
    }

    inline bool hasFront() const { return _front != 0; }

    inline bool hasBack() const { return _back != 0; }

    inline bool hasSide(Side sid) const
    {
        return sid == Front? hasFront() : hasBack();
    }

    inline EdgeTip &setFront(LineSegment::Side *lineSeg)
    {
        _front = lineSeg;
        return *this;
    }

    inline EdgeTip &setBack(LineSegment::Side *lineSeg)
    {
        _back = lineSeg;
        return *this;
    }

    inline EdgeTip &setSide(Side sid, LineSegment::Side *lineSeg)
    {
        return sid == Front? setFront(lineSeg) : setBack(lineSeg);
    }

private:
    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    coord_t _angle;

    /// Line segments on each side of the tip. Front is the side of increasing
    /// angles, back is the side of decreasing angles. Either can be @c 0.
    LineSegment::Side *_front, *_back;
};

class EdgeTips
{
public:
    typedef std::list<EdgeTip> All;

    EdgeTips() : _tips(0) {}

    bool isEmpty() const { return _tips.empty(); }

    /// Clear all tips in the set.
    void clear() { _tips.clear(); }

    /**
     * Add a new edge tip to the set in it's rightful place according to
     * an anti-clockwise (increasing angle) order.
     *
     * @param angleEpsilon  Smallest difference between two angles before being
     *                      considered equal (in degrees).
     */
    EdgeTip &add(coord_t angle, LineSegment::Side *front = 0, LineSegment::Side *back = 0,
                 coord_t angleEpsilon = 1.0 / 1024.0)
    {
        All::reverse_iterator after;

        for(after = _tips.rbegin();
            after != _tips.rend() && angle + angleEpsilon < (*after).angle(); after++)
        {}

        return *_tips.insert(after.base(), EdgeTip(angle, front, back));
    }

    All const &all() const { return _tips; }

private:
    All _tips;
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_EDGETIP
