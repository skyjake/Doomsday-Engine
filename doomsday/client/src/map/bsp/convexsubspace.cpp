/** @file map/bsp/convexsubspace.cpp BSP Builder Convex Subspace.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <QHash>
#include <QtAlgorithms>

#include <de/mathutil.h>

#include "HEdge"
#include "Line"
#include "Polygon"
#include "Sector"
#include "map/bsp/linesegment.h"

#include "render/r_main.h" /// validCount @todo Remove me

#include "map/bsp/convexsubspace.h"

/// Smallest difference between two angles before being considered equal (in degrees).
static coord_t const ANG_EPSILON = 1.0 / 1024.0;

namespace de {
namespace bsp {

typedef LineSegment::Side Segment;
typedef QList<Segment *> SegmentList;

struct OrderedSegment
{
    Segment *segment;
    double fromAngle;
    double toAngle;
};

typedef QList<OrderedSegment> OrderedSegments;

/**
 * Represents a clockwise ordering of a subset of the line segments and
 * implements logic for partitioning that subset into @em contiguous ranges
 * for geometry construction.
 */
struct Continuity
{
    typedef QList<OrderedSegment *> OrderedSegmentList;

    /// Front sector uniformly referenced by all line segments.
    Sector *sector;

    /// Coverage metric.
    double coverage;

    /// Number of discordant (i.e., non-contiguous) line segments.
    int discordSegments;

    /// Number of referencing line segments of each type:
    int norm;
    int part;
    int self;

    /// The ordered line segments.
    OrderedSegmentList orderedSegs;

    Continuity(Sector *sector)
        : sector(sector), discordSegments(0), coverage(0)
    {}

    /**
     * Perform heuristic comparison between two continuities to determine a
     * preference order for BSP sector attribution. The algorithm used weights
     * the two choices according to the number and "type" of the referencing
     * line segments and the "coverage" metric.
     *
     * @return  @c true if "this" choice is rated better than @a other.
     *
     * @todo Remove when heuristic sector selection is no longer necessary.
     */
    bool operator < (Continuity const &other) const
    {
        if(norm == other.norm)
        {
            return !(coverage < other.coverage);
        }
        return norm > other.norm;
    }

    /**
     * Assumes that segments are added in clockwise order.
     */
    void addOneSegment(OrderedSegment const &oseg)
    {
        DENG_ASSERT(oseg.segment->sectorPtr() == sector);
        orderedSegs.append(const_cast<OrderedSegment *>(&oseg));

        // Account for the new line segment.
        Segment const &seg = *oseg.segment;
        if(!seg.hasMapSide())
        {
            part += 1;
        }
        else if(seg.mapSide().line().isSelfReferencing())
        {
            self += 1;
        }
        else
        {
            norm += 1;
        }
    }

    void evaluate()
    {
        // Calculate the 'coverage' metric.
        coverage = 0;
        foreach(OrderedSegment const *oseg, orderedSegs)
        {
            if(oseg->fromAngle > oseg->toAngle)
                coverage += oseg->fromAngle - oseg->toAngle;
            else
                coverage += oseg->fromAngle + (360.0 - oseg->toAngle);
        }

        // Account discontiguous segments.
        discordSegments = 0;
        for(int i = 0; i < orderedSegs.count() - 1; ++i)
        {
            Segment const &segA = *orderedSegs[i  ]->segment;
            Segment const &segB = *orderedSegs[i+1]->segment;

            if(segB.from().origin() != segA.to().origin())
                discordSegments += 1;
        }
        if(orderedSegs.count() > 1)
        {
            Segment const &segB = *orderedSegs.last()->segment;
            Segment const &segA = *orderedSegs.first()->segment;

            if(segB.to().origin() != segA.from().origin())
                discordSegments += 1;
        }
    }

#ifdef DENG_DEBUG
    void debugPrint() const
    {
        LOG_INFO("Continuity [%p] (sector:%i, coverage:%f, discord:%i)")
            << de::dintptr(this)
            << (sector? sector->indexInArchive() : -1)
            << coverage
            << discordSegments;

        foreach(OrderedSegment const *oseg, orderedSegs)
        {
            LOG_INFO("[%p] Angle: %1.6f %s -> Angle: %1.6f %s")
                << de::dintptr(oseg)
                << oseg->fromAngle
                << oseg->segment->from().origin().asText()
                << oseg->toAngle
                << oseg->segment->to().origin().asText();
        }
    }
#endif
};

