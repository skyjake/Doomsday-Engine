/** @file map/bsp/hplane.cpp BSP Builder Half-plane.
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

#include <memory>

#include <QtAlgorithms>

#include <de/Error>
#include <de/Log>

#include "Line"
#include "Sector"
#include "Vertex"

#include "map/bsp/edgetip.h"
#include "map/bsp/linesegment.h"
#include "map/bsp/partitioner.h"

#include "map/bsp/hplane.h"

namespace de {
namespace bsp {

HPlane::Intercept::Intercept(ddouble distance, LineSegment::Side &lineSeg, int edge)
    : selfRef(false),
      before(0),
      after(0),
      _distance(distance),
      _lineSeg(&lineSeg),
      _edge(edge)
{}

LineSegment::Side &HPlane::Intercept::lineSegment() const
{
    return *_lineSeg;
}

int HPlane::Intercept::lineSegmentEdge() const
{
    return _edge;
}

#ifdef DENG_DEBUG
void HPlane::Intercept::debugPrint() const
{
    LOG_INFO("Vertex #%i %s beforeSector: #%d afterSector: #%d %s")
        << vertex().indexInMap()
        << vertex().origin().asText()
        << (before? before->indexInMap() : -1)
        << (after? after->indexInMap() : -1)
        << (selfRef? "SELFREF" : "");
}
#endif

DENG2_PIMPL(HPlane)
{
    /// The partition line.
    Partition partition;

    /// Direction vector length.
    coord_t length;

    /// World angle.
    coord_t angle;

    /// Logical line slope (i.e., world angle) classification.
    slopetype_t slopeType;

    /// Perpendicular scale factor.
    coord_t perp;

    /// Parallel scale factor.
    coord_t para;

    /// Line segment from which the partition line was derived (if any).
    LineSegment::Side *lineSegment;

    /// Intercept points along the half-plane.
    Intercepts intercepts;

    /// Set to @c true when @var intercepts requires sorting.
    bool needSortIntercepts;

    Instance(Public *i, Vector2d const &partitionOrigin,
             Vector2d const &partitionDirection)
        : Base(i),
          partition(partitionOrigin, partitionDirection),
          length(partition.direction.length()),
          angle(M_DirectionToAngleXY(partition.direction.x, partition.direction.y)),
          slopeType(M_SlopeTypeXY(partition.direction.x, partition.direction.y)),
          perp(partition.origin.y * partition.direction.x - partition.origin.x * partition.direction.y),
          para(-partition.origin.x * partition.direction.x - partition.origin.y * partition.direction.y),
          lineSegment(0),
          needSortIntercepts(false)
    {}

    /**
     * Search the list of intercepts for to see if there is one for the
     * specified @a vertex.
     *
     * @param vertex  The vertex to look for.
     *
     * @return  @c true iff an intercept for @a vertex was found.
     */
    bool haveInterceptForVertex(Vertex const &vertex) const
    {
        foreach(Intercept const &icpt, intercepts)
        {
            if(&icpt.vertex() == &vertex)
                return true;
        }
        return false;
    }
};

HPlane::HPlane(Vector2d const &partitionOrigin, Vector2d const &partitionDirection)
    : d(new Instance(this, partitionOrigin, partitionDirection))
{}

void HPlane::clearIntercepts()
{
    d->intercepts.clear();
    // An empty intercept list is logically sorted.
    d->needSortIntercepts = false;
}

