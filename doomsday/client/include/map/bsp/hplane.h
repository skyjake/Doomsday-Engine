/** @file map/bsp/hplane.h BSP Builder Half-plane.
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

#ifndef DENG_WORLD_MAP_BSP_HPLANE
#define DENG_WORLD_MAP_BSP_HPLANE

#include <QList>

#include <de/Vector>

#include "partition.h"
#include "Sector"
#include "Vertex"

/// Two intercepts whose distance is inclusive of this bound will be merged.
#define HPLANE_INTERCEPT_MERGE_DISTANCE_EPSILON     1.0 / 128

namespace de {
namespace bsp {

class LineSegment;

/**
 * Models the partitioning binary space half-plane.
 */
class HPlane
{
public:
    /**
     * Used to model an intercept in the list of intersections.
     */
    class Intercept
    {
    public: /// @todo make private:
        // Vertex in question.
        Vertex *vertex;

        // True if this intersection was on a self-referencing line.
        bool selfRef;

        /// Sector on each side of the vertex (along the partition), or @c 0
        /// if that direction is "closed" (i.e., the intercept point is along
        /// a map line that has no Sector on the relevant side).
        Sector *before;
        Sector *after;

    public:
        Intercept(ddouble distance)
            : vertex(0),
              selfRef(false),
              before(0),
              after(0),
              _distance(distance)
        {}

        bool operator < (Intercept const &other) const {
            return _distance < other._distance;
        }

        /**
         * Determine the distance between "this" and the @a other intercept.
         */
        ddouble operator - (Intercept const &other) const {
            return _distance - other._distance;
        }

        /**
         * Returns distance along the half-plane relative to the origin.
         */
        ddouble distance() const { return _distance; }

        void merge(Intercept const &other)
        {
            /*
            LOG_AS("LineSegmentIntercept::merge");
            debugPrint();
            other.debugPrint();
            */

            if(selfRef && !other.selfRef)
            {
                if(before && other.before)
                    before = other.before;

                if(after && other.after)
                    after = other.after;

                selfRef = false;
            }

            if(!before && other.before)
                before = other.before;

            if(!after && other.after)
                after = other.after;

            /*
            LOG_TRACE("Result:");
            debugPrint();
            */
        }

#ifdef DENG_DEBUG
        void debugPrint() const
        {
            LOG_INFO("Vertex #%i %s beforeSector: #%d afterSector: #%d %s")
                << vertex->indexInMap()
                << vertex->origin().asText()
                << (before? before->indexInMap() : -1)
                << (after? after->indexInMap() : -1)
                << (selfRef? "SELFREF" : "");
        }
#endif

    private:
        /// Distance along the half-plane relative to the origin.
        ddouble _distance;
    };

    typedef QList<Intercept> Intercepts;

public:
    /**
     * Construct a new half-plane with the given origin and direction.
     *
     * @param partitionOrigin     Origin of the partition line.
     * @param partitionDirection  Direction of the partition line.
     */
    HPlane(Vector2d const &partitionOrigin    = Vector2d(0, 0),
           Vector2d const &partitionDirection = Vector2d(0, 0));

    /**
     * Reconfigure the half-plane according to the given line segment.
     *
     * @param newLineSeg  The "new" line segment to configure using.
     */
    void configure(LineSegment const &newLineSeg);

    /**
     * Perform intersection of the half-plane with the specified @a lineSeg.
     * If the two are found to intersect -- a new intercept will be added to
     * the list of intercepts. If a previous intersection for the specified
     * @a lineSeg @a edge has already been found then no new intercept will
     * be created and @c 0 is returned.
     *
     * @param lineSeg  Line segment to perform intersection with.
     * @param edge     Line segment edge identifier of the vertex to associate
     *                 with any resulting intercept.
     *
     * @return  The resultant new intercept; otherwise @a 0.
     */
    Intercept *interceptLineSegment(LineSegment const &lineSeg, int edge);

    /**
     * Sort and then merge near-intercepts from the given list.
     *
     * @todo fixme: Logically this is very suspect. Implementing this logic by
     * merging near-intercepts at hplane level is wrong because this does
     * nothing about any intercepting half-edge vertices. Consequently, rather
     * than moving the existing vertices and welding them, this will result in
     * the creation of new gaps gaps along the partition and result in holes
     * (which buildHEdgesAtIntersectionGaps() will then warn about).
     *
     * This should be redesigned so that near-intercepting vertices are welded
     * in a stable manner (i.e., not incrementally, which can result in vertices
     * drifting away from the hplane). Logically, therefore, this should not
     * be done prior to creating hedges along the partition - instead this
     * should happen afterwards. -ds
     *
     * @param intercepts  The list of intercepts to be sorted (in place).
     */
    void sortAndMergeIntercepts();

    /**
     * Clear the list of intercept "points" for the half-plane.
     */
    void clearIntercepts();

#ifdef DENG_DEBUG
    void printIntercepts() const;
#endif

    /**
     * Returns the Partition (immutable) used to model the partitioning line of
     * the half-plane.
     *
     * @see configure()
     */
    Partition const &partition() const;

    /**
     * Returns a copy of the LineSegment (immutable) from which the half-plane
     * was originally configured.
     *
     * @see configure()
     */
    LineSegment const &lineSegment() const;

    /**
     * Returns the list of intercepts for the half-plane for efficient traversal.
     *
     * @note This list may or may not yet be sorted. If a sorted list is desired
     * then sortAndMergeIntercepts() should first be called.
     *
     * @see interceptLineSegment(), intercepts()
     */
    Intercepts const &intercepts() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_HPLANE