DENG2_PIMPL_NOREF(ConvexSubspace)
{
    typedef QList<Continuity> Continuities;

    /// The set of line segments.
    Segments segments;

    /// Line segment continuity map.
    Continuities continuities;

    /// Set to @c true when the continuity map needs to be rebuilt.
    bool needRebuildContinuityMap;

    /// Chosen map sector for this subspace (if any).
    Sector *sector;

    /// BSP leaf attributed to the subspace (if any).
    BspLeaf *bspLeaf;

    Instance()
        : needRebuildContinuityMap(false),
          sector(0),
          bspLeaf(0)
    {}

    Instance(Instance const &other)
        : de::IPrivate(),
          segments                (other.segments),
          needRebuildContinuityMap(other.needRebuildContinuityMap),
          sector                  (other.sector),
          bspLeaf                 (other.bspLeaf)
    {}

    Vector2d findCenter()
    {
        Vector2d center;
        int numPoints = 0;
        foreach(Segment *seg, segments)
        {
            center += seg->from().origin();
            numPoints += 1;
        }
        if(numPoints)
        {
            center /= numPoints;
        }
        return center;
    }

    /**
     * Returns a list of all the line segments sorted in a clockwise order
     * (i.e., descending angles) according to their position/orientation
     * relative to @a point.
     */
    OrderedSegments orderedSegments(Vector2d const &point)
    {
        OrderedSegments clockwiseSegments;

        foreach(Segment *seg, segments)
        {
            Vector2d fromDist = seg->from().origin() - point;
            Vector2d toDist   = seg->to().origin() - point;

            OrderedSegment oseg;
            oseg.segment   = seg;
            oseg.fromAngle = M_DirectionToAngleXY(fromDist.x, fromDist.y);
            oseg.toAngle   = M_DirectionToAngleXY(toDist.x, toDist.y);

            clockwiseSegments.append(oseg);
        }

        // Sort the segments (algorithm: "double bubble").
        int const numSegments = clockwiseSegments.count();
        for(int pass = 0; pass < numSegments - 1; ++pass)
        {
            bool swappedAny = false;
            for(int i = 0; i < numSegments - 1; ++i)
            {
                OrderedSegment const &a = clockwiseSegments.at(i);
                OrderedSegment const &b = clockwiseSegments.at(i+1);

                if(a.fromAngle + ANG_EPSILON < b.fromAngle)
                {
                    clockwiseSegments.swap(i, i + 1);
                    swappedAny = true;
                }
            }
            if(!swappedAny) break;
        }

        // LOG_DEBUG("Ordered segments around %s") << point.asText();
        return clockwiseSegments;
    }

    void buildContinuityMap(Vector2d const &center, OrderedSegments const &clockwiseOrder)
    {
        needRebuildContinuityMap = false;

        typedef QHash<Sector *, Continuity *> SectorContinuityMap;
        SectorContinuityMap scMap;

        foreach(OrderedSegment const &oseg, clockwiseOrder)
        {
            Sector *frontSector = oseg.segment->sectorPtr();

            SectorContinuityMap::iterator found = scMap.find(frontSector);
            if(found == scMap.end())
            {
                continuities.append(Continuity(frontSector));
                found = scMap.insert(frontSector, &continuities.last());
            }

            Continuity *conty = found.value();
            conty->addOneSegment(oseg);
        }

        for(int i = 0; i < continuities.count(); ++i)
        {
            continuities[i].evaluate();
        }
    }

private:
    Instance &operator = (Instance const &); // no assignment
};

ConvexSubspace::ConvexSubspace()
    : d(new Instance())
{}

ConvexSubspace::ConvexSubspace(QList<LineSegment::Side *> const &segments)
    : d(new Instance())
{
    addSegments(segments);
}