void HPlane::configure(LineSegment::Side const &newBaseSeg)
{
    // Only map line segments are suitable.
    DENG_ASSERT(newBaseSeg.hasMapSide());

    LOG_AS("HPlane::configure");

    // Clear the list of intersection points.
    clearIntercepts();

    // Reconfigure the partition line.
    Line::Side &mapSide = newBaseSeg.mapSide();

    d->partition.origin    = mapSide.from().origin();
    d->partition.direction = mapSide.to().origin() - mapSide.from().origin();

    d->lineSegment = const_cast<LineSegment::Side *>(&newBaseSeg);

    d->length    = d->partition.direction.length();
    d->angle     = M_DirectionToAngleXY(d->partition.direction.x, d->partition.direction.y);
    d->slopeType = M_SlopeTypeXY(d->partition.direction.x, d->partition.direction.y);

    d->perp = d->partition.origin.y * d->partition.direction.x
            - d->partition.origin.x * d->partition.direction.y;

    d->para = -d->partition.origin.x * d->partition.direction.x
            -  d->partition.origin.y * d->partition.direction.y;

    //LOG_DEBUG("line segment %p %s.")
    //    << de::dintptr(&newBaseSeg) << d->partition.asText();
}

/**
 * Determines whether a conceptual line oriented at @a vtx and "pointing"
 * at the specified world @a angle enters an "open" sector (which is to say
 * that said line does not enter void space and does not intercept with any
 * existing map or partition line segment in the plane, thus "closed").
 *
 * @return  The "open" sector at this angle; otherwise @c 0 (closed).
 */
static Sector *openSectorAtAngle(EdgeTips const &tips, coord_t angle)
{
    DENG_ASSERT(!tips.isEmpty());

    // First check whether there's a wall_tip that lies in the exact
    // direction of the given direction (which is relative to the vertex).
    DENG2_FOR_EACH_CONST(EdgeTips::All, it, tips.all())
    {
        EdgeTip const &tip = *it;
        coord_t diff = de::abs(tip.angle() - angle);
        if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
        {
            return 0; // Yes, found one.
        }
    }

    // OK, now just find the first wall_tip whose angle is greater than
    // the angle we're interested in. Therefore we'll be on the front side
    // of that tip edge.
    DENG2_FOR_EACH_CONST(EdgeTips::All, it, tips.all())
    {
        EdgeTip const &tip = *it;
        if(angle + ANG_EPSILON < tip.angle())
        {
            // Found it.
            return (tip.hasFront()? tip.front().sectorPtr() : 0);
        }
    }

    // Not found. The open sector will therefore be on the back of the tip
    // at the greatest angle.
    EdgeTip const &tip = tips.all().back();
    return (tip.hasBack()? tip.back().sectorPtr() : 0);
}

double HPlane::intersect(LineSegment::Side const &lineSeg, int edge)
{
    Vertex &vertex = lineSeg.vertex(edge);
    coord_t pointV1[2] = { vertex.origin().x, vertex.origin().y };
    coord_t directionV1[2] = { d->partition.direction.x, d->partition.direction.y };
    return V2d_PointLineParaDistance(pointV1, directionV1, d->para, d->length);
}

HPlane::Intercept *HPlane::intercept(LineSegment::Side const &lineSeg, int edge,
    EdgeTips const &edgeTips)
{
    // Already present for this vertex?
    Vertex &vertex = lineSeg.vertex(edge);
    if(d->haveInterceptForVertex(vertex)) return 0;

    coord_t distToVertex = intersect(lineSeg, edge);

    d->intercepts.append(Intercept(distToVertex, const_cast<LineSegment::Side &>(lineSeg), edge));
    Intercept *newIntercept = &d->intercepts.last();

    newIntercept->selfRef = (lineSeg.hasMapSide() && lineSeg.mapLine().isSelfReferencing());

    newIntercept->before = openSectorAtAngle(edgeTips, inverseAngle());
    newIntercept->after  = openSectorAtAngle(edgeTips, angle());

    // The addition of a new intercept means we'll need to resort.
    d->needSortIntercepts = true;

    return newIntercept;
}

/**
 * Merges @a next into @a cur.
 */
