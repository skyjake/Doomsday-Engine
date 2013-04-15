/** @file hplane.h BSP Builder half-plane and plane intersection list.
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

#ifndef LIBDENG_BSP_HPLANE
#define LIBDENG_BSP_HPLANE

#include "dd_types.h"
#include <de/vector1.h>
#include "partition.h"
#include <list>

namespace de {
namespace bsp {

/**
 * @ingroup bsp
 */
class HPlaneIntercept
{
public:
    HPlaneIntercept() : _distance(0), _userData(0)
    {}

    HPlaneIntercept(coord_t distance, void *userData)
        : _distance(distance), _userData(userData)
    {}

    /**
     * Determine the distance between two intercepts. It does not matter
     * if the intercepts are from different half-planes.
     */
    coord_t operator - (HPlaneIntercept const &other) const
    {
        return _distance - other.distance();
    }

    /**
     * Distance from the owning HPlane's origin point. Negative values
     * mean this intercept is positioned to the left of the origin.
     */
    coord_t distance() const { return _distance; }

    /**
     * Retrieve the data pointer associated with this intercept.
     */
    void *userData() const { return _userData; }

private:
    /**
     * Distance along the owning HPlane in the map coordinate space.
     */
    coord_t _distance;

    /// User data pointer associated with this intercept.
    void *_userData;
};

/**
 * @ingroup bsp
 */
class HPlane : private Partition
{
public:
    typedef std::list<HPlaneIntercept> Intercepts;

    typedef bool (*mergepredicate_t)(HPlaneIntercept &a, HPlaneIntercept &b, void *userData);

public:
    HPlane(Vector2d const &origin    = Vector2d(0, 0),
           Vector2d const &direction = Vector2d(0, 0));

    virtual ~HPlane();

    Vector2d const &origin() const;

    inline coord_t xOrigin() const { return origin().x; }
    inline coord_t yOrigin() const { return origin().y; }

    void setOrigin(Vector2d const &newOrigin);

    inline void setOrigin(coord_t newX, coord_t newY) {
        setOrigin(Vector2d(newX, newY));
    }

    void setXOrigin(coord_t newX);
    void setYOrigin(coord_t newY);

    Vector2d const &direction() const;

    inline coord_t xDirection() const { return direction().x; }
    inline coord_t yDirection() const { return direction().y; }

    void setDirection(Vector2d const &newDirection);

    inline void setDirection(coord_t newDX, coord_t newDY) {
        setDirection(Vector2d(newDX, newDY));
    }

    void setXDirection(coord_t newDX);
    void setYDirection(coord_t newDY);

    /**
     * Empty all intersections from the specified HPlane.
     */
    void clear();

    /**
     * Insert a point at the given intersection into the intersection list.
     *
     * @param distance  Distance along the partition for the new intercept,
     *                  in map units.
     * @param userData  User data object to link with the new intercept.
     *                  Ownership remains unchanged.
     */
    HPlaneIntercept &newIntercept(coord_t distance, void *userData = 0);

    void mergeIntercepts(mergepredicate_t predicate, void *userData);

    Intercepts const &intercepts() const;

#ifdef DENG_DEBUG
    static void DebugPrint(HPlane const &inst);
#endif

private:
    /// The intercept list. Kept sorted by distance, in ascending order.
    Intercepts _intercepts;
};

} // namespace bsp
} // namespace de

#endif // LIBDENG_BSP_HPLANE
