/** @file linesegment.h  World BSP Line Segment.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_WORLD_BSP_LINESEGMENT_H
#define DE_WORLD_BSP_LINESEGMENT_H

#include <de/error.h>
#include <de/vector.h>

#include "../../mesh/hedge.h"
#include "../line.h"
#include "../vertex.h"

/// Rounding threshold within which two points are considered as co-incident.
#define LINESEGMENT_INCIDENT_DISTANCE_EPSILON       1.0 / 128

namespace world {
namespace bsp {

class ConvexSubspaceProxy;

/**
 * LineRelationship delineates the possible logical relationships between two
 * line (segments) in the plane.
 *
 * @ingroup bsp
 */
enum LineRelationship
{
    Collinear,
    Right,
    RightIntercept, ///< Right vertex intercepts.
    Left,
    LeftIntercept,  ///< Left vertex intercepts.
    Intersects
};

/// @todo Might be a useful global utility function? -ds
LineRelationship lineRelationship(coord_t fromDist, coord_t toDist);

/**
 * Models a finite line segment in the plane.
 *
 * @ingroup bsp
 */
class LineSegment
{
    DE_NO_COPY(LineSegment)
    DE_NO_ASSIGN(LineSegment)

public:
    /// Required sector attribution is missing. @ingroup errors
    DE_ERROR(MissingSectorError);

    /// Logical side identifiers:
    enum { Front, Back };

    /// Vertex identifiers:
    enum { From, To };

    /// Edge identifiers:
    enum { Left, Right };

    /**
     * Logical side of which there are always two (a front and a back).
     */
    class Side
    {
        DE_NO_COPY(Side)
        DE_NO_ASSIGN(Side)

    public:
        /// Required neighbor segment is missing. @ingroup errors
        DE_ERROR(MissingNeighborError);

        /// Required map line side attribution is missing. @ingroup errors
        DE_ERROR(MissingMapSideError);

        /// Required half-edge is missing. @ingroup errors
        DE_ERROR(MissingHEdgeError);

    public:
        Side(LineSegment &line);

        /**
         * Returns the specified relative vertex from the LineSegment owner.
         *
         * @see lineSideId(), line(), LineSegment::vertex(),
         */
        inline Vertex &vertex(int to) const { return line().vertex(lineSideId() ^ to); }

        /**
         * Returns the relative From Vertex for "this" side, from the LineSegment owner.
         *
         * @see vertex(), to()
         */
        inline Vertex &from() const { return vertex(From); }

        /**
         * Returns the relative To Vertex for "this" side, from the LineSegment owner.
         *
         * @see vertex(), from()
         */
        inline Vertex &to() const { return vertex(To); }

        /**
         * Returns the LineSegment owner of the side.
         */
        LineSegment &line() const;

        /**
         * Returns the logical identifier for "this" side (Front or Back).
         */
        int lineSideId() const;

        /**
         * Returns @c true iff this is the front side of the owning line segment.
         *
         * @see lineSideId()
         */
        inline bool isFront() const { return lineSideId() == Front; }

        /**
         * Returns @c true iff this is the back side of the owning line segment.
         *
         * @see lineSideId(), isFront()
         */
        inline bool isBack() const { return !isFront(); }

        /**
         * Returns the relative back Side from the line segment owner.
         *
         * @see lineSideId(), line(), LineSegment::side(),
         */
        inline Side &back() const { return line().side(lineSideId() ^ 1); }

        /**
         * Returns @c true iff a map LineSide is attributed to "this" side of
         * the line segment.
         */
        bool hasMapSide() const;

        /// @copydoc hasMapSide()
        inline bool hasMapLine() { return hasMapSide(); }

        /**
         * Returns the map LineSide attributed to "this" side of the line segment.
         *
         * @see hasMapSide()
         */
        LineSide &mapSide() const;

        /**
         * Returns a pointer to the map LineSide attributed to this side of the
         * line segment; otherwise @c nullptr
         */
        inline LineSide *mapSidePtr() const { return hasMapSide()? &mapSide() : nullptr; }

        /**
         * Change the map LineSide attributed to the "this" side of the line
         * segment.
         *
         * @param newMapSide  New map line side to attribute. Can be @c nullptr.
         */
        void setMapSide(LineSide *newMapSide);

        /**
         * Returns a pointer to the @em partition map Line attributed to "this"
         * side of the line segment (if any).
         */
        Line *partitionMapLine() const;

        /**
         * Change the @em partition map line attributed to "this" side of the
         * line segment.
         *
         * @param newMapLine  New map "partition" line. Can be @c nullptr.
         */
        void setPartitionMapLine(Line *newMapLine);

        /**
         * Convenient accessor method for returning the map Line of the LineSide
         * is attributed to "this" side of the line segment.
         *
         * @see hasMapSide(), mapSide()
         */
        inline Line &mapLine() const { return mapSide().line(); }

        /**
         * Returns true iff the specified @a edge neighbor segment side is configured.
         *
         * @param edge  If non-zero test the Right neighbor, otherwise the Left.
         *
         * @see neighbor()
         */
        bool hasNeighbor(int edge) const;

