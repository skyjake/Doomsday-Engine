/** @file hplane.cpp  World map BSP builder half-plane.
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

#include "de_platform.h"
#include "world/bsp/hplane.h"

#include "Line"
#include "Sector"
#include "Vertex"

#include "world/bsp/edgetip.h"
#include "world/bsp/linesegment.h"
#include "world/bsp/partitioner.h"

#include <de/Error>
#include <de/Log>
#include <de/vector1.h> // remove me
#include <de/mathutil.h> // M_InverseAngle
#include <QtAlgorithms>
#include <memory>

namespace de {
namespace bsp {

HPlane::Intercept::Intercept(ddouble distance, LineSegmentSide &lineSeg, int edge)
    : _before  (0)
    , _after   (0)
    , _distance(distance)
    , _lineSeg (&lineSeg)
    , _edge    (edge)
{}

LineSegmentSide &HPlane::Intercept::lineSegment() const
{
    return *_lineSeg;
}

int HPlane::Intercept::lineSegmentEdge() const
{
    return _edge;
}

Sector *HPlane::Intercept::before() const
{
    return _before? _before->sectorPtr() : 0;
}

Sector *HPlane::Intercept::after() const
{
    return _after? _after->sectorPtr() : 0;
}

LineSegmentSide *HPlane::Intercept::beforeLineSegment() const
{
    return _before;
}

LineSegmentSide *HPlane::Intercept::afterLineSegment() const
{
    return _after;
}

#ifdef DENG_DEBUG
void HPlane::Intercept::debugPrint() const
{
    LOGDEV_MAP_MSG("Vertex #%i %s beforeSector: #%d afterSector: #%d %s")
        << vertex().indexInMap()
        << vertex().origin().asText()
        << (_before && _before->hasSector()? _before->sector().indexInArchive() : -1)
        << (_after && _after->hasSector()? _after->sector().indexInArchive() : -1);
}
#endif

DENG2_PIMPL(HPlane)
{
    Partition partition;          ///< The partition line.
    coord_t length;               ///< Direction vector length.
    coord_t angle;                ///< Cartesian world angle.
    slopetype_t slopeType;        ///< Logical world angle classification.

    coord_t perp;                 ///< Perpendicular scale factor.
    coord_t para;                 ///< Parallel scale factor.

    LineSegmentSide *lineSegment; ///< Source of the partition (if any, not owned).

    Intercepts intercepts;        ///< Points along the half-plane.
    bool needSortIntercepts;      ///< @c true= @var intercepts requires sorting.

    Instance(Public *i, Partition const &partition)
        : Base(i)
        , partition  (partition)
        , length     (partition.direction.length())
        , angle      (M_DirectionToAngleXY(partition.direction.x, partition.direction.y))
        , slopeType  (M_SlopeTypeXY(partition.direction.x, partition.direction.y))
        , perp       (partition.origin.y * partition.direction.x - partition.origin.x * partition.direction.y)
        , para       (-partition.origin.x * partition.direction.x - partition.origin.y * partition.direction.y)
        , lineSegment(0)
        , needSortIntercepts(false)
    {}

    /**
     * Find an intercept by @a vertex.
     */
    Intercept *interceptByVertex(Vertex const &vertex)
    {
        foreach(Intercept const &icpt, intercepts)
        {
            if(&icpt.vertex() == &vertex)
                return const_cast<Intercept *>(&icpt);
        }
        return 0;
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

        if(cur.lineSegmentIsSelfReferencing() &&
           !next.lineSegmentIsSelfReferencing())
        {
            if(cur.before() && next.before())
                cur._before = next._before;

            if(cur.after() && next.after())
                cur._after = next._after;
        }

        if(!cur.before() && next.before())
            cur._before = next._before;

        if(!cur.after() && next.after())
            cur._after = next._after;

        /*
        LOG_TRACE("Result:");
        cur.debugPrint();
        */
    }
};

HPlane::HPlane(Partition const &partition) : d(new Instance(this, partition))
{}

void HPlane::clearIntercepts()
{
    d->intercepts.clear();
    // An empty intercept list is logically sorted.
    d->needSortIntercepts = false;
}

