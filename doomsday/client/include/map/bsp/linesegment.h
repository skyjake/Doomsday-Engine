/** @file map/bsp/linesegment.h BSP Builder Line Segment.
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

#ifndef DENG_WORLD_MAP_BSP_LINESEGMENT
#define DENG_WORLD_MAP_BSP_LINESEGMENT

#include <de/mathutil.h> // M_InverseAngle

#include <de/Error>
#include <de/Vector>

#include "Line"
#include "Vertex"

#include "map/bsp/linesegment.h"

/// Rounding threshold within which two points are considered as co-incident.
#define LINESEGMENT_INCIDENT_DISTANCE_EPSILON       1.0 / 128

class HEdge;

namespace de {
namespace bsp {

class SuperBlock;

/**
 * LineRelationship delineates the possible logical relationships between two
 * line (segments) in the plane.
 */
enum LineRelationship
{
    Collinear = 0,
    Right,
    RightIntercept, ///< Right vertex intercepts.
    Left,
    LeftIntercept,  ///< Left vertex intercepts.
    Intersects
};

/**
 * Models a finite line segment in the plane.
 *
 * @ingroup bsp
 */
class LineSegment
{
public:
    /// Required half-edge is missing. @ingroup errors
    DENG2_ERROR(MissingHEdgeError);

    /// Required twin segment is missing. @ingroup errors
    DENG2_ERROR(MissingTwinError);

    /// Required map line side attribution is missing. @ingroup errors
    DENG2_ERROR(MissingMapSideError);

    /// Edge/vertex identifiers:
    enum
    {
        From,
        To
    };

public: /// @todo make private:
    LineSegment *nextOnSide;
    LineSegment *prevOnSide;

    /// The superblock that contains this segment, or @c 0 if the segment is no
    /// longer in any superblock (e.g., now in or being turned into a leaf edge).
    SuperBlock *bmapBlock;

    /// Map sector attributed to the line segment. Can be @c 0 for partition lines.
    Sector *sector;

public:
    LineSegment(Vertex &from, Vertex &to,
                Line::Side *side = 0, Line::Side *sourceLineSide = 0);
    LineSegment(LineSegment const &other);

    LineSegment &operator = (LineSegment const &other);

    /**
     * Returns the specified edge vertex for the half-edge.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to) const;

    /**
     * Returns the From/Start vertex for the line segment.
     */
    inline Vertex &from() const { return vertex(From); }

    /**
     * Convenient accessor method for returning the origin of the From point
     * for the line segment.
     *
     * @see from()
     */
    inline Vector2d const &fromOrigin() const { return from().origin(); }

    /**
     * Returns the To/End vertex for the line segment.
     */
    inline Vertex &to() const { return vertex(To); }

    /**
     * Convenient accessor method for returning the origin of the To point
     * for the line segment.
     *
     * @see to()
     */
    inline Vector2d const &toOrigin() const { return to().origin(); }

    /**
     * Replace the specified edge vertex of the line segment.
     *
     * @param to  If not @c 0 replace the To vertex; otherwise the From vertex.
     * @param newVertex  The replacement vertex.
     */
    void replaceVertex(int to, Vertex &newVertex);

    inline void replaceFrom(Vertex &newVertex) { replaceVertex(From, newVertex); }
    inline void replaceTo(Vertex &newVertex)   { replaceVertex(To, newVertex); }

    /**
     * Returns @c true iff a @em twin is linked to the line segment.
     */
    bool hasTwin() const;

    /**
     * Returns the linked @em twin of the line segment.
     *
     * @see hasTwin(), setTwin()
     */
    LineSegment &twin() const;

    /**
     * Returns a pointer to the linked @em twin of the line segment; otherwise @c 0.
     *
     * @see hasTwin()
     */
    inline LineSegment *twinPtr() const { return hasTwin()? &twin() : 0; }

    /**
     * Change the linked @em twin of the line segment.
     *
     * @param newTwin  New twin for the line segment. Can be @c 0.
     *
     * @see twin()
     */
    void setTwin(LineSegment *newTwin);

    /**
     * Returns @c true iff a map Line::Side is attributed to the line segment.
     */
    bool hasMapSide() const;

