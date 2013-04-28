/** @file map/bsp/linesegment.cpp BSP Builder Line Segment.
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

#include <de/mathutil.h>
#include <de/vector1.h> /// @todo remove me

#include "m_misc.h" // M_BoxOnLineSide2

#include <de/Observers>

#include "HEdge"
#include "map/bsp/superblockmap.h"

#include "map/bsp/linesegment.h"

namespace de {
namespace bsp {

DENG2_PIMPL(LineSegment),
DENG2_OBSERVES(Vertex, OriginChange)
{
    /// Vertexes of the segment (not owned).
    Vertex *from, *to;

    /// Direction vector from -> to.
    Vector2d direction;

    /// Linked @em twin segment (that on the other side of "this" line segment).
    LineSegment *twin;

    /// Map Line side that "this" segment initially comes from or @c 0 signifying
    /// a partition line segment (not owned).
    Line::Side *mapSide;

    /// Map Line side that "this" segment initially comes from. For map lines,
    /// this is just the same as @var mapSide. For partition lines this is the
    /// the partition line's map Line side. (Not owned.)
    Line::Side *sourceMapSide;

    /// Neighbor line segments relative to "this" segment along the source
    /// line (both partition and map lines).
    LineSegment *rightNeighbor;
    LineSegment *leftNeighbor;

    /// The superblock that contains this segment, or @c 0 if the segment is no
    /// longer in any superblock (e.g., now in or being turned into a leaf edge).
    SuperBlock *bmapBlock;

    /// Map sector attributed to the line segment. Can be @c 0 for partition lines.
    Sector *sector;

    // Precomputed data for faster calculations.
    coord_t pLength;
    coord_t pAngle;
    coord_t pPara;
    coord_t pPerp;
    slopetype_t pSlopeType;

    /// Half-edge produced from this segment (if any, not owned).
    HEdge *hedge;

    Instance(Public *i, Vertex &from_, Vertex &to_, Line::Side *mapSide,
             Line::Side *sourceMapSide)
        : Base(i),
          from(&from_),
          to(&to_),
          twin(0),
          mapSide(mapSide),
          sourceMapSide(sourceMapSide),
          rightNeighbor(0),
          leftNeighbor(0),
          bmapBlock(0),
          sector(0),
          pLength(0),
          pAngle(0),
          pPara(0),
          pPerp(0),
          pSlopeType(ST_VERTICAL),
          hedge(0)
    {
        from->audienceForOriginChange += this;
        to->audienceForOriginChange   += this;
    }

    ~Instance()
    {
        from->audienceForOriginChange -= this;
        to->audienceForOriginChange   -= this;
    }

    inline Vertex **vertexAdr(int edge) {
        return edge? &to : &from;
    }

    inline LineSegment **neighborAdr(int edge) {
        return edge? &rightNeighbor : &leftNeighbor;
    }

    void replaceVertex(int edge, Vertex &newVertex)
    {
        Vertex **adr = vertexAdr(edge);

        if(*adr && *adr == &newVertex) return;

        if(*adr) (*adr)->audienceForOriginChange -= this;
        *adr = &newVertex;
        if(*adr) (*adr)->audienceForOriginChange += this;

        updateCache();
    }

    /**
     * To be called to update precalculated vectors, distances, etc...
     * following a dependent vertex origin change notification.
     *
     * @todo Optimize: defer until next accessed. -ds
     */
    void updateCache()
    {
        direction  = to->origin() - from->origin();
        pLength    = direction.length();
        DENG2_ASSERT(pLength > 0);
        pAngle     = M_DirectionToAngleXY(direction.x, direction.y);
        pSlopeType = M_SlopeTypeXY(direction.x, direction.y);
        pPerp      =  from->origin().y * direction.x - from->origin().x * direction.y;
        pPara      = -from->origin().x * direction.x - from->origin().y * direction.y;
    }

    void vertexOriginChanged(Vertex &vertex, Vector2d const &oldOrigin, int changedAxes)
    {
        DENG2_UNUSED3(vertex, oldOrigin, changedAxes);
        updateCache();
    }
};

LineSegment::LineSegment(Vertex &from, Vertex &to, Line::Side *mapSide,
                         Line::Side *sourceMapSide)
    : d(new Instance(this, from, to, mapSide, sourceMapSide))
{
    d->updateCache();
}

