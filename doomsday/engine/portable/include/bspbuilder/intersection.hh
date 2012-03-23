/**
 * @file bsp_intersection.h
 * BSP Builder Intersections. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_BSP_INTERSECTION
#define LIBDENG_MAP_BSP_INTERSECTION

#include "bspbuilder/hedges.hh"

#include <list>

namespace de {

class BspBuilder;

struct HPlanePartition {
    double origin[2];
    double angle[2];

    HPlanePartition(double x=0, double y=0, double dX=0, double dY=0)
    {
        origin[0] = x;
        origin[1] = y;
        angle[0] = dX;
        angle[1] = dY;
    }

    HPlanePartition(double const _origin[2], double const _angle[2])
    {
        origin[0] = _origin[0];
        origin[1] = _origin[1];
        angle[0] = _angle[0];
        angle[1] = _angle[1];
    }
};

struct HPlaneIntercept {
    // How far along the partition line the vertex is. Zero is at the
    // partition half-edge's start point, positive values move in the same
    // direction as the partition's direction, and negative values move
    // in the opposite direction.
    double distance;

    void* userData;

    HPlaneIntercept() :
        distance(0), userData(0)
    {}

    HPlaneIntercept(double distance, void* userData) :
        distance(distance), userData(userData)
    {}

    double operator - (const HPlaneIntercept& other) const {
        return distance - other.distance;
    }
};

class HPlane {
public:
    typedef std::list<HPlaneIntercept> Intercepts;

    HPlane(BspBuilder* builder) :
        builder(builder), intercepts(0), partition()
    {
        initHEdgeInfo();
    }

    HPlane(BspBuilder* builder, double const origin[2], double const angle[2]) :
        builder(builder), intercepts(0), partition(origin, angle)
    {
        initHEdgeInfo();
    }

    ~HPlane() { clear(); }

    const double* origin() { return partition.origin; }
    double x() { return partition.origin[0]; }
    double y() { return partition.origin[1]; }

    HPlane* setOrigin(double const origin[2]);
    HPlane* setXY(double x, double y);
    HPlane* setX(double x);
    HPlane* setY(double y);

    const double* angle() { return partition.angle; }
    double dX() { return partition.angle[0]; }
    double dY() { return partition.angle[1]; }

    HPlane* setAngle(double const angle[2]);
    HPlane* setDXY(double x, double y);
    HPlane* setDX(double dx);
    HPlane* setDY(double dy);

    /// @todo Does not belong here.
    BspHEdgeInfo* partitionHEdgeInfo() { return &hedgeInfo; }

    /**
     * Empty all intersections from the specified HPlane.
     */
    void clear();

    /**
     * Insert a point at the given intersection into the intersection list.
     *
     * @todo Ownership of the user data should NOT be passed to this object.
     *
     * @param userData  Ownership passes to HPlane.
     */
    HPlaneIntercept* newIntercept(double distance, void* userData=NULL);

    Intercepts::const_iterator deleteIntercept(Intercepts::const_iterator at);

    inline bool empty() const { return intercepts.empty(); }

    inline Intercepts::const_iterator begin() const { return intercepts.begin(); }

    inline Intercepts::const_iterator end() const { return intercepts.end(); }

    inline Intercepts::size_type size() const { return intercepts.size(); }

private:
    void initHEdgeInfo()
    {
        memset(&hedgeInfo, 0, sizeof(hedgeInfo));
    }

private:
    HPlanePartition partition;

    /// The intercept list. Kept sorted by along_dist, in ascending order.
    Intercepts intercepts;

    /// Additional information used by the node builder during construction.
    BspHEdgeInfo hedgeInfo;

    /// BspBuilder instance which produced this.
    /// @todo Remove me.
    BspBuilder* builder;
};

} // namespace de

#endif /// LIBDENG_MAP_BSP_INTERSECTION
