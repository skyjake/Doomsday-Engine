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

HPlane::Intercept::Intercept(ddouble distance, LineSegment &lineSeg, int edge)
    : selfRef(false),
      before(0),
      after(0),
      _distance(distance),
      _lineSeg(&lineSeg),
      _edge(edge)
{}

LineSegment &HPlane::Intercept::lineSegment() const
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

    /// Map line segment which is the basis for the half-plane.
    std::auto_ptr<LineSegment> lineSegment;

    /// Intercept points along the half-plane.
    Intercepts intercepts;

    /// Set to @c true when @var intercepts requires sorting.
    bool needSortIntercepts;

    Instance(Public *i, Vector2d const &partitionOrigin,
             Vector2d const &partitionDirection)
        : Base(i),
          partition(partitionOrigin, partitionDirection),
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

void HPlane::configure(LineSegment const &newBaseSeg)
{
    // Only map line segments are suitable.
    DENG_ASSERT(newBaseSeg.hasMapSide());

    LOG_AS("HPlane::configure");

    // Clear the list of intersection points.
    clearIntercepts();

    Line::Side &side = newBaseSeg.mapSide();
    d->partition.origin    = side.from().origin();
    d->partition.direction = side.to().origin() - side.from().origin();

    // Update/store a copy of the line segment.
    d->lineSegment.reset(new LineSegment(newBaseSeg));
    d->lineSegment->ceaseVertexObservation(); /// @todo refactor away -ds

    //LOG_DEBUG("line segment %p %s.")
    //    << de::dintptr(&newLineSeg) << d->partition.asText();
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

HPlane::Intercept *HPlane::intercept(LineSegment const &lineSeg, int edge,
    EdgeTips const &edgeTips)
{
    // Already present for this vertex?
    Vertex &vertex = lineSeg.vertex(edge);
    if(d->haveInterceptForVertex(vertex)) return 0;

    d->intercepts.append(Intercept(d->lineSegment->distance(vertex.origin()),
                                   const_cast<LineSegment &>(lineSeg), edge));
    Intercept *newIntercept = &d->intercepts.last();

    newIntercept->selfRef = (lineSeg.hasMapSide() && lineSeg.line().isSelfReferencing());

    newIntercept->before = openSectorAtAngle(edgeTips, d->lineSegment->inverseAngle());
    newIntercept->after  = openSectorAtAngle(edgeTips, d->lineSegment->angle());

    // The addition of a new intercept means we'll need to resort.
    d->needSortIntercepts = true;

    return newIntercept;
}

static void mergeIntercepts(HPlane::Intercept &final,
                            HPlane::Intercept const &other)
{
    /*
    LOG_AS("HPlane::mergeIntercepts");
    final.debugPrint();
    other.debugPrint();
    */

    if(final.selfRef && !other.selfRef)
    {
        if(final.before && other.before)
            final.before = other.before;

        if(final.after && other.after)
            final.after = other.after;

        final.selfRef = false;
    }

    if(!final.before && other.before)
        final.before = other.before;

    if(!final.after && other.after)
        final.after = other.after;

    /*
    LOG_TRACE("Result:");
    final.debugPrint();
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

Partition const &HPlane::partition() const
{
    return d->partition;
}

LineSegment const &HPlane::lineSegment() const
{
    DENG_ASSERT(d->lineSegment.get() != 0);
    return *d->lineSegment.get();
}

HPlane::Intercepts const &HPlane::intercepts() const
{
    return d->intercepts;
}

} // namespace bsp
} // namespace de
