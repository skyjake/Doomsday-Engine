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

DENG2_PIMPL_NOREF(LineSegment::Side)
{
    /// Owning line segment.
    LineSegment *line;

    /// Direction vector from -> to.
    Vector2d direction;

    /// Map Line side that "this" segment initially comes from or @c 0 signifying
    /// a partition line segment (not owned).
    Line::Side *mapSide;

    /// Map Line of the partition line which resulted in this segment due to
    /// splitting (not owned).
    Line *partitionMapLine;

    /// Neighbor line segments relative to "this" segment along the source
    /// line (both partition and map lines).
    LineSegment::Side *rightNeighbor;
    LineSegment::Side *leftNeighbor;

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

    Instance(LineSegment &line)
        : line(&line),
          mapSide(0),
          partitionMapLine(0),
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
    {}

    inline LineSegment::Side **neighborAdr(int edge) {
        return edge? &rightNeighbor : &leftNeighbor;
    }
};

LineSegment::Side::Side(LineSegment &line)
    : d(new Instance(line))
{}

void LineSegment::Side::updateCache()
{
    d->direction  = to().origin() - from().origin();
    d->pLength    = d->direction.length();
    DENG2_ASSERT(d->pLength > 0);
    d->pAngle     = M_DirectionToAngleXY(d->direction.x, d->direction.y);
    d->pSlopeType = M_SlopeTypeXY(d->direction.x, d->direction.y);
    d->pPerp      =  from().origin().y * d->direction.x - from().origin().x * d->direction.y;
    d->pPara      = -from().origin().x * d->direction.x - from().origin().y * d->direction.y;
}

LineSegment &LineSegment::Side::line() const
{
    return *d->line;
}

int LineSegment::Side::lineSideId() const
{
    return &d->line->front() == this? LineSegment::Front : LineSegment::Back;
}

Vector2d const &LineSegment::Side::direction() const
{
    return d->direction;
}

slopetype_t LineSegment::Side::slopeType() const
{
    return d->pSlopeType;
}

coord_t LineSegment::Side::angle() const
{
    return d->pAngle;
}

bool LineSegment::Side::hasHEdge() const
{
    return d->hedge != 0;
}

HEdge &LineSegment::Side::hedge() const
{
    if(d->hedge)
    {
        return *d->hedge;
    }
    /// @throw MissingHEdgeError Attempted with no half-edge associated.
    throw MissingHEdgeError("LineSegment::Side::hedge", "No half-edge is associated");
}

void LineSegment::Side::setHEdge(HEdge *newHEdge)
{
    d->hedge = newHEdge;
}

bool LineSegment::Side::hasMapSide() const
{
    return d->mapSide != 0;
}

Line::Side &LineSegment::Side::mapSide() const
{
    if(d->mapSide)
    {
        return *d->mapSide;
    }
    /// @throw MissingMapSideError Attempted with no map line side attributed.
    throw MissingMapSideError("LineSegment::Side::mapSide", "No map line side is attributed");
}

void LineSegment::Side::setMapSide(Line::Side *newMapSide)
{
    d->mapSide = newMapSide;
}

Line *LineSegment::Side::partitionMapLine() const
{
    return d->partitionMapLine;
}

void LineSegment::Side::setPartitionMapLine(Line *newMapLine)
{
    d->partitionMapLine = newMapLine;
}

bool LineSegment::Side::hasNeighbor(int edge) const
{
    return (*d->neighborAdr(edge)) != 0;
}

LineSegment::Side &LineSegment::Side::neighbor(int edge) const
{
    LineSegment::Side **neighborAdr = d->neighborAdr(edge);
    if(*neighborAdr)
    {
        return **neighborAdr;
    }
    /// @throw MissingNeighborError Attempted with no relevant neighbor attributed.
    throw MissingNeighborError("LineSegment::Side::neighbor", QString("No %1 neighbor is attributed").arg(edge? "Right" : "Left"));
}

void LineSegment::Side::setNeighbor(int edge, LineSegment::Side *newNeighbor)
{
    *d->neighborAdr(edge) = newNeighbor;
}

SuperBlock *LineSegment::Side::bmapBlockPtr() const
{
    return d->bmapBlock;
}

void LineSegment::Side::setBMapBlock(SuperBlock *newBMapBlock)
{
    d->bmapBlock = newBMapBlock;
}

bool LineSegment::Side::hasSector() const
{
    return d->sector != 0;
}

