/** @file convexsubspaceproxy.h  (World BSP) Convex subspace proxy.
 *
 * @authors Copyright © 2013-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_WORLD_BSP_CONVEXSUBSPACEPROXY_H
#define DE_WORLD_BSP_CONVEXSUBSPACEPROXY_H

#include "linesegment.h"
#include "../../mesh/mesh.h"
#include "../sector.h"

#include <de/list.h>
#include <de/error.h>
#include <de/log.h>

namespace world {
namespace bsp {

/**
 * @ingroup bsp
 */
struct OrderedSegment
{
    LineSegmentSide *segment;
    double fromAngle;
    double toAngle;

    bool operator == (const OrderedSegment &other) const
    {
        return    de::fequal(fromAngle, other.fromAngle)
               && de::fequal(toAngle  , other.toAngle);
    }

#ifdef DE_DEBUG
    void debugPrint() const
    {
        LOGDEV_MAP_MSG("%p Angle: %1.6f %s -> Angle: %1.6f %s")
            << this
            << fromAngle
            << (segment ? segment->from().origin().asText() : "(null)")
            << toAngle
            << (segment ? segment->to().origin().asText() : "(null)");
    }
#endif
};
typedef de::List<OrderedSegment> OrderedSegments;

/**
 * Models a @em logical convex subspace in the partition plane, providing the
 * analysis functionality necessary to classify and then separate the segments
 * into unique geometries.
 *
 * Acts as staging area for the future construction of a ConvexSubspace.
 *
 * Here infinity (i.e., a subspace containing no segments) is considered to be
 * convex. Accordingly any segments linked to the subspace are @em not "owned"
 * by it.
 *
 * @note Important: It is the user's responsibility to ensure convexity else
 * many of the operations on the set of segments will be illogical.
 *
 * @todo This functionality could be merged into ConvexSubspace -ds
 *
 * @ingroup bsp
 */
class ConvexSubspaceProxy
{
public:
    /**
     * Construct an empty convex subspace proxy.
     */
    ConvexSubspaceProxy();

    /**
     * Construct a convex subspace proxy from a list of line @a segments.
     *
     * @param segments  List of line segments which are assumed to define a
     *                  convex subspace in the plane. Ownership of the segments
     *                  is @em NOT given to the subspace. Note that duplicates
     *                  are pruned automatically.
     */
    ConvexSubspaceProxy(de::List<LineSegmentSide *> const &segments);

    /**
     * Construct a convex subspace by duplicating @a other.
     */
    ConvexSubspaceProxy(const ConvexSubspaceProxy &other);

    ConvexSubspaceProxy &operator = (const ConvexSubspaceProxy &);

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
    void addSegments(de::List<LineSegmentSide *> const &segments);

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
    void addOneSegment(const LineSegmentSide &segment);

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
    inline ConvexSubspaceProxy &operator << (const LineSegmentSide &segment) {
        addOneSegment(segment);
        return *this;
    }

    /**
     * Build and assign all geometries to the BSP leaf specified. Note that
     * any existing geometries will be replaced (thus destroyed by BspLeaf).
     * Also, a map sector is chosen and attributed to the BSP leaf.
     *
     * @param bspLeaf  BSP leaf to build geometry for.
     * @param mesh     Mesh from which to assign geometry.
     */
    void buildGeometry(BspLeaf &bspLeaf, mesh::Mesh &mesh) const;

    /**
     * The BspLeaf to which the subspace has been attributed if any.
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
     *                    Use @c nullptr to clear.
     *
     * @see bspLeaf()
     *
     * @todo Refactor away.
     */
    void setBspLeaf(BspLeaf *newBspLeaf);

    /**
     * Provides a clockwise ordered list of the line segments in the subspace.
     */
    const OrderedSegments &segments() const;

private:
    DE_PRIVATE(d)
};

}  // namespace bsp
}  // namespace world

#endif  // DE_WORLD_BSP_CONVEXSUBSPACEPROXY_H
