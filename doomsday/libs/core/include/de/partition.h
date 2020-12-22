/** @file partition.h  Infinite line of the form point + direction vector.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_PARTITION_H
#define LIBCORE_PARTITION_H

#include "de/math.h"
#include "de/string.h"
#include "de/vector.h"

#include <sstream>

namespace de {

/**
 * An infinite line of the form point + direction vector. The members are public
 * for convenient access.
 *
 * @ingroup types
 */
class Partition
{
public:
    Vec2d direction;
    Vec2d origin;

public:
    Partition(const Vec2d &direction = Vec2d(),
              const Vec2d &origin    = Vec2d())
        : direction(direction), origin(origin) {}

    Partition(const Partition &other)
        : direction(other.direction), origin(other.origin) {}

    /**
     * Where does the given @a point lie relative to the partition line?
     *
     * @param point  The point to test.
     *
     * @return @c <0 Point is to the left of the line.
     *         @c =0 Point lies directly on/incident with the line.
     *         @c >0 Point is to the right of the line.
     */
    ddouble pointOnSide(const Vec2d &point) const {
        return (origin.y - point.y) * direction.x - (origin.x - point.x) * direction.y;
    }

    /**
     * Returns @c true iff "this" line and @a other are parallel. In the special
     * case of either line having a zero-length direction, @c true is returned.
     */
    bool isParallelTo(const Partition &other, ddouble epsilon = .99999999) const
    {
        ddouble len = direction.length();
        if (len == 0) return true;

        ddouble otherLen = other.direction.length();
        if (otherLen == 0) return true;

        ddouble dot = direction.dot(other.direction) / len / otherLen;

        // If it's close enough, we'll consider them parallel.
        epsilon = de::abs(epsilon);
        return dot > epsilon || dot < -epsilon;
    }

    /**
     * Determines how far along "this" line (relative to the origin) that the
     * @a other line and this intersect.
     *
     * @return  Intersection point expressed as a scale factor, relative to the
     * line origin. In the special case of the two lines being parallel @c 0 is
     * returned.
     *
     * @see intercept()
     */
    ddouble intersection(const Partition &other) const
    {
        ddouble divsor = direction.x * other.direction.y -
                         direction.y * other.direction.x;

        // Special case: parallel?
        if (divsor == 0) return 0;

        Vec2d delta = origin - other.origin;
        return (delta.y * other.direction.x - delta.x * other.direction.y) / divsor;
    }

    /**
     * Determine the intercept point where "this" line and @a other intersect
     * and return the Euclidean point at which the two intercept.
     */
    inline Vec2d intercept(const Partition &other) const {
        return origin + direction * intersection(other);
    }

    String asText() const
    {
        std::ostringstream s;
        s << direction.x << "/" << direction.y << " " << origin;
        return s.str();
    }
};

}  // namespace de

#endif  // LIBCORE_PARTITION_H