void HPlane::configure(LineSegmentSide const &newBaseSeg)
{
    // Only map line segments are suitable.
    DENG_ASSERT(newBaseSeg.hasMapSide());

    LOG_AS("HPlane::configure");

    // Clear the list of intersection points.
    clearIntercepts();

    // Reconfigure the partition line.
    LineSide &mapSide = newBaseSeg.mapSide();

    d->partition.direction = mapSide.to().origin() - mapSide.from().origin();
    d->partition.origin    = mapSide.from().origin();

    d->lineSegment = const_cast<LineSegmentSide *>(&newBaseSeg);

    d->length    = d->partition.direction.length();
    d->angle     = M_DirectionToAngleXY(d->partition.direction.x, d->partition.direction.y);
    d->slopeType = M_SlopeTypeXY(d->partition.direction.x, d->partition.direction.y);

    d->perp = d->partition.origin.y * d->partition.direction.x
            - d->partition.origin.x * d->partition.direction.y;

    d->para = -d->partition.origin.x * d->partition.direction.x
            -  d->partition.origin.y * d->partition.direction.y;

    //LOG_DEBUG("line segment %p %s")
    //    << &newBaseSeg << d->partition.asText();
}

/**
 * Determines whether a conceptual line oriented at @a vtx and "pointing"
 * at the specified world @a angle enters an "open" sector (which is to say
 * that said line does not enter void space and does not intercept with any
 * existing map or partition line segment in the plane, thus "closed").
 *
 * @return  The "open" sector at this angle; otherwise @c 0 (closed).
 */
static LineSegmentSide *lineSegAtAngle(EdgeTips const &tips, coord_t angle)
{
    // Is there a tip exactly at this angle?
    if(tips.at(angle))
        return 0; // Closed.

    // Find the first tip after (larger) than this angle. If present the side
    // we're interested in is the front.
    if(EdgeTip const *tip = tips.after(angle))
    {
        return tip->hasFront()? &tip->front() : 0;
    }

    // The open sector must therefore be on the back of the tip with the largest
    // angle (if present).
    if(EdgeTip const *tip = tips.largest())
    {
        return tip->hasBack()? &tip->back() : 0;
    }

    return 0; // No edge tips.
}

double HPlane::intersect(LineSegmentSide const &lineSeg, int edge)
{
    Vertex &vertex = lineSeg.vertex(edge);
    coord_t pointV1[2] = { vertex.origin().x, vertex.origin().y };
    coord_t directionV1[2] = { d->partition.direction.x, d->partition.direction.y };
    return V2d_PointLineParaDistance(pointV1, directionV1, d->para, d->length);
}

HPlane::Intercept *HPlane::intercept(LineSegmentSide const &lineSeg, int edge,
    EdgeTips const &edgeTips)
{
    bool selfRef = (lineSeg.hasMapSide() && lineSeg.mapLine().isSelfReferencing());

    // Already present for this vertex?
    Intercept *icpt;
    if((icpt = d->interceptByVertex(lineSeg.vertex(edge))))
    {
        // If the new intercept line is not self-referencing we'll replace it.
        if(!(icpt->lineSegmentIsSelfReferencing() && !selfRef))
        {
            return icpt;
        }
    }
    else
    {
        d->intercepts.append(
            Intercept(intersect(lineSeg, edge),
                      const_cast<LineSegmentSide &>(lineSeg), edge));
        icpt = &d->intercepts.last();

        // The addition of a new intercept means we'll need to resort.
        d->needSortIntercepts = true;
    }

    icpt->_lineSeg = const_cast<LineSegmentSide *>(&lineSeg);
    icpt->_edge    = edge;
    icpt->_before  = lineSegAtAngle(edgeTips, inverseAngle());
    icpt->_after   = lineSegAtAngle(edgeTips, angle());

    return icpt;
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
            d->mergeIntercepts(cur, next);

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

coord_t HPlane::inverseAngle() const
{
    return M_InverseAngle(angle());
}

slopetype_t HPlane::slopeType() const
{
    return d->slopeType;
}

LineSegmentSide *HPlane::lineSegment() const
{
    return d->lineSegment;
}

void HPlane::distance(LineSegmentSide const &lineSeg, coord_t *fromDist, coord_t *toDist) const
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

LineRelationship HPlane::relationship(LineSegmentSide const &lineSeg,
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
        LOG_DEBUG(" %u: >%1.2f") << (index++) << icpt.distance();
        icpt.debugPrint();
    }
}
#endif

} // namespace bsp
} // namespace de