static void mergeIntercepts(HPlane::Intercept &cur, HPlane::Intercept const &next)
{
    /*
    LOG_AS("HPlane::mergeIntercepts");
    cur.debugPrint();
    next.debugPrint();
    */

    if(&cur.lineSegment().line() == &next.lineSegment().line())
        return;

    if(cur.selfRef && !next.selfRef)
    {
        if(cur.before && next.before)
            cur.before = next.before;

        if(cur.after && next.after)
            cur.after = next.after;

        cur.selfRef = false;
    }

    if(!cur.before && next.before)
        cur.before = next.before;

    if(!cur.after && next.after)
        cur.after = next.after;

    /*
    LOG_TRACE("Result:");
    cur.debugPrint();
    */
}

void HPlane::sortAndMergeIntercepts()
{
    // Any work to do?
    if(!d->needSortIntercepts) return;

    qSort(d->intercepts.begin(), d->intercepts.end());

    for(int i = 0; i < d->intercepts.count() - 1; ++i)
    {
        Intercept &cur  = d->intercepts[i];
        Intercept &next = d->intercepts[i+1];

        // Sanity check.
        ddouble distance = next.distance() - cur.distance();
        if(distance < -0.1)
        {
            throw Error("HPlane::sortAndMergeIntercepts",
                        String("Invalid intercept order - %1 > %2")
                            .arg(cur.distance(), 0, 'f', 3)
                            .arg(next.distance(), 0, 'f', 3));
        }

        // Are we merging this pair?
        if(distance <= HPLANE_INTERCEPT_MERGE_DISTANCE_EPSILON)
        {
            // Yes - merge the "next" intercept into "cur".
            mergeIntercepts(cur, next);

            // Destroy the "next" intercept.
            d->intercepts.removeAt(i+1);

            // Process the new "cur" and "next" pairing.
            i -= 1;
        }
    }

    d->needSortIntercepts = false;
}

Partition const &HPlane::partition() const
{
    return d->partition;
}

coord_t HPlane::angle() const
{
    return d->angle;
}

slopetype_t HPlane::slopeType() const
{
    return d->slopeType;
}

LineSegment::Side *HPlane::lineSegment() const
{
    return d->lineSegment;
}

void HPlane::distance(LineSegment::Side const &lineSeg, coord_t *fromDist, coord_t *toDist) const
{
    // Any work to do?
    if(!fromDist && !toDist) return;

    /// @attention Ensure line segments produced from the partition's source
    /// line are always treated as collinear. This special case is only
    /// necessary due to precision inaccuracies when a line is split into
    /// multiple segments.
    if(d->lineSegment != 0 &&
       &d->lineSegment->mapSide().line() == lineSeg.partitionMapLine())
    {
        if(fromDist) *fromDist = 0;
        if(toDist)   *toDist   = 0;
        return;
    }

    coord_t toSegDirectionV1[2] = { d->partition.direction.x, d->partition.direction.y } ;

    if(fromDist)
    {
        coord_t fromV1[2] = { lineSeg.from().origin().x, lineSeg.from().origin().y };
        *fromDist = V2d_PointLinePerpDistance(fromV1, toSegDirectionV1, d->perp, d->length);
    }
    if(toDist)
    {
        coord_t toV1[2] = { lineSeg.to().origin().x, lineSeg.to().origin().y };
        *toDist = V2d_PointLinePerpDistance(toV1, toSegDirectionV1, d->perp, d->length);
    }
}

LineRelationship HPlane::relationship(LineSegment::Side const &lineSeg,
    coord_t *retFromDist, coord_t *retToDist) const
{
    coord_t fromDist, toDist;
    distance(lineSeg, &fromDist, &toDist);

    LineRelationship rel = lineRelationship(fromDist, toDist);

    if(retFromDist) *retFromDist = fromDist;
    if(retToDist)   *retToDist   = toDist;

    return rel;
}

HPlane::Intercepts const &HPlane::intercepts() const
{
    return d->intercepts;
}

#ifdef DENG_DEBUG
void HPlane::printIntercepts() const
{
    uint index = 0;
    foreach(Intercept const &icpt, d->intercepts)
    {
        LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt.distance();
        icpt.debugPrint();
    }
}
#endif

} // namespace bsp
} // namespace de
