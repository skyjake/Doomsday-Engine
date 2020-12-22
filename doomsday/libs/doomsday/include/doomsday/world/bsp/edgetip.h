/** @file edgetip.h  World BSP Edge Tip.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_WORLD_BSP_EDGETIP_H
#define DE_WORLD_BSP_EDGETIP_H

#include "linesegment.h"
#include <list>

namespace world {
namespace bsp {

/**
 * A "edge tip" is where the edge of a line segment and the relevant vertex meet.
 *
 * @ingroup bsp
 */
class EdgeTip
{
public:
    // Logical side identifiers.
    enum Side { Front, Back };

public:
    EdgeTip() {}
    EdgeTip(LineSegmentSide &side)
        : _angle(side.angle())
        , _front(side.hasSector()? &side : nullptr)
        , _back(side.back().hasSector()? &side.back() : nullptr)
    {}

    double angle() const { return _angle; }

    void setAngle(coord_t newAngle) {
        _angle = newAngle;
    }

    bool hasSide(Side sid) const {
        return sid == Front? _front != nullptr : _back != nullptr;
    }

    inline bool hasFront() const { return hasSide(Front); }
    inline bool hasBack() const  { return hasSide(Back ); }

    LineSegmentSide &side(Side sid) const {
        if(sid == Front)
        {
            DE_ASSERT(_front != nullptr);
            return *_front;
        }
        else
        {
            DE_ASSERT(_back != nullptr);
            return *_back;
        }
    }

    inline LineSegmentSide &front() const { return side(Front); }
    inline LineSegmentSide &back () const { return side(Back ); }

    inline LineSegmentSide *frontPtr() const { return hasFront() ? &front() : nullptr; }
    inline LineSegmentSide *backPtr () const { return hasBack()  ? &back () : nullptr; }

    void setSide(Side sid, LineSegmentSide *lineSeg) {
        if(sid == Front)
        {
            _front = lineSeg;
        }
        else
        {
            _back = lineSeg;
        }
    }

    inline void setFront(LineSegmentSide *lineSeg) { setSide(Front, lineSeg); }
    inline void setBack (LineSegmentSide *lineSeg) { setSide(Back , lineSeg); }

private:
    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    double _angle = 0;

    /// Line segments on each side of the tip. Front is the side of increasing
    /// angles, back is the side of decreasing angles. Either may be @c nullptr.
    LineSegmentSide *_front = nullptr;
    LineSegmentSide *_back  = nullptr;
};

/**
 * Provides an always-sorted EdgeTip data set.
 *
 * @ingroup bsp
 */
class EdgeTips
{
public:
    ~EdgeTips() { clear(); }

    /// @see insert()
    inline EdgeTips &operator << (const EdgeTip &tip) {
        insert(tip);
        return *this;
    }

    /**
     * Returns @c true iff the set contains zero edge tips.
     */
    bool isEmpty() const { return _tips.empty(); }

    /**
     * Insert a copy of @a tip into the set, in it's rightful place according to
     * an anti-clockwise (increasing angle) order. @a epsilon is the angle
     * equivalence threshold (in degrees).
     */
    void insert(const EdgeTip &tip, double epsilon = 1.0 / 128) {
        Tips::reverse_iterator after = _tips.rbegin();
        while(after != _tips.rend() && tip.angle() + epsilon < (*after).angle())
        {
            after++;
        }
        _tips.insert(after.base(), tip);
    }

    /**
     * Returns the tip from the set with the smallest angle if not empty.
     */
    const EdgeTip *smallest() const {
        return _tips.empty()? nullptr : &_tips.front();
    }

    /**
     * Returns the tip from the set with the largest angle if not empty.
     */
    const EdgeTip *largest() const {
        return _tips.empty()? nullptr : &_tips.back();
    }

    /**
     * @a epsilon is the angle equivalence threshold (in degrees).
     */
    const EdgeTip *at(double angle, double epsilon = 1.0 / 128) const {
        for(const EdgeTip &tip : _tips)
        {
            double delta = de::abs(tip.angle() - angle);
            if(delta < epsilon || delta > (360.0 - epsilon))
            {
                return &tip;
            }
        }
        return nullptr;
    }

    /**
     * @a epsilon is the angle equivalence threshold (in degrees).
     */
    const EdgeTip *after(double angle, double epsilon = 1.0 / 128) const {
        for(const EdgeTip &tip : _tips)
        {
            if(angle + epsilon < tip.angle())
            {
                return &tip;
            }
        }
        return nullptr;
    }

    /**
     * Clear all tips in the set.
     */
    void clear() { _tips.clear(); }

    /**
     * Clear all tips attributed to the specified line segment @a seg.
     */
    void clearByLineSegment(LineSegment &seg)
    {
        Tips::iterator it = _tips.begin();
        while(it != _tips.end())
        {
            EdgeTip &tip = *it;
            if((tip.hasFront() && &tip.front().line() == &seg) ||
               (tip.hasBack()  && &tip.back().line()  == &seg))
            {
                it = _tips.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

private:
    typedef std::list<EdgeTip> Tips;
    Tips _tips;
};

}  // namespace bsp
}  // namespace world

#endif  // DE_WORLD_BSP_EDGETIP_H