LineSegment::LineSegment(LineSegment const &other)
    : d(new Instance(this, *other.d->from, *other.d->to,
                            other.d->mapSide, other.d->sourceMapSide))
{
    d->direction  = other.d->direction;
    d->twin       = other.d->twin;
    d->rightNeighbor = other.d->rightNeighbor;
    d->leftNeighbor = other.d->leftNeighbor;
    d->bmapBlock  = other.d->bmapBlock;
    d->sector     = other.d->sector;
    d->pLength    = other.d->pLength;
    d->pAngle     = other.d->pAngle;
    d->pPara      = other.d->pPara;
    d->pPerp      = other.d->pPerp;
    d->pSlopeType = other.d->pSlopeType;
    d->hedge      = other.d->hedge;
}

LineSegment &LineSegment::operator = (LineSegment const &other)
{
    d->direction     = other.d->direction;
    d->twin          = other.d->twin;
    d->mapSide       = other.d->mapSide;
    d->sourceMapSide = other.d->sourceMapSide;
    d->rightNeighbor    = other.d->rightNeighbor;
    d->leftNeighbor    = other.d->leftNeighbor;
    d->bmapBlock     = other.d->bmapBlock;
    d->sector        = other.d->sector;
    d->pLength       = other.d->pLength;
    d->pAngle        = other.d->pAngle;
    d->pPara         = other.d->pPara;
    d->pPerp         = other.d->pPerp;
    d->pSlopeType    = other.d->pSlopeType;
    d->hedge         = other.d->hedge;

    replaceFrom(*other.d->from);
    replaceTo(*other.d->to);

    d->updateCache();

    return *this;
}

Vertex &LineSegment::vertex(int to) const
{
    DENG_ASSERT(*d->vertexAdr(to) != 0);
    return **d->vertexAdr(to);
}

void LineSegment::replaceVertex(int to, Vertex &newVertex)
{
    d->replaceVertex(to, newVertex);
}

Vector2d const &LineSegment::direction() const
{
    return d->direction;
}

slopetype_t LineSegment::slopeType() const
{
    return d->pSlopeType;
}

coord_t LineSegment::angle() const
{
    return d->pAngle;
}

bool LineSegment::hasTwin() const
{
    return d->twin != 0;
}