        /**
         * Returns true iff a @em Left edge neighbor is configured.
         */
        inline bool hasLeft() const { return hasNeighbor(Left); }

        /**
         * Returns true iff a @em Right edge neighbor is configured.
         */
        inline bool hasRight() const { return hasNeighbor(Right); }

        /**
         * Returns the specified @a edge neighbor of "this" side of the line segment.
         *
         * @param edge  If non-zero retrieve the @em Right neighbor, otherwise the
         *              @em Left.
         *
         * @see hasNeighbor()
         */
        Side &neighbor(int edge) const;

        /**
         * Returns the @em Left neighbor of "this" side of the line segment.
         * @see neighbor(), hasLeft()
         */
        inline Side &left() const { return neighbor(Left); }

        /**
         * Returns the @em Right neighbor of "this" side of the line segment.
         * @see neighbor(), hasRight()
         */
        inline Side &right() const { return neighbor(Right); }

        /**
         * Change the specified @a edge neighbor of "this" side of the line segment.
         *
         * @param edge  If non-zero change the @em Right neighbor, otherwise the @em Left.
         *
         * @param newNeighbor  New line segment side to set as the neighbor. Can be @c nullptr.
         */
        void setNeighbor(int edge, Side *newNeighbor);

        /**
         * Change the @em Left neighbor of the "this" side of the line segment.
         *
         * @param newLeft  New left neighbor line segment side. Can be @c nullptr.
         *
         * @see setNeighbor()
         */
        inline void setLeft(Side *newLeft) { setNeighbor(Left, newLeft); }

        /**
         * Change the @em Right neighbor of the "this" side of the line segment.
         *
         * @param newRight  New right neighbor line segment side. Can be @c nullptr.
         *
         * @see setNeighbor()
         */
        inline void setRight(Side *newRight) { setNeighbor(Right, newRight); }

        /**
         * Returns the line segment block tree node that contains "this" side of
         * the line segment; otherwise @c nullptr if not contained.
         */
        /*LineSegmentBlockTreeNode*/ void *blockTreeNodePtr() const;

        /**
         * Change the line segment block tree node to which "this" side of the
         * line segment is associated.
         *
         * @param newNode  New tree node. Use @c nullptr to clear.
         */
        void setBlockTreeNode(/*LineSegmentBlockTreeNode*/ void *newNode);

        /**
         * Returns @c true iff a map sector is attributed to "this" side of the
         * line segment.
         */
        bool hasSector() const;

        /**
         * Returns the map sector attributed to "this" side of the line segment.
         *
         * @see hasSector()
         */
        Sector &sector() const;

        /**
         * Returns a pointer to the Sector attributed to "this" side of the line
         * segment; otherwise @c nullptr.
         *
         * @see hasSector()
         */
        inline Sector *sectorPtr() const { return hasSector() ? &sector() : nullptr; }

        /**
         * Change the sector attributed to "this" side of the line segment.
         *
         * @param newSector  New sector to attribute. Ownership is unaffected.
         *                   Use @c nullptr to clear.
         */
        void setSector(Sector *newSector);

        /**
         * Returns a direction vector for "this" side of the line segment,
         * from the From/Start vertex origin to the To/End vertex origin.
         */
        const de::Vec2d &direction() const;

        /**
         * Returns the logical @em slopetype for "this" side of the line
         * segment (which, is determined from the world direction).
         *
         * @see direction()
         * @see M_SlopeType()
         */
        slopetype_t slopeType() const;

        /**
         * Returns the accurate length of the line segment from the From/Start to
         * vertex origin to the To/End vertex origin.
         */
        coord_t length() const;

        /**
         * Returns the world angle of "this" side of the line segment (which,
         * is derived from the direction vector).
         *
         * @see inverseAngle(), direction()
         */
        coord_t angle() const;

        /**
         * Calculates the @em parallel distance from "this" side of the line
         * segment to the specified @a point in the plane (i.e., in the
         * direction of this side).
         *
         * @return  Distance to the point expressed as a fraction/scale factor.
         */
        coord_t distance(de::Vec2d point) const;

        /**
         * Calculate @em perpendicular distances from one or both of the
         * vertexe(s) of "this" side of the line segment to the @a other line
         * segment side. For this operation the @a other line segment is
         * interpreted as an infinite line. The vertexe(s) of "this" side of
         * the line segment are projected onto the conceptually infinite line
         * defined by @a other and the length of the resultant vector(s) are
         * then determined.
         *
         * @param other  Other line segment side to determine distances to.
         *
         * Return values:
         * @param fromDist  Perpendicular distance from the "from" vertex.
         *                  Can be @c nullptr.
         * @param toDist    Perpendicular distance from the "to" vertex.
         *                  Can be @c nullptr.
         */
        void distance(const Side &other, coord_t *fromDist = nullptr,
                      coord_t *toDist = nullptr) const;

