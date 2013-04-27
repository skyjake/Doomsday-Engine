/** @file lineowner.h World Map Line Owner.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_MAP_LINEOWNER
#define DENG_WORLD_MAP_LINEOWNER

#include <de/Vector>

#include <de/binangle.h>

class Line;

/**
 * @ingroup map
 *
 * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
 */
class LineOwner
{
public:
    /// Ring navigation direction identifiers:
    enum Direction
    {
        /// Previous (anticlockwise).
        Previous,

        /// Next (clockwise).
        Next
    };

public: /// @todo Make private:
    Line *_line;

    /// {Previous, Next} (i.e. {anticlk, clk}).
    LineOwner *_link[2];

    /// Angle between this and the next line owner, clockwise.
    binangle_t _angle;

    struct ShadowVert {
        de::Vector2d inner;
        de::Vector2d extended;
    } _shadowOffsets;

public:
    /*LineOwner() : _line(0), _angle(0)
    {
        _link[Previous] = 0;
        _link[Next] = 0;
    }*/

    /**
     * Returns @c true iff the previous line owner in the ring (anticlockwise)
     * is not the same as this LineOwner.
     *
     * @see prev()
     */
    inline bool hasPrev() const { return &prev() != this; }

    /**
     * Returns @c true iff the next line owner in the ring (clockwise) is not
     * the same as this LineOwner.
     *
     * @see next()
     */
    inline bool hasNext() const { return &next() != this; }

    /**
     * Navigate to the adjacent line owner in the ring (if any). Note this may
     * be the same LineOwner.
     */
    LineOwner &navigate(Direction dir = Previous) { return *_link[dir]; }

    /// @copydoc navigate()
    LineOwner const &navigate(Direction dir = Previous) const { return *_link[dir]; }

    /**
     * Returns the previous line owner in the ring (anticlockwise). Note that
     * this may be the same LineOwner.
     *
     * @see hasPrev()
     */
    inline LineOwner &prev() { return navigate(Previous); }

    /// @copydoc prev()
    inline LineOwner const &prev() const { return navigate(Previous); }

    /**
     * Returns the next line owner in the ring (clockwise). Note that this may
     * be the same LineOwner.
     *
     * @see hasNext()
     */
    inline LineOwner &next() { return navigate(Next); }

    /// @copydoc next()
    inline LineOwner const &next() const { return navigate(Next); }

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
    de::Vector2d const &innerShadowOffset() const { return _shadowOffsets.inner; }

    /**
     * Returns the extended shadow offset of the line owner.
     */
    de::Vector2d const &extendedShadowOffset() const { return _shadowOffsets.extended; }
};

#endif // DENG_WORLD_MAP_LINEOWNER