LineSegment &LineSegment::twin() const
{
    if(d->twin)
    {
        return *d->twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("LineSegment::twin", "No twin line segment is associated");
}

void LineSegment::setTwin(LineSegment *newTwin)
{
    d->twin = newTwin;
}

bool LineSegment::hasHEdge() const
{
    return d->hedge != 0;
}

HEdge &LineSegment::hedge() const
{
    if(d->hedge)
    {
        return *d->hedge;
    }
    /// @throw MissingHEdgeError Attempted with no half-edge associated.
    throw MissingHEdgeError("LineSegment::hedge", "No half-edge is associated");
}

void LineSegment::setHEdge(HEdge *newHEdge)
{
    d->hedge = newHEdge;
}

bool LineSegment::hasMapSide() const
{
    return d->mapSide != 0;
}

Line::Side &LineSegment::mapSide() const
{
    if(d->mapSide)
    {
        return *d->mapSide;
    }
    /// @throw MissingMapSideError Attempted with no map line side attributed.
    throw MissingMapSideError("LineSegment::mapSide", "No map line side is attributed");
}

bool LineSegment::hasSourceMapSide() const
{
    return d->sourceMapSide != 0;
}

Line::Side &LineSegment::sourceMapSide() const
{
    if(d->sourceMapSide)
    {
        return *d->sourceMapSide;
    }
    /// @throw MissingMapSideError Attempted with no source map line side attributed.
    throw MissingMapSideError("LineSegment::sourceMapSide", "No source map line side is attributed");
}

bool LineSegment::hasNeighbor(int edge) const
{
    return *d->neighborAdr(edge) != 0;
}

LineSegment &LineSegment::neighbor(int edge) const
{
    LineSegment **neighborAdr = d->neighborAdr(edge);
    if(*neighborAdr)
    {
        return **neighborAdr;
    }
    /// @throw MissingNeighborError Attempted with no relevant neighbor attributed.
    throw MissingNeighborError("LineSegment::neighbor", QString("No neighbor %1 is attributed").arg(edge? "Right" : "Left"));
}

void LineSegment::setNeighbor(int edge, LineSegment *newNeighbor)
{
    *d->neighborAdr(edge) = newNeighbor;
}

SuperBlock *LineSegment::bmapBlockPtr() const
{
    return d->bmapBlock;
}

void LineSegment::setBMapBlock(SuperBlock *newBMapBlock)
{
    d->bmapBlock = newBMapBlock;
}

Sector *LineSegment::sectorPtr() const
{
    return d->sector;
}

void LineSegment::setSector(Sector *newSector)
{
    d->sector = newSector;
}

coord_t LineSegment::length() const
{
    return d->pLength;
}

coord_t LineSegment::distance(Vector2d point) const
{
    coord_t pointV1[2] = { point.x, point.y };
    coord_t directionV1[2] = { d->direction.x, d->direction.y };
    return V2d_PointLineParaDistance(pointV1, directionV1, d->pPara, d->pLength);
}

void LineSegment::distance(LineSegment const &other, coord_t *fromDist, coord_t *toDist) const
{
    // Any work to do?
    if(!fromDist && !toDist) return;

    /// @attention Ensure line segments produced from the partition's source
    /// line are always treated as collinear. This special case is only
    /// necessary due to precision inaccuracies when a line is split into
    /// multiple segments.
    if(hasSourceMapSide() && other.hasSourceMapSide() &&
       &sourceMapSide().line() == &other.sourceMapSide().line())
    {
        if(fromDist) *fromDist = 0;
        if(toDist)   *toDist   = 0;
        return;
    }

    coord_t toSegDirectionV1[2] = { other.direction().x, other.direction().y } ;

    if(fromDist)
    {
        coord_t fromV1[2] = { d->from->origin().x, d->from->origin().y };
        *fromDist = V2d_PointLinePerpDistance(fromV1, toSegDirectionV1, other.d->pPerp, other.d->pLength);
    }
    if(toDist)
    {
        coord_t toV1[2]   = { d->to->origin().x, d->to->origin().y };
        *toDist = V2d_PointLinePerpDistance(toV1, toSegDirectionV1, other.d->pPerp, other.d->pLength);
    }
}

/// @todo Might be a useful global utility function? -ds
static LineRelationship lineRelationship(coord_t fromDist, coord_t toDist)
{
    static coord_t const distEpsilon = LINESEGMENT_INCIDENT_DISTANCE_EPSILON;

    // Collinear with "this" line?
    if(de::abs(fromDist) <= distEpsilon && de::abs(toDist) <= distEpsilon)
    {
        return Collinear;
    }

    // To the right of "this" line?.
    if(fromDist > -distEpsilon && toDist > -distEpsilon)
    {
        // Close enough to intercept?
        if(fromDist < distEpsilon || toDist < distEpsilon) return RightIntercept;
        return Right;
    }

    // To the left of "this" line?
    if(fromDist < distEpsilon && toDist < distEpsilon)
    {
        // Close enough to intercept?
        if(fromDist > -distEpsilon || toDist > -distEpsilon) return LeftIntercept;
        return Left;
    }

    return Intersects;
}

LineRelationship LineSegment::relationship(LineSegment const &other,
    coord_t *retFromDist, coord_t *retToDist) const
{
    coord_t fromDist, toDist;
    distance(other, &fromDist, &toDist);

    LineRelationship rel = lineRelationship(fromDist, toDist);

    if(retFromDist) *retFromDist = fromDist;
    if(retToDist)   *retToDist   = toDist;

    return rel;
}

int LineSegment::boxOnSide(AABoxd const &box) const
{
    coord_t fromV1[2] = { d->from->origin().x, d->from->origin().y };
    coord_t directionV1[2] = { d->direction.x, d->direction.y } ;
    return M_BoxOnLineSide2(&box, fromV1, directionV1, d->pPerp, d->pLength,
                            LINESEGMENT_INCIDENT_DISTANCE_EPSILON);
}

void LineSegment::ceaseVertexObservation()
{
    d->from->audienceForOriginChange -= d.get();
    d->to->audienceForOriginChange   -= d.get();
}

} // namespace bsp
} // namespace de
