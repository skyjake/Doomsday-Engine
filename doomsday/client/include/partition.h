/** @file partition.h Infinite line of the form point + direction vector.
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

#ifndef DENG_MATH_PARTITION
#define DENG_MATH_PARTITION

#include <de/Vector>

namespace de {

/**
 * An infinite line of the form point + direction vector. The members are public
 * for convenient access.
 *
 * @ingroup math
 */
class Partition
{
public:
    Vector2d origin;
    Vector2d direction;

public:
    Partition(Vector2d const &origin, Vector2d const &direction)
        : origin(origin), direction(direction) {}

    Partition(Partition const &other)
        : origin(other.origin), direction(other.direction) {}

    /**
     * Where does the given @a point lie relative to the partition line?
     *
     * @param point  The point to test.
     *
     * @return @c <0 Point is to the left of the line.
     *         @c =0 Point lies directly on/incident with the line.
     *         @c >0 Point is to the right of the line.
     */
    ddouble pointOnSide(Vector2d const &point) const {
        return (origin.y - point.y) * direction.x - (origin.x - point.x) * direction.y;
    }

    /**
     * @copydoc pointOnSide()
     *
     * @param x  X coordinate for the point.
     * @param y  Y coordinate for the point.
     */
    ddouble pointOnSide(ddouble x, ddouble y) const {
        return pointOnSide(Vector2d(x, y));
    }
};

} // namespace de

#endif // DENG_MATH_PARTITION
