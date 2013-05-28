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

/**
 * Represents a candidate sector for BSP leaf attribution.
 */
struct SectorCandidate
{
    /// The sector choice.
    Sector *sector;

    /// Number of referencing line segments of each type:
    int norm;
    int part;
    int self;

    SectorCandidate(Sector &sector)
        : sector(&sector), norm(0), part(0), self(0)
    {}

    /**
     * Perform heuristic comparison between two choices to determine a
     * preference order. The algorithm used weights the two choices according
     * to the number and "type" of the referencing line segments.
     *
     * @return  @c true if "this" choice is rated better than @a other.
     */
    bool operator < (SectorCandidate const &other) const
    {
        if(norm == other.norm)
        {
            if(part == other.part)
            {
                if(self == other.self)
                {
                    // All equal; use the unique indices to stablize.
                    return sector->indexInMap() < other.sector->indexInMap();
                }
                return self > other.self;
            }
            return part > other.part;
        }
        return norm > other.norm;
    }

    /**
     * Account for a new line segment which references this choice.
     * Consider collinear segments only once.
     */
    void account(LineSegment::Side &seg)
    {
        // Determine the type of reference and increment the count.
        if(!seg.hasMapSide())
        {
            Line *mapLine = seg.partitionMapLine();
            if(mapLine && mapLine->validCount() == validCount)
                return;

            part += 1;

            if(mapLine)
                mapLine->setValidCount(validCount);
        }
        else
        {
            if(seg.mapLine().validCount() == validCount)
                return;

            if(seg.mapLine().isSelfReferencing())
            {
                self += 1;
            }
            else
            {
                norm += 1;
            }

            seg.mapLine().setValidCount(validCount);
        }
    }
};
typedef QHash<Sector *, SectorCandidate> SectorCandidateHash;

DENG2_PIMPL_NOREF(ConvexSubspace)
{
    typedef QList<LineSegment::Side *> SegmentList;

    /// The set of line segments.
    Segments segments;

    /// A clockwise ordering of the line segments.
    SegmentList clockwiseSegments;

    /// Chosen map sector for this subspace (if any).
    Sector *sector;

    /// Set to @c true when should to rethink our chosen sector.
    bool needChooseSector;

    /// BSP leaf attributed to the subspace (if any).
    BspLeaf *bspLeaf;

    Instance()
        : sector(0),
          needChooseSector(true),
          bspLeaf(0)
    {}

    Instance(Instance const &other)
        : de::IPrivate(),
          segments        (other.segments),
          sector          (other.sector),
          needChooseSector(other.needChooseSector),
          bspLeaf         (other.bspLeaf)
    {}

    Vector2d findCenter()
    {
        Vector2d center;
        int numPoints = 0;
        foreach(LineSegment::Side *segment, segments)
        {
            center += segment->from().origin();
            numPoints += 1;
        }
        if(numPoints)
        {
            center /= numPoints;
        }
        return center;
    }

    /**
     * Sort the line segments in a clockwise order (i.e., descending angles)
     * according to their position/orientation relative to the center of the
     * subspace.
     */
    void orderSegments()
    {
        clockwiseSegments = SegmentList::fromSet(segments);

        // Any work to do?
        if(clockwiseSegments.isEmpty())
            return;

        // Sort the segments by angle (from the middle point to the start vertex).
        Vector2d center = findCenter();

        /// "double bubble"
        int const numSegments = clockwiseSegments.count();
        for(int pass = 0; pass < numSegments - 1; ++pass)
        {
            bool swappedAny = false;
            for(int i = 0; i < numSegments - 1; ++i)
            {
                LineSegment::Side const *a = clockwiseSegments.at(i);
                LineSegment::Side const *b = clockwiseSegments.at(i+1);

                Vector2d v1Dist = a->from().origin() - center;
                Vector2d v2Dist = b->from().origin() - center;

                coord_t v1Angle = M_DirectionToAngleXY(v1Dist.x, v1Dist.y);
                coord_t v2Angle = M_DirectionToAngleXY(v2Dist.x, v2Dist.y);

                if(v1Angle + ANG_EPSILON < v2Angle)
                {
                    clockwiseSegments.swap(i, i + 1);
                    swappedAny = true;
                }
            }
            if(!swappedAny) break;
        }

        // LOG_DEBUG("Sorted segments around %s") << center.asText();
    }

    void chooseSector()
    {
        needChooseSector = false;
        sector = 0;

        // No candidates?
        if(segments.isEmpty()) return;

        // Only one candidate?
        if(segments.count() == 1)
        {
            // Lets hope its a good one...
            sector = (*segments.constBegin())->sectorPtr();
            return;
        }

        /*
         * Multiple candidates.
         * We will consider collinear segments only once.
         */
        validCount++;

        SectorCandidateHash candidates;
        foreach(LineSegment::Side *seg, segments)
        {
            // Segments with no sector can't help us.
            if(!seg->hasSector()) continue;

            Sector *cand = seg->sectorPtr();

            // Is this a new candidate?
            SectorCandidateHash::iterator found = candidates.find(cand);
            if(found == candidates.end())
            {
                // Yes, record it.
                found = candidates.insert(cand, SectorCandidate(*cand));
            }

            // Account for a new segment referencing this sector.
            found.value().account(*seg);
        }

        if(candidates.isEmpty()) return; // Eeek!

        // Sort our candidates such that our preferred sector appears first.
        // This shouldn't take too long, typically there are no more than two
        // or three to choose from.
        QList<SectorCandidate> sortedCandidates = candidates.values();
        qSort(sortedCandidates.begin(), sortedCandidates.end());

        // We'll choose the highest rated candidate.
        sector = sortedCandidates.first().sector;
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
        // We'll need to rethink our sector choice.
        d->needChooseSector = true;
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
        // We'll need to rethink our sector choice.
        d->needChooseSector = true;
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

    d->orderSegments();

    // Construct the polygon and ring of half-edges.
    Polygon *poly = new Polygon;

    // Iterate backwards so that the half-edges can be linked clockwise.
    for(int i = d->clockwiseSegments.count(); i-- > 0; )
    {
        LineSegment::Side *seg = d->clockwiseSegments[i];
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
    // Do we need to rethink our choice?
    if(d->needChooseSector)
    {
        d->chooseSector();
    }
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