        /**
         * Determine the logical relationship between "this" line segment side
         * and the @a other. In doing so the perpendicular distance for the
         * vertexes of the line segment side are calculated (and optionally
         * returned).
         *
         * @param other  Other line segment side to determine relationship to.
         *
         * Return values:
         * @param retFromDist  Perpendicular distance from the "from" vertex.
         *                     Can be @c nullptr.
         * @param retToDist    Perpendicular distance from the "to" vertex.
         *                     Can be @c nullptr.
         *
         * @return LineRelationship between the line segments.
         *
         * @see distance()
         */
        LineRelationship relationship(const Side &other, coord_t *retFromDist,
                                      coord_t *retToDist) const;

        /// @see M_BoxOnLineSide2()
        int boxOnSide(const AABoxd &box) const;

        /**
         * Returns the axis-aligned bounding box of the line segment (derived
         * from the coordinates of the two vertexes).
         */
        inline AABoxd bounds() const { return line().bounds(); }

        /**
         * Returns @c true iff a built half-edge is linked to "this" side of
         * the line segment.
         *
         * @see hedge()
         */
        bool hasHEdge() const;

        /**
         * Returns the built half-edge for "this" side of the line segment.
         *
         * @see hasHEdge()
         */
        mesh::HEdge &hedge() const;

        /**
         * Returns a pointer to the built half-edge linked to "this" side of
         * the line segment. otherwise @c nullptr.
         *
         * @see hasHEdge()
         */
        inline mesh::HEdge *hedgePtr() const { return hasHEdge()? &hedge() : nullptr; }

        /**
         * Change the built half-edge linked to "this" side of the line segment.
         *
         * @param newHEdge New half-edge. Can be @c nullptr.
         *
         * @see hedge()
         */
        void setHEdge(mesh::HEdge *newHEdge);

        /**
         * Returns a pointer to the ConvexSubspaceProxy to which "this" side of the
         * line segment is attributed. May return @c nullptr if not yet attributed.
         */
        ConvexSubspaceProxy *convexSubspace() const;

        /**
         * Change the convex subspace to which "this" side of the line segment
         * is attributed.
         *
         * @param newConvexSubspace  ConvexSubspace to attribute. Use @c nullptr to
         *                           clear the attribution.
         */
        void setConvexSubspace(ConvexSubspaceProxy *newConvexSubspace);

        /**
         * To be called to update precalculated vectors, distances, etc...
         * following a dependent vertex origin change notification.
         *
         * @todo Optimize: defer until next accessed. -ds
         * @todo Make private. -ds
         */
        void updateCache();

    private:
        DE_PRIVATE(d)
    };

public:
    LineSegment(Vertex &from, Vertex &to);

    /**
     * Returns the specified logical side of the line segment.
     *
     * @param back  If not @c nullptr return the Back side; otherwise the Front side.
     */
    Side       &side(int back);
    const Side &side(int back) const;

    /**
     * Returns the logical Front side of the line segment.
     */
    inline Side       &front()       { return side(Front); }
    inline const Side &front() const { return side(Front); }

    /**
     * Returns the logical Back side of the line segment.
     */
    inline Side       &back()        { return side(Back); }
    inline const Side &back() const  { return side(Back); }

    /**
     * Returns the specified edge vertex of the line segment.
     *
     * @param to  If not @c nullptr return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to) const;

    /**
     * Convenient accessor method for returning the origin of the specified
     * edge vertex for the line segment.
     *
     * @see vertex()
     */
    inline const de::Vec2d &vertexOrigin(int to) const {
        return vertex(to).origin();
    }

    /**
     * Returns the From/Start vertex for the line segment.
     */
    inline Vertex &from() const { return vertex(From); }

    /**
     * Convenient accessor method for returning the origin of the From/Start
     * vertex for the line segment.
     *
     * @see from()
     */
    inline const de::Vec2d &fromOrigin() const { return from().origin(); }

    /**
     * Returns the To/End vertex for the line segment.
     */
    inline Vertex &to() const { return vertex(To); }

    /**
     * Convenient accessor method for returning the origin of the To/End vertex
     * for the line segment.
     *
     * @see to()
     */
    inline const de::Vec2d &toOrigin() const { return to().origin(); }

    /**
     * Returns the axis-aligned bounding box of the line segment (derived from
     * the coordinates of the two vertexes).
     *
     * @todo Cache this result.
     */
    AABoxd bounds() const;

    /**
     * Replace the specified edge vertex of the line segment.
     *
     * @param to  If not @c nullptr replace the To vertex; otherwise the From vertex.
     * @param newVertex  The replacement vertex.
     */
    void replaceVertex(int to, Vertex &newVertex);

    inline void replaceFrom(Vertex &newVertex) { replaceVertex(From, newVertex); }
    inline void replaceTo  (Vertex &newVertex) { replaceVertex(To  , newVertex); }

private:
    DE_PRIVATE(d)
};

typedef LineSegment::Side LineSegmentSide;

}  // namespace bsp
}  // namespace world

#endif  // DE_WORLD_BSP_LINESEGMENT_H
