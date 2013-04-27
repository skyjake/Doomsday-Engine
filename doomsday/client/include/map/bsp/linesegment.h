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

#include <de/Error>
#include <de/Vector>

#include "Line"
#include "Vertex"

#include "map/bsp/linesegment.h"

class HEdge;

namespace de {
namespace bsp {

class SuperBlock;

/**
 * Models a finite line segment in the plane.
 *
 * @ingroup bsp
 */
class LineSegment
{
public:
    /// Required BSP leaf is missing. @ingroup errors
    DENG2_ERROR(MissingBspLeafError);

    /// Required twin segment is missing. @ingroup errors
    DENG2_ERROR(MissingTwinError);

    /// Required map line side attribution is missing. @ingroup errors
    DENG2_ERROR(MissingMapLineSideError);

    /// Edge/vertex identifiers:
    enum
    {
        From,
        To
    };

public: /// @todo make private:
    coord_t direction[2];

    // Precomputed data for faster calculations.
    coord_t pLength;
    coord_t pAngle;
    coord_t pPara;
    coord_t pPerp;
    slopetype_t pSlopeType;

    LineSegment *nextOnSide;
    LineSegment *prevOnSide;

    /// The superblock that contains this segment, or @c 0 if the segment is no
    /// longer in any superblock (e.g., now in or being turned into a leaf edge).
    SuperBlock *bmapBlock;

    /// Map Line side that this line segment initially comes (otherwise @c 0 signifying
    /// a "mini-segment").
    Line::Side *_lineSide;

    /// Line side that this line segment initially comes from. For "real" segments,
    /// this is just the same as @var lineSide. For "mini-segments", this is the
    /// the partition line side.
    Line::Side *sourceLineSide;

    /// Map sector attributed to the line segment. Can be @c 0 for "mini-segments".
    Sector *sector;

    /// Linked @em twin line segment (that on the other side of "this" line segment).
    LineSegment *_twin;

    /// Half-edge produced from this line segment (if any).
    HEdge *hedge;

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
     */
    LineSegment &twin() const;

    /**
     * Returns a pointer to the linked @em twin of the line segment;
     * otherwise @c 0.
     *
     * @see hasTwin()
     */
    inline LineSegment *twinPtr() const { return hasTwin()? &twin() : 0; }

    /**
     * Returns @c true iff a Line::Side is attributed to the line segment.
     */
    bool hasMapLineSide() const;

    /**
     * Returns the Line::Side attributed to the line segment.
     *
     * @see hasLineSide()
     */
    Line::Side &mapLineSide() const;

    /**
     * Returns a pointer to the Line::Side attributed to the line segment.
     *
     * @see hasTwin()
     */
    inline Line::Side *lineSidePtr() const { return hasMapLineSide()? &mapLineSide() : 0; }

    /**
     * Convenient accessor method for returning the Line of the Line::Side
     * which is attributed to the line segment.
     *
     * @see hasLineSide(), lineSide()
     */
    inline Line &line() const { return mapLineSide().line(); }

    /**
     * Convenient accessor method for returning the Line side identifier of
     * the Line::Side attributed to the line segment.
     *
     * @see hasLineSide(), lineSide()
     */
    inline int lineSideId() const { return mapLineSide().lineSideId(); }

    /**
     * Returns @c true iff a BspLeaf is linked to the line segment.
     */
    bool hasBspLeaf() const;

    /**
     * Returns the BSP leaf for the line segment.
     */
    BspLeaf &bspLeaf() const;

    /// @todo refactor away -ds
    void ceaseVertexObservation();

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_LINESEGMENT