    /**
     * Returns the map Line::Side attributed to the line segment.
     *
     * @see hasMapSide()
     */
    Line::Side &mapSide() const;

    /**
     * Returns a pointer to the map Line::Side attributed to the line segment.
     *
     * @see hasMapSide()
     */
    inline Line::Side *mapSidePtr() const { return hasMapSide()? &mapSide() : 0; }

    /**
     * Returns @c true iff a @em source map Line::Side is attributed to the line segment.
     */
    bool hasSourceMapSide() const;

    /**
     * Returns the @em source map Line::Side attributed to the line segment.
     *
     * @see hasSourceMapSide()
     */
    Line::Side &sourceMapSide() const;

    /**
     * Returns a pointer to the @em source map Line::Side attributed to the line segment.
     *
     * @see hasSourceMapSide()
     */
    inline Line::Side *sourceMapSidePtr() const { return hasSourceMapSide()? &sourceMapSide() : 0; }

    /**
     * Convenient accessor method for returning the map Line of the Line::Side
     * is attributed to the line segment.
     *
     * @see hasMapSide(), mapSide()
     */
    inline Line &line() const { return mapSide().line(); }

    /**
     * Convenient accessor method for returning the map Line side identifier of
     * the Line::Side attributed to the line segment.
     *
     * @see hasMapSide(), mapSide()
     */
    inline int mapLineSideId() const { return mapSide().lineSideId(); }

    /**
     * Returns @c true iff a half-edge is linked to the line segment.
     *
     * @see hedge()
     */
    bool hasHEdge() const;

    /**
     * Returns the linked half-edge for the line segment.
     *
     * @see hasHEdge()
     */
    HEdge &hedge() const;

    /**
     * Returns a pointer to the linked half-edge of the line segment; otherwise @c 0.
     *
     * @see hasHEdge()
     */
    inline HEdge *hedgePtr() const { return hasHEdge()? &hedge() : 0; }

    /**
     * Change the linked half-edge of the line segment.
     *
     * @param newHEdge  New half-edge for the line segment. Can be @c 0.
     *
     * @see hedge()
     */
    void setHEdge(HEdge *newHEdge);

    /**
     * Returns a direction vector for the line segment from the From/Start vertex
     * origin to the To/End vertex origin.
     */
    Vector2d const &direction() const;

    /**
     * Returns the logical @em slopetype for the line segment (which, is determined
     * according to the world direction).
     *
     * @see direction()
     * @see M_SlopeType()
     */
    slopetype_t slopeType() const;

    /**
     * Returns the accurate length of the line segment from the From/Start to vertex
     * origin to the To/End vertex origin.
     */
    coord_t length() const;

    /**
     * Returns the world angle of the line (which, is derived from the direction
     * vector).
     *
     * @see inverseAngle(), direction()
     */
    coord_t angle() const;

    /**
     * Returns the inverted world angle for the line (i.e., rotated 180 degrees).
     *
     * @see angle()
     */
    inline coord_t inverseAngle() const { return M_InverseAngle(angle()); }

    /**
     * Calculates the @em parallel distance from the line segment to the specified
     * @a point in the plane (i.e., along the direction of the line).
     *
     * @return  Distance to the point expressed as a fraction/scale factor.
     */
    coord_t distance(Vector2d point) const;

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
    void distance(LineSegment const &other, coord_t *fromDist = 0, coord_t *toDist = 0) const;

    /**
     * Determine the logical relationship between "this" line segment and the
     * @a other. In doing so the perpendicular distance for the vertexes of the
     * line segment are calculated (and optionally returned).
     *
     * @param other     Other line segment to determine relationship to.
     *
     * Return values:
     * @param fromDist  Perpendicular distance from the "from" vertex. Can be @c 0.
     * @param toDist    Perpendicular distance from the "to" vertex. Can be @c 0.
     *
     * @return LineRelationship between the line segments.
     *
     * @see distance()
     */
    LineRelationship relationship(LineSegment const &other, coord_t *retFromDist,
                                  coord_t *retToDist) const;

    /// @see M_BoxOnLineSide2()
    int boxOnSide(AABoxd const &box) const;

    /// @todo refactor away -ds
    void ceaseVertexObservation();

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_LINESEGMENT
