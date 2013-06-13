/** @file world/bsp/hplane.h World BSP Half-plane.
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

#ifndef DENG_WORLD_BSP_HPLANE_H
#define DENG_WORLD_BSP_HPLANE_H

#include <QList>

#include <de/mathutil.h> // M_InverseAngle

#include <de/Vector>

#include "world/bsp/linesegment.h"
#include "partition.h"

/// Two intercepts whose distance is inclusive of this bound will be merged.
#define HPLANE_INTERCEPT_MERGE_DISTANCE_EPSILON     1.0 / 128

class Sector;
class Vertex;

namespace de {
namespace bsp {

class EdgeTips;

/**
 * Models the partitioning binary space half-plane.
 *
 * @ingroup bsp
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
        // True if this intersection was on a self-referencing line.
        bool selfRef;

        /// Sector on each side of the vertex (along the partition), or @c 0
        /// if that direction is "closed" (i.e., the intercept point is along
        /// a map line that has no Sector on the relevant side).
        Sector *before;
        Sector *after;
        bool meetAtVertex;

    public:
        Intercept(double distance, LineSegment::Side &lineSeg, int edge,
                  bool meetAtVertex);

        bool operator < (Intercept const &other) const {
            return _distance < other._distance;
        }

        /**
         * Determine the distance between "this" and the @a other intercept.
         */
        double operator - (Intercept const &other) const {
            return _distance - other._distance;
        }

        /**
         * Returns distance along the half-plane relative to the origin.
         */
        double distance() const { return _distance; }

        /**
         * Returns the intercepted line segment.
         */
        LineSegment::Side &lineSegment() const;

        /**
         * Returns the identifier for the relevant edge of the intercepted
         * line segment.
         */
        int lineSegmentEdge() const;

        /**
         * Returns the relative vertex from the intercepted line segment.
         *
         * @see lineSegment(), lineSegmentEdge()
         */
        inline Vertex &vertex() const {
            return lineSegment().vertex(lineSegmentEdge());
        }

#ifdef DENG_DEBUG
        void debugPrint() const;
#endif

    private:
        /// Distance along the half-plane relative to the origin.
        double _distance;

        // The intercepted line segment and edge identifier.
        LineSegment::Side *_lineSeg;
        int _edge;
    };

    typedef QList<Intercept> Intercepts;

public:
    /**
     * Construct a new half-plane from the given @a partition line.
     */
    explicit HPlane(Partition const &partition = Partition());

    /**
     * Reconfigure the half-plane according to the given line segment.
     *
     * @param newLineSeg  The "new" line segment to configure using.
     */
    void configure(LineSegment::Side const &newLineSeg);

    /**
     * Perform intersection of the half-plane with the specified @a lineSeg
     * to determine the distance (along the partition line) at which the
     * @a edge vertex can be found.
     *
     * @param lineSeg   Line segment to perform intersection with.
     * @param edge      Line segment edge identifier of the vertex to use
     *                  for intersection.
     *
     * @return  Distance to intersection point along the half-plane (relative
     *          to the origin).
     */
    double intersect(LineSegment::Side const &lineSeg, int edge);

    /**
     * Perform intersection of the half-plane with the specified @a lineSeg.
     * If the two are found to intersect -- a new intercept will be added to
     * the list of intercepts. If a previous intersection for the specified
     * @a lineSeg @a edge has already been found then no new intercept will
     * be created and @c 0 is returned.
     *
     * @param lineSeg   Line segment to perform intersection with.
     * @param edge      Line segment edge identifier of the vertex to associate
     *                  with any resulting intercept.
     *
     * @param meetAtVertex  @c true intercept point is close enough to the
     *                      @a edge vertex to be considered incident.
     *
     * @param edgeTips  Set of EdgeTips for the identified @a edge of
     *                  @a lineSeg. (@todo Refactor away -ds)
     *
     * @return  The resultant new intercept; otherwise @a 0.
     */
    Intercept *intercept(LineSegment::Side const &lineSeg, int edge,
                         bool meetAtVertex, EdgeTips const &edgeTips);

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
     * Returns the world angle of the partition line (which, is derived from the
     * direction vector).
     *
     * @see inverseAngle(), Partition::direction
     */
    coord_t angle() const;

    /**
     * Returns the inverted world angle for the partition line (i.e., rotated 180 degrees).
     *
     * @see angle()
     */
    inline coord_t inverseAngle() const { return M_InverseAngle(angle()); }

    /**
     * Returns the logical @em slopetype for the partition line (which, is determined
     * according to the world direction).
     *
     * @see direction()
     * @see M_SlopeType()
     */
    slopetype_t slopeType() const;

    /**
     * Returns a pointer to the map Line attributed to the line segment which was
     * chosen as the half-plane partition. May return @c 0 (if no map line was
     * attributed).
     */
    LineSegment::Side *lineSegment() const;

    /**
     * Calculate @em perpendicular distances from one or both of the vertexe(s)
     * of "this" line segment to the @a other line segment. For this operation
     * the @a other line segment is interpreted as an infinite line. The vertexe(s)
     * of "this" line segment are projected onto the conceptually infinite line
     * defined by @a other and the length of the resultant vector(s) are then
     * determined.
     *
     * @param other     Other line segment to determine vertex distances to.
     *
     * Return values:
     * @param fromDist  Perpendicular distance from the "from" vertex. Can be @c 0.
     * @param toDist    Perpendicular distance from the "to" vertex. Can be @c 0.
     */
    void distance(LineSegment::Side const &lineSegment, coord_t *fromDist = 0,
                  coord_t *toDist = 0) const;

    /**
     * Determine the logical relationship between the partition line and the given
     * @a lineSegment. In doing so the perpendicular distance for the vertexes of
     * the line segment are calculated (and optionally returned).
     *
     * @param lineSegment  Line segment to determine relationship to.
     *
     * Return values:
     * @param fromDist  Perpendicular distance from the "from" vertex. Can be @c 0.
     * @param toDist    Perpendicular distance from the "to" vertex. Can be @c 0.
     *
     * @return LineRelationship between the partition line and the line segment.
     */
    LineRelationship relationship(LineSegment::Side const &lineSegment,
                                  coord_t *retFromDist, coord_t *retToDist) const;

    /**
     * Returns the list of intercepts for the half-plane for efficient traversal.
     *
     * @note This list may or may not yet be sorted. If a sorted list is desired
     * then sortAndMergeIntercepts() should first be called.
     *
     * @see interceptLineSegmentSide()
     */
    Intercepts const &intercepts() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_HPLANE_H