ConvexSubspace::ConvexSubspace(ConvexSubspace const &other)
    : d(new Instance(*other.d))
{}

void ConvexSubspace::addSegments(QList<LineSegment::Side *> const &newSegments)
{
    int sizeBefore = d->segments.size();

    d->segments.unite(Segments::fromList(newSegments));

    if(d->segments.size() != sizeBefore)
    {
        // We'll need to rebuild the continuity map.
        d->needRebuildContinuityMap = true;
    }

#ifdef DENG_DEBUG
    int numSegmentsAdded = d->segments.size() - sizeBefore;
    if(numSegmentsAdded < newSegments.size())
    {
        LOG_DEBUG("ConvexSubspace pruned %i duplicate segments")
            << (newSegments.size() - numSegmentsAdded);
    }
#endif
}

void ConvexSubspace::addOneSegment(LineSegment::Side const &newSegment)
{
    int sizeBefore = d->segments.size();

    d->segments.insert(const_cast<LineSegment::Side *>(&newSegment));

    if(d->segments.size() != sizeBefore)
    {
        // We'll need to rebuild the continuity map.
        d->needRebuildContinuityMap = true;
    }
    else
    {
        LOG_DEBUG("ConvexSubspace pruned one duplicate segment");
    }
}

Polygon *ConvexSubspace::buildLeafGeometry() const
{
    if(isEmpty())
        return 0;

    Vector2d center = d->findCenter();
    OrderedSegments clockwiseSegments = d->orderedSegments(center);

    d->buildContinuityMap(center, clockwiseSegments);

    // Choose a sector to attribute to any BSP leaf we might produce.
    qSort(d->continuities.begin(), d->continuities.end());
    d->sector = d->continuities.first().sector;

#ifdef DENG_DEBUG
    LOG_INFO("\nConvexSubspace %s BSP sector:%i (%i continuities)")
        << center.asText()
        << (d->sector? d->sector->indexInArchive() : -1)
        << d->continuities.count();

    foreach(Continuity const &conty, d->continuities)
    {
        conty.debugPrint();
    }
#endif

    // Construct the polygon and ring of half-edges.
    Polygon *poly = new Polygon;

    // Iterate backwards so that the half-edges can be linked clockwise.
    for(int i = clockwiseSegments.size(); i-- > 0; )
    {
        Segment *seg = clockwiseSegments[i].segment;
        HEdge *hedge = new HEdge(seg->from(), seg->mapSidePtr());

        // Link the new half-edge for this line segment to the head of
        // the list in the new polygon geometry.
        hedge->_next = poly->_hedge;
        poly->_hedge = hedge;

        // There is now one more half-edge in this polygon.
        poly->_hedgeCount += 1;

        // Is there a half-edge on the back side we need to twin with?
        if(seg->back().hasHEdge())
        {
            seg->back().hedge()._twin = hedge;
            hedge->_twin = seg->back().hedgePtr();
        }

        // Link the new half-edge with this line segment.
        seg->setHEdge(hedge);
    }

    // Link the half-edges anticlockwise and close the ring.
    HEdge *hedge = poly->_hedge;
    forever
    {
        if(hedge->hasNext())
        {
            // Link anticlockwise.
            hedge->_next->_prev = hedge;
            hedge = hedge->_next;
        }
        else
        {
            // Circular link.
            hedge->_next = poly->_hedge;
            hedge->_next->_prev = hedge;
            break;
        }
    }

    /// @todo Polygon should encapsulate.
    poly->updateAABox();
    poly->updateCenter();

    return poly;
}

Sector *ConvexSubspace::chooseSectorForBspLeaf() const
{
    DENG_ASSERT(!d->needRebuildContinuityMap);
    return d->sector;
}

BspLeaf *ConvexSubspace::bspLeaf() const
{
    return d->bspLeaf;
}

void ConvexSubspace::setBspLeaf(BspLeaf *newBspLeaf)
{
    d->bspLeaf = newBspLeaf;
}

ConvexSubspace::Segments const &ConvexSubspace::segments() const
{
    return d->segments;
}

} // namespace bsp
} // namespace de
