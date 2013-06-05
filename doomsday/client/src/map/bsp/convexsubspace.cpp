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
#include <QSet>
#include <QtAlgorithms>

#include <de/binangle.h>
#include <de/mathutil.h>

#include "BspLeaf"
#include "HEdge"
#include "Line"
#include "Mesh"
#include "Sector"
#include "Segment"
#include "map/bsp/linesegment.h"

#include "render/r_main.h" /// validCount @todo Remove me

#include "map/bsp/convexsubspace.h"

/// Smallest difference between two angles before being considered equal (in degrees).
static coord_t const ANG_EPSILON = 1.0 / 1024.0;

namespace de {
namespace bsp {

typedef QList<LineSegment::Side *> SegmentList;

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

    /// The discordant line segments.
    OrderedSegmentList discordSegs;

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

        // Separate the discordant duplicates.
        OrderedSegmentList *list = &orderedSegs;
        foreach(OrderedSegment const *other, orderedSegs)
        {
            if(oseg == *other)
            {
                list = &discordSegs;
                break;
            }
        }

        list->append(const_cast<OrderedSegment *>(&oseg));

        // Account for the new line segment.
        LineSegment::Side const &seg = *oseg.segment;
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

        // Update the 'coverage' metric.
        if(oseg.fromAngle > oseg.toAngle)
            coverage += oseg.fromAngle - oseg.toAngle;
        else
            coverage += oseg.fromAngle + (360.0 - oseg.toAngle);
    }

    void evaluate()
    {
        // Account remaining discontiguous segments.
        discordSegments = 0;
        for(int i = 0; i < orderedSegs.count() - 1; ++i)
        {
            LineSegment::Side const &segA = *orderedSegs[i  ]->segment;
            LineSegment::Side const &segB = *orderedSegs[i+1]->segment;

            if(segB.from().origin() != segA.to().origin())
                discordSegments += 1;
        }
        if(orderedSegs.count() > 1)
        {
            LineSegment::Side const &segB = *orderedSegs.last()->segment;
            LineSegment::Side const &segA = *orderedSegs.first()->segment;

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
            oseg->debugPrint();
        }
        foreach(OrderedSegment const *oseg, discordSegs)
        {
            oseg->debugPrint();
        }
    }
#endif
};

