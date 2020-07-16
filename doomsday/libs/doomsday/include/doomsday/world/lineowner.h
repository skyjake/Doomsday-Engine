/** @file lineowner.h World map line owner.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include <de/legacy/binangle.h>
#include <de/vector.h>

namespace world {

class Line;

/**
 * @ingroup world
 *
 * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
 */
class LineOwner
{
public: /// @todo Make private:
    Line *_line;

    LineOwner *_link[2]; // by ClockDirection

    /// Angle between this and the next line owner, clockwise.
    binangle_t _angle;

    struct ShadowVert {
        de::Vec2d inner;
        de::Vec2d extended;
    } _shadowOffsets;

public:
    /*LineOwner() : _line(0), _angle(0)
    {
        _link[Previous] = 0;
        _link[Next] = 0;
    }*/

    /**
     * Returns @c true iff the previous line owner in the ring (CounterClockwise)
     * is not the same as this LineOwner.
     *
     * @see prev()
     */
    inline bool hasPrev() const { return prev() != this; }

    /**
     * Returns @c true iff the next line owner in the ring (clockwise) is not
     * the same as this LineOwner.
     *
     * @see next()
     */
    inline bool hasNext() const { return next() != this; }

    /**
     * Navigate to the adjacent line owner in the ring (if any). Note this may
     * be the same LineOwner.
     */
    LineOwner *navigate(de::ClockDirection dir = de::CounterClockwise) { return _link[dir]; }

    /// @copydoc navigate()
    const LineOwner *navigate(de::ClockDirection dir = de::CounterClockwise) const { return _link[dir]; }

    /**
     * Returns the previous line owner in the ring (CounterClockwise). Note that
     * this may be the same LineOwner.
     *
     * @see hasPrev()
     */
    inline LineOwner *prev() { return navigate(de::CounterClockwise); }

    /// @copydoc prev()
    inline const LineOwner *prev() const { return navigate(de::CounterClockwise); }

    /**
     * Returns the next line owner in the ring (clockwise). Note that this may
     * be the same LineOwner.
     *
     * @see hasNext()
     */
    inline LineOwner *next() { return navigate(de::Clockwise); }

    /// @copydoc next()
    inline const LineOwner *next() const { return navigate(de::Clockwise); }

//    inline LineOwner *prevPtr() { return _link[de::CounterClockwise]; }
//    inline LineOwner *nextPtr() { return _link[de::Clockwise]; }

    /**
     * Returns the line "owner".
     */
    Line &line() const { return *_line; }

    /**
     * Returns the angle between the line owner and the next in the ring (clockwise).
     */
    binangle_t angle() const { return _angle; }

    /**
     * Returns the inner shadow offset of the line owner.
     */
    const de::Vec2d &innerShadowOffset() const { return _shadowOffsets.inner; }

    /**
     * Returns the extended shadow offset of the line owner.
     */
    const de::Vec2d &extendedShadowOffset() const { return _shadowOffsets.extended; }
};

} // namespace world