Sector &LineSegment::Side::sector() const
{
    if(d->sector)
    {
        return *d->sector;
    }
    /// @throw LineSegment::MissingSectorError Attempted with no sector attributed.
    throw LineSegment::MissingSectorError("LineSegment::Side::sector", "No sector is attributed");
}

void LineSegment::Side::setSector(Sector *newSector)
{
    d->sector = newSector;
}

coord_t LineSegment::Side::length() const
{
    return d->pLength;
}

coord_t LineSegment::Side::distance(Vector2d point) const
{
    coord_t pointV1[2] = { point.x, point.y };
    coord_t directionV1[2] = { d->direction.x, d->direction.y };
    return V2d_PointLineParaDistance(pointV1, directionV1, d->pPara, d->pLength);
}

void LineSegment::Side::distance(LineSegment::Side const &other, coord_t *fromDist, coord_t *toDist) const
{
    // Any work to do?
    if(!fromDist && !toDist) return;

    /// @attention Ensure line segments produced from the partition's source
    /// line are always treated as collinear. This special case is only
    /// necessary due to precision inaccuracies when a line is split into
    /// multiple segments.
    if(d->partitionMapLine != 0 && d->partitionMapLine == other.partitionMapLine())
    {
        if(fromDist) *fromDist = 0;
        if(toDist)   *toDist   = 0;
        return;
    }

    coord_t toSegDirectionV1[2] = { other.direction().x, other.direction().y } ;

    if(fromDist)
    {
        coord_t fromV1[2] = { from().origin().x, from().origin().y };
        *fromDist = V2d_PointLinePerpDistance(fromV1, toSegDirectionV1, other.d->pPerp, other.d->pLength);
    }
    if(toDist)
    {
        coord_t toV1[2]   = { to().origin().x, to().origin().y };
        *toDist = V2d_PointLinePerpDistance(toV1, toSegDirectionV1, other.d->pPerp, other.d->pLength);
    }
}

LineRelationship lineRelationship(coord_t fromDist, coord_t toDist)
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

LineRelationship LineSegment::Side::relationship(LineSegment::Side const &other,
    coord_t *retFromDist, coord_t *retToDist) const
{
    coord_t fromDist, toDist;
    distance(other, &fromDist, &toDist);

    LineRelationship rel = lineRelationship(fromDist, toDist);

    if(retFromDist) *retFromDist = fromDist;
    if(retToDist)   *retToDist   = toDist;

    return rel;
}

int LineSegment::Side::boxOnSide(AABoxd const &box) const
{
    coord_t fromV1[2] = { from().origin().x, from().origin().y };
    coord_t directionV1[2] = { d->direction.x, d->direction.y } ;
    return M_BoxOnLineSide2(&box, fromV1, directionV1, d->pPerp, d->pLength,
                            LINESEGMENT_INCIDENT_DISTANCE_EPSILON);
}

DENG2_PIMPL(LineSegment),
DENG2_OBSERVES(Vertex, OriginChange)
{
    /// Vertexes of the line segment (not owned).
    Vertex *from;
    Vertex *to;

    /// Sides of the line segment (owned).
    LineSegment::Side front;
    LineSegment::Side back;

    Instance(Public *i, Vertex &from_, Vertex &to_)
        : Base(i),
          from(&from_),
          to(&to_),
          front(*i),
          back(*i)
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

    void replaceVertex(int edge, Vertex &newVertex)
    {
        Vertex **adr = vertexAdr(edge);

        if(*adr && *adr == &newVertex) return;

        if(*adr) (*adr)->audienceForOriginChange -= this;
        *adr = &newVertex;
        if(*adr) (*adr)->audienceForOriginChange += this;

        front.updateCache();
        back.updateCache();
    }

    void vertexOriginChanged(Vertex &vertex, Vector2d const &oldOrigin, int changedAxes)
    {
        DENG2_UNUSED3(vertex, oldOrigin, changedAxes);
        front.updateCache();
        back.updateCache();
    }
};

LineSegment::LineSegment(Vertex &from, Vertex &to)
    : d(new Instance(this, from, to))
{
    d->front.updateCache();
    d->back.updateCache();
}

LineSegment::Side &LineSegment::side(int back)
{
    return back? d->back : d->front;
}

LineSegment::Side const &LineSegment::side(int back) const
{
    return back? d->back : d->front;
}

Vertex &LineSegment::vertex(int to) const
{
    DENG_ASSERT((to? d->to : d->from) != 0);
    return to? *d->to : *d->from;
}

void LineSegment::replaceVertex(int to, Vertex &newVertex)
{
    d->replaceVertex(to, newVertex);
}

} // namespace bsp
} // namespace de
