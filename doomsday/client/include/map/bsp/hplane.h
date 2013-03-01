/**
 * @file hplane.h
 * BSP Builder half-plane and plane intersection list. @ingroup bsp
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

#ifndef LIBDENG_BSP_HPLANE
#define LIBDENG_BSP_HPLANE

#include "dd_types.h"
#include <de/vector1.h>
#include <list>

namespace de {
namespace bsp {

struct HPlanePartition
{
    coord_t origin[2];
    coord_t direction[2];

    explicit HPlanePartition(coord_t x=0, coord_t y=0, coord_t dX=0, coord_t dY=0)
    {
        V2d_Set(origin, x, y);
        V2d_Set(direction, dX, dY);
    }

    HPlanePartition(coord_t const _origin[2], coord_t const _direction[2])
    {
        V2d_Copy(origin, _origin);
        V2d_Copy(direction, _direction);
    }

    HPlanePartition& setOrigin(coord_t x, coord_t y)
    {
        V2d_Set(origin, x, y);
        return *this;
    }

    HPlanePartition& setOrigin(coord_t const newOrigin[2])
    {
        return setOrigin(newOrigin[0], newOrigin[1]);
    }

    HPlanePartition& setDirection(coord_t dx, coord_t dy)
    {
        V2d_Set(direction, dx, dy);
        return *this;
    }

    HPlanePartition& setDirection(coord_t const newDirection[2])
    {
        return setDirection(newDirection[0], newDirection[1]);
    }
};

class HPlaneIntercept
{
public:
    /**
     * Distance from the owning HPlane's origin point. Negative values
     * mean this intercept is positioned to the left of the origin.
     */
    coord_t distance() const { return _distance; }

    /**
     * Retrieve the data pointer associated with this intercept.
     */
    void* userData() const { return _userData; }

    /**
     * Determine the distance between two intercepts. It does not matter
     * if the intercepts are from different half-planes.
     */
    coord_t operator - (const HPlaneIntercept& other) const
    {
        return _distance - other.distance();
    }

    HPlaneIntercept() : _distance(0), _userData(0) {}
    HPlaneIntercept(coord_t distance, void* userData) :
        _distance(distance), _userData(userData) {}

private:
    /**
     * Distance along the owning HPlane in the map coordinate space.
     */
    coord_t _distance;

    /// User data pointer associated with this intercept.
    void* _userData;
};

class HPlane
{
public:
    typedef std::list<HPlaneIntercept> Intercepts;

    typedef bool (*mergepredicate_t)(HPlaneIntercept& a, HPlaneIntercept& b, void* userData);

public:
    HPlane() : partition(), intercepts_(0){}
    HPlane(coord_t const origin[2], coord_t const direction[2]) :
        partition(origin, direction), intercepts_(0) {}

    ~HPlane() { clear(); }

    const coord_t* origin() { return partition.origin; }
    coord_t x() { return partition.origin[VX]; }
    coord_t y() { return partition.origin[VY]; }

    HPlane* setOrigin(coord_t const origin[2]);
    HPlane* setXY(coord_t x, coord_t y);
    HPlane* setX(coord_t x);
    HPlane* setY(coord_t y);

    const coord_t* direction() { return partition.direction; }
    coord_t dX() { return partition.direction[VX]; }
    coord_t dY() { return partition.direction[VY]; }

    HPlane* setDirection(coord_t const direction[2]);
    HPlane* setDXY(coord_t x, coord_t y);
    HPlane* setDX(coord_t dx);
    HPlane* setDY(coord_t dy);

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
    HPlaneIntercept& newIntercept(coord_t distance, void* userData = NULL);

    void mergeIntercepts(mergepredicate_t predicate, void* userData);

    const Intercepts& intercepts() const;

    static void DebugPrint(const HPlane& inst);

private:
    HPlanePartition partition;

    /// The intercept list. Kept sorted by distance, in ascending order.
    Intercepts intercepts_;
};

} // namespace bsp
} // namespace de

#endif // LIBDENG_BSP_HPLANE
