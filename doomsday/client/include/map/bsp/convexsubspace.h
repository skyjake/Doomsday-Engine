/** @file map/bsp/convexsubspace.h BSP Builder Convex Subspace.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_BSP_CONVEXSUBSPACE
#define DENG_WORLD_MAP_BSP_CONVEXSUBSPACE

#include <QList>

#include <de/Error>

#include "map/bsp/linesegment.h"

class BspLeaf;
class Sector;

namespace de {

class Mesh;

namespace bsp {

struct OrderedSegment
{
    LineSegment::Side *segment;
    double fromAngle;
    double toAngle;

    bool operator == (OrderedSegment const &other) const
    {
        return de::fequal(fromAngle, other.fromAngle) &&
               de::fequal(toAngle, other.toAngle);
    }

#ifdef DENG_DEBUG
    void debugPrint() const
    {
        LOG_INFO("[%p] Angle: %1.6f %s -> Angle: %1.6f %s")
            << de::dintptr(this)
            << fromAngle
            << (segment? segment->from().origin().asText() : "(null)")
            << toAngle
            << (segment? segment->to().origin().asText() : "(null)");
    }
#endif
};
typedef QList<OrderedSegment> OrderedSegments;

/**
 * Models a @em logical convex subspace in the partition plane, providing the
 * analysis functionality necessary to classify and then separate the segments
 * into unique geometries.
 *
 * Here infinity (i.e., a subspace containing no segments) is considered to be
 * convex. Accordingly any segments linked to the subspace are @em not "owned"
 * by it.
 *
 * @note Important: It is the user's responsibility to ensure convexity else
 * many of the operations on the set of segments will be illogical.
 *
 * @ingroup bsp
 */
class ConvexSubspace
{
public:
    /**
     * Construct an empty convex subspace.
     */
    ConvexSubspace();

    /**
     * Construct a convex subspace from a list of line @a segments.
     *
     * @param segments  List of line segments which are assumed to define a
     *                  convex subspace in the plane. Ownership of the segments
     *                  is @em NOT given to the subspace. Note that duplicates
     *                  are pruned automatically.
     */
    ConvexSubspace(QList<LineSegment::Side *> const &segments);

    /**
     * Construct a convex subspace by duplicating @a other.
     */
    ConvexSubspace(ConvexSubspace const &other);

    /**
     * Returns the total number of segments in the subspace.
     */
    int segmentCount() const;

    /**
     * Returns @c true iff the subspace is "empty", which is to say there are
     * zero line segments in the set.
     *
     * Equivalent to @code segmentCount() == 0 @endcode
     */
    inline bool isEmpty() const { return segmentCount() == 0; }

    /**
     * Add more line segments to the subspace. It is assumed that the new set
     * conforms to, or is compatible with the subspace.
     *
     * @param segments  List of line segments to add to the subspace. Ownership
     *                  of the segments is @em NOT given to the subspace. Note
     *                  that duplicates or any which are already present are
     *                  pruned automatically.
     *
     * @see addOneSegment()
     */
    void addSegments(QList<LineSegment::Side *> const &segments);

    /**
     * Add a single line segment to the subspace which is assumed to conform to,
     * or is compatible with the subspace.
     *
     * @param segment  Line segment to add. Ownership is @em NOT given to the
     *                 subspace. Note that if the segment is already present in
     *                 the set it will be pruned (nothing will happen).
     *
     * @see operator<<(), addSegments()
     */
    void addOneSegment(LineSegment::Side const &segment);

    /**
     * Add @a segment to the subspace which is assumed to conform to, or is
     * compatible with the subspace.
     *
     * @param segment  Line segment to add. Ownership is @em NOT given to the
     *                 subspace. Note that if the segment is already present in
     *                 the set it will be pruned (nothing will happen).
     *
     * @return  Reference to this subspace.
     *
     * @see addOneSegment()
     */
    inline ConvexSubspace &operator << (LineSegment::Side const &segment) {
        addOneSegment(segment);
        return *this;
    }

    /**
     * Build and assign all geometries to the BSP leaf specified. Note that
     * any existing geometries will be replaced (thus destroyed by BspLeaf).
     * Also, a map sector is chosen and attributed to the BSP leaf.
     *
     * @param bspLeaf  BSP leaf to build geometry for.
     */
    void buildGeometry(BspLeaf &bspLeaf) const;

    /**
     * The BspLeaf to which the subspace has been attributed. May return @c 0
     * if not attributed.
     *
     * @see setBspLeaf()
     *
     * @todo Refactor away.
     */
    BspLeaf *bspLeaf() const;

    /**
     * Change the BspLeaf to which the subspace is attributed.
     *
     * @param newBspLeaf  BSP leaf to attribute (ownership is unaffected).
     *                    Can be @c 0 (to clear the attribution).
     *
     * @see bspLeaf()
     *
     * @todo Refactor away.
     */
    void setBspLeaf(BspLeaf *newBspLeaf);

    /**
     * Provides a clockwise ordered list of the line segments in the subspace.
     */
    OrderedSegments const &segments() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_CONVEXSUBSPACE