DENG2_PIMPL_NOREF(ConvexSubspace)
{
    typedef QSet<LineSegment::Side *> Segments;
    typedef QList<Continuity> Continuities;

    /// The set of line segments.
    Segments segments;

    /// The same line segments in a clockwise order with angle info.
    OrderedSegments orderedSegments;

    /// Set to @c true when the ordered segment list needs to be rebuilt.
    bool needRebuildOrderedSegments;

    /// Line segment continuity map.
    Continuities continuities;

    /// BSP leaf attributed to the subspace (if any).
    BspLeaf *bspLeaf;

    Instance()
        : needRebuildOrderedSegments(false),
          bspLeaf(0)
    {}

    Instance(Instance const &other)
        : de::IPrivate(),
          segments                  (other.segments),
          orderedSegments           (other.orderedSegments),
          needRebuildOrderedSegments(other.needRebuildOrderedSegments),
          bspLeaf                   (other.bspLeaf)
    {}

    /**
     * Returns @c true iff at least one line segment in the set is derived
     * from a map line.
     */
    bool haveMapLineSegment()
    {
        foreach(LineSegment::Side *seg, segments)
        {
            if(seg->hasMapSide())
                return true;
        }
        return false;
    }

    Vector2d findCenter()
    {
        Vector2d center;
        int numPoints = 0;
        foreach(LineSegment::Side *seg, segments)
        {
            center += seg->from().origin();
            center += seg->to().origin();
            numPoints += 2;
        }
        if(numPoints)
        {
            center /= numPoints;
        }
        return center;
    }

    /**
     * Builds the ordered list of line segments, which, is sorted firstly in
     * a clockwise order (i.e., descending angles) according to the origin of
     * their 'from' vertex relative to @a point. A secondary ordering is also
     * applied such that line segments with the same origin coordinates are
     * sorted by descending 'to' angle.
     */
    void buildOrderedSegments(Vector2d const &point)
    {
        needRebuildOrderedSegments = false;

        orderedSegments.clear();

        foreach(LineSegment::Side *seg, segments)
        {
            Vector2d fromDist = seg->from().origin() - point;
            Vector2d toDist   = seg->to().origin() - point;

            OrderedSegment oseg;
            oseg.segment   = seg;
            oseg.fromAngle = M_DirectionToAngleXY(fromDist.x, fromDist.y);
            oseg.toAngle   = M_DirectionToAngleXY(toDist.x, toDist.y);

            orderedSegments.append(oseg);
        }

        // Sort algorithm: "double bubble".

        // Order by descending 'from' angle.
        int const numSegments = orderedSegments.count();
        for(int pass = 0; pass < numSegments - 1; ++pass)
        {
            bool swappedAny = false;
            for(int i = 0; i < numSegments - 1; ++i)
            {
                OrderedSegment const &a = orderedSegments.at(i);
                OrderedSegment const &b = orderedSegments.at(i+1);
                if(a.fromAngle + ANG_EPSILON < b.fromAngle)
                {
                    orderedSegments.swap(i, i + 1);
                    swappedAny = true;
                }
            }
            if(!swappedAny) break;
        }

        for(int pass = 0; pass < numSegments - 1; ++pass)
        {
            bool swappedAny = false;
            for(int i = 0; i < numSegments - 1; ++i)
            {
                OrderedSegment const &a = orderedSegments.at(i);
                OrderedSegment const &b = orderedSegments.at(i+1);
                if(a.fromAngle == b.fromAngle)
                {
                    if(b.segment->length() > a.segment->length())
                    {
                        orderedSegments.swap(i, i + 1);
                        swappedAny = true;
                    }
                }
            }
            if(!swappedAny) break;
        }

        // LOG_DEBUG("Ordered segments around %s") << point.asText();
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

    d->segments.unite(QSet<LineSegment::Side *>::fromList(newSegments));

    if(d->segments.size() != sizeBefore)
    {
        // We'll need to rebuild the ordered segment list.
        d->needRebuildOrderedSegments = true;
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
        // We'll need to rebuild the ordered segment list.
        d->needRebuildOrderedSegments = true;
    }
    else
    {
        LOG_DEBUG("ConvexSubspace pruned one duplicate segment");
    }
}

void ConvexSubspace::buildGeometry(BspLeaf &leaf) const
{
    LOG_AS("ConvexSubspace::buildGeometry");
    bool const isDegenerate = segmentCount() < 3;

    // Sanity check.
    if(!isDegenerate && !d->haveMapLineSegment())
        throw Error("ConvexSubspace::buildGeometry", "No map line segment");

    if(d->needRebuildOrderedSegments)
    {
        d->buildOrderedSegments(d->findCenter());
    }

    /*
     * Build the continuity map.
     */
    d->continuities.clear();

    typedef QHash<Sector *, Continuity *> SectorContinuityMap;
    SectorContinuityMap scMap;

    foreach(OrderedSegment const &oseg, d->orderedSegments)
    {
        Sector *frontSector = oseg.segment->sectorPtr();

        SectorContinuityMap::iterator found = scMap.find(frontSector);
        if(found == scMap.end())
        {
            d->continuities.append(Continuity(frontSector));
            found = scMap.insert(frontSector, &d->continuities.last());
        }

        Continuity *conty = found.value();
        conty->addOneSegment(oseg);
    }

    for(int i = 0; i < d->continuities.count(); ++i)
    {
        Continuity &conty = d->continuities[i];

        conty.evaluate();

        if(!conty.discordSegs.isEmpty())
        {
            // Construct a new mesh and set of half-edges.
            Mesh *mesh = new Mesh;
            Face *face = mesh->newFace();

            foreach(OrderedSegment const *oseg, conty.discordSegs)
            {
                LineSegment::Side *lineSeg = oseg->segment;
                Line::Side *mapSide = lineSeg->mapSidePtr();
                HEdge *hedge = mesh->newHEdge(lineSeg->from());

                // Ownership of the segment will be assigned to the space partitioner.
                Segment *seg = new Segment(mapSide, hedge);

                // Attribute the half-edge to the segment.
                hedge->setMapElement(seg);

                if(mapSide)
                {
                    seg->setLineSideOffset(Vector2d(mapSide->from().origin() - lineSeg->from().origin()).length());
                }

                seg->setLength(Vector2d(lineSeg->to().origin() - lineSeg->from().origin()).length());

                seg->setAngle(bamsAtan2(int( lineSeg->to().origin().y - lineSeg->from().origin().y ),
                                        int( lineSeg->to().origin().x - lineSeg->from().origin().x )) << FRACBITS);

                // Link the new half-edge for this line segment to the head of
                // the list in the new face geometry.
                hedge->setNext(face->hedge());
                face->setHEdge(hedge);

                // Is there a half-edge on the back side we need to twin with?
                if(lineSeg->back().hasSegment())
                {
                    lineSeg->back().segment().hedge().setTwin(hedge);
                    hedge->setTwin(&lineSeg->back().segment().hedge());

                    seg->setBack(&lineSeg->back().segment());
                    seg->back().setBack(seg);
                }

                // Link the new segment with the line segment.
                lineSeg->setSegment(seg);
            }

            // Link the half-edges anticlockwise and close the ring.
            HEdge *hedge = face->hedge();
            forever
            {
                // There is now one more half-edge in this face.
                /// @todo Face should encapsulate.
                face->_hedgeCount += 1;

                // Attribute the half-edge to the Face.
                hedge->setFace(face);

                if(hedge->hasNext())
                {
                    // Link anticlockwise.
                    hedge->next().setPrev(hedge);
                    hedge = &hedge->next();
                }
                else
                {
                    // Circular link.
                    hedge->setNext(face->hedge());
                    hedge->next().setPrev(hedge);
                    break;
                }
            }

            /// @todo Face should encapsulate.
            face->updateAABox();
            face->updateCenter();

            // Assign the mesh to the BSP leaf (takes ownership).
            leaf.assignExtraPoly(mesh);
        }
    }

    // Determine which sector to attribute to the BSP leaf.
    qSort(d->continuities.begin(), d->continuities.end());
    leaf.setSector(d->continuities.first().sector);

/*#ifdef DENG_DEBUG
    LOG_INFO("\nConvexSubspace %s BSP sector:%i (%i continuities)")
        << d->findCenter().asText()
        << (leaf.hasSector()? leaf.sector().indexInArchive() : -1)
        << d->continuities.count();

    foreach(Continuity const &conty, d->continuities)
    {
        conty.debugPrint();
    }
#endif*/

    if(!isDegenerate)
    {
        // Construct a new mesh and ring of half-edges.
        Mesh *mesh = new Mesh;
        Face *face = mesh->newFace();

        // Iterate backwards so that the half-edges can be linked clockwise.
        for(int i = d->orderedSegments.size(); i-- > 0; )
        {
            LineSegment::Side *lineSeg = d->orderedSegments[i].segment;

            if(lineSeg->hasSegment())
                continue;

            Line::Side *mapSide = lineSeg->mapSidePtr();
            HEdge *hedge = mesh->newHEdge(lineSeg->from());

            // Ownership of the segment will be assigned to the space partitioner.
            Segment *seg = new Segment(mapSide, hedge);

            // Attribute the half-edge to the segment.
            hedge->setMapElement(seg);

            if(mapSide)
            {
                seg->setLineSideOffset(Vector2d(mapSide->from().origin() - lineSeg->from().origin()).length());
            }

            seg->setLength(Vector2d(lineSeg->to().origin() - lineSeg->from().origin()).length());

            seg->setAngle(bamsAtan2(int( lineSeg->to().origin().y - lineSeg->from().origin().y ),
                                    int( lineSeg->to().origin().x - lineSeg->from().origin().x )) << FRACBITS);

            // Link the new half-edge for this line segment to the head of
            // the list in the new Face geometry.
            hedge->setNext(face->hedge());
            face->setHEdge(hedge);

            // Is there a half-edge on the back side we need to twin with?
            if(lineSeg->back().hasSegment())
            {
                lineSeg->back().segment().hedge().setTwin(hedge);
                hedge->setTwin(&lineSeg->back().segment().hedge());

                seg->setBack(&lineSeg->back().segment());
                seg->back().setBack(seg);
            }

            // Link the new segment with the line segment.
            lineSeg->setSegment(seg);
        }

        // Link the half-edges anticlockwise and close the ring.
        HEdge *hedge = face->hedge();
        forever
        {
            // There is now one more half-edge in this face.
            /// @todo Face should encapsulate.
            face->_hedgeCount += 1;

            // Attribute the half-edge to the Face.
            hedge->setFace(face);

            if(hedge->hasNext())
            {
                // Link anticlockwise.
                hedge->next().setPrev(hedge);
                hedge = &hedge->next();
            }
            else
            {
                // Circular link.
                hedge->setNext(face->hedge());
                hedge->next().setPrev(hedge);
                break;
            }
        }

        /// @todo Face should encapsulate.
        face->updateAABox();
        face->updateCenter();

        // Assign the mesh to the BSP leaf (takes ownership).
        leaf.assignPoly(mesh);
    }

    if(!leaf.hasSector())
    {
        LOG_WARNING("BspLeaf %p is degenerate/orphan (%d half-edges).")
            << de::dintptr(&leaf)
            << (leaf.hasPoly()? leaf.poly().firstFace()->hedgeCount() : 0);
    }
}

int ConvexSubspace::segmentCount() const
{
    return d->segments.count();
}

OrderedSegments const &ConvexSubspace::segments() const
{
    if(d->needRebuildOrderedSegments)
    {
        d->buildOrderedSegments(d->findCenter());
    }
    return d->orderedSegments;
}

BspLeaf *ConvexSubspace::bspLeaf() const
{
    return d->bspLeaf;
}

void ConvexSubspace::setBspLeaf(BspLeaf *newBspLeaf)
{
    d->bspLeaf = newBspLeaf;
}

} // namespace bsp
} // namespace de
