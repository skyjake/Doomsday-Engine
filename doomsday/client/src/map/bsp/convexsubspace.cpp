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
#include <QList>
#include <QtAlgorithms>

#include "Line"
#include "Sector"
#include "map/bsp/linesegment.h"

#include "render/r_main.h" /// validCount @todo Remove me

#include "map/bsp/convexsubspace.h"

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
    /// The set of line segments.
    Segments segments;

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
                found = candidates.insert(sector, SectorCandidate(*cand));
            }

            // Account for a new segment referencing this sector.
            found.value().account(*seg);
        }

        if(candidates.isEmpty()) return; // Eeek!

        // Sort our candidates such that our preferred sector appears first.
        // This shouldn't take too long, typically there is no more than two or
        // three to choose from.
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
