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

#include <de/mathutil.h>
#include <de/vector1.h>

#include <de/Error>
#include <de/Vector>

#include "HEdge"
#include "Line"
#include "Vertex"

#include "map/bsp/linesegment.h"

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

    /// Required twin line segment is missing. @ingroup errors
    DENG2_ERROR(MissingTwinError);

    /// Required line attribution is missing. @ingroup errors
    DENG2_ERROR(MissingLineSideError);

    /// Edge/vertex identifiers:
    enum
    {
        From,
        To
    };

public: /// @todo make private:
    Vector2d start;
    Vector2d end;
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

    /// Line side that this line segment initially comes (otherwise @c 0 signifying
    /// a "mini-segment").
    Line::Side *_lineSide;

    /// Line side that this line segment initially comes from. For "real" segments,
    /// this is just the same as @var lineSide. For "mini-segments", this is the
    /// the partition line side.
    Line::Side *sourceLineSide;

    /// Map sector attributed to the line segment. Can be @c 0 for "mini-segments".
    Sector *sector;

    /// Start and end vertexes of the segment (if any).
    Vertex *_from, *_to;

    /// Linked @em twin line segment (that on the other side of "this" line segment).
    LineSegment *_twin;

    /// Half-edge produced from this line segment (if any).
    HEdge *hedge;

public:
    LineSegment()
        : pLength(0),
          pAngle(0),
          pPara(0),
          pPerp(0),
          pSlopeType(ST_VERTICAL),
          nextOnSide(0),
          prevOnSide(0),
          bmapBlock(0),
          _lineSide(0),
          sourceLineSide(0),
          sector(0),
          _from(0),
          _to(0),
          _twin(0),
          hedge(0)
    {
        V2d_Set(direction, 0, 0);
    }

    LineSegment(LineSegment const &other)
        : start(other.start),
          end(other.end),
          pLength(other.pLength),
          pAngle(other.pAngle),
          pPara(other.pPara),
          pPerp(other.pPerp),
          pSlopeType(other.pSlopeType),
          nextOnSide(other.nextOnSide),
          prevOnSide(other.prevOnSide),
          bmapBlock(other.bmapBlock),
          _lineSide(other._lineSide),
          sourceLineSide(other.sourceLineSide),
          sector(other.sector),
          _from(other._from),
          _to(other._to),
          _twin(other._twin),
          hedge(other.hedge)
    {
        V2d_Copy(direction, other.direction);
    }

    LineSegment &operator = (LineSegment const &other)
    {
        start = other.start;
        end = other.end;
        V2d_Copy(direction, other.direction);
        pLength = other.pLength;
        pAngle = other.pAngle;
        pPara = other.pPara;
        pPerp = other.pPerp;
        pSlopeType = other.pSlopeType;
        nextOnSide = other.nextOnSide;
        prevOnSide = other.prevOnSide;
        bmapBlock = other.bmapBlock;
        _lineSide = other._lineSide;
        sourceLineSide = other.sourceLineSide;
        sector = other.sector;

        _from = other._from;
        _to   = other._to;
        _twin = other._twin;
        hedge = other.hedge;
        return *this;
    }

    void configure()
    {
        DENG_ASSERT(_from != 0);
        DENG_ASSERT(_to != 0);

        start = _from->origin();
        end   = _to->origin();
        Vector2d tempDir = end - start;
        V2d_Set(direction, tempDir.x, tempDir.y);

        pLength    = V2d_Length(direction);
        DENG2_ASSERT(pLength > 0);
        pAngle     = M_DirectionToAngle(direction);
        pSlopeType = M_SlopeType(direction);

        pPerp =  start.y * direction[VX] - start.x * direction[VY];
        pPara = -start.x * direction[VX] - start.y * direction[VY];
    }

    /**
     * Returns the specified edge vertex for the half-edge.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to);

    /// @copydoc vertex()
    Vertex const &vertex(int to) const;

    /**
     * Convenient accessor method for returning the origin of the From point
     * for the line segment.
     *
     * @see from()
     */
    de::Vector2d const &fromOrigin() const { return start; }

    /**
     * Convenient accessor method for returning the origin of the To point
     * for the line segment.
     *
     * @see to()
     */
    de::Vector2d const &toOrigin() const { return end; }

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
    bool hasLineSide() const;

    /**
     * Returns the Line::Side attributed to the line segment.
     *
     * @see hasLineSide()
     */
    Line::Side &lineSide() const;

    /**
     * Returns a pointer to the Line::Side attributed to the line segment.
     *
     * @see hasTwin()
     */
    inline Line::Side *lineSidePtr() const { return hasLineSide()? &lineSide() : 0; }

    /**
     * Convenient accessor method for returning the Line of the Line::Side
     * which is attributed to the line segment.
     *
     * @see hasLineSide(), lineSide()
     */
    inline Line &line() const { return lineSide().line(); }

    /**
     * Convenient accessor method for returning the Line side identifier of
     * the Line::Side attributed to the line segment.
     *
     * @see hasLineSide(), lineSide()
     */
    inline int lineSideId() const { return lineSide().lineSideId(); }

    /**
     * Returns @c true iff a BspLeaf is linked to the line segment.
     */
    bool hasBspLeaf() const;

    /**
     * Returns the BSP leaf for the line segment.
     */
    BspLeaf &bspLeaf() const;
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_LINESEGMENT
