/** @file map/sectionedge.cpp World Map (Wall) Section Edge.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include <QtAlgorithms>

#include "Sector"

#include "map/r_world.h" // R_SideSectionCoords, R_GetVtxLineOwner
#include "map/lineowner.h"

#include "map/sectionedge.h"

using namespace de;

SectionEdge::Intercept::Intercept(SectionEdge *owner, double distance)
    : IHPlane::IIntercept(distance),
      _owner(owner)
{}

SectionEdge::Intercept::Intercept(Intercept const &other)
    : IHPlane::IIntercept(other.distance()),
      _owner(other._owner) /// @todo Should not copy owner
{}

SectionEdge &SectionEdge::Intercept::owner() const
{
    return *_owner;
}

Vector3d SectionEdge::Intercept::origin() const
{
    return Vector3d(owner().origin(), distance());
}

DENG2_PIMPL(SectionEdge), public IHPlane
{
    Line::Side *lineSide;
    int section;
    ClockDirection neighborScanDirection;
    coord_t lineOffset;
    Vertex *lineVertex;
    Sector *frontSec;
    Sector *backSec;

    bool isValid;
    Vector2f materialOrigin;

    /// Data for the IHPlane model.
    struct HPlane
    {
        /// The partition line.
        Partition partition;

        /// Intercept points along the half-plane.
        SectionEdge::Intercepts intercepts;

        /// Set to @c true when @var intercepts requires sorting.
        bool needSortIntercepts;

        HPlane() : needSortIntercepts(false) {}

        HPlane(HPlane const & other)
            : partition         (other.partition),
              intercepts        (other.intercepts),
              needSortIntercepts(other.needSortIntercepts)
        {}
    } hplane;

    Instance(Public *i, Line::Side *lineSide, int section,
             ClockDirection neighborScanDirection, coord_t lineOffset,
             Vertex *lineVertex, Sector *frontSec, Sector *backSec)
        : Base(i),
          lineSide(lineSide),
          section(section),
          neighborScanDirection(neighborScanDirection),
          lineOffset(lineOffset),
          lineVertex(lineVertex),
          frontSec(frontSec),
          backSec(backSec),
          isValid(false)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i),
          lineSide             (other.lineSide),
          section              (other.section),
          neighborScanDirection(other.neighborScanDirection),
          lineOffset           (other.lineOffset),
          lineVertex           (other.lineVertex),
          frontSec             (other.frontSec),
          backSec              (other.backSec),
          isValid              (other.isValid),
          materialOrigin       (other.materialOrigin),
          hplane               (other.hplane)
    {}

    void verifyValid() const
    {
        if(!isValid)
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw SectionEdge::InvalidError("SectionEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    // Implements IHPlane
    void configure(Partition const &newPartition)
    {
        clearIntercepts();
        hplane.partition = newPartition;
    }

    // Implements IHPlane
    Partition const &partition() const
    {
        return hplane.partition;
    }

    SectionEdge::Intercept *find(double distance)
    {
        for(int i = 0; i < hplane.intercepts.count(); ++i)
        {
            SectionEdge::Intercept &icpt = hplane.intercepts[i];
            if(de::fequal(icpt.distance(), distance))
                return &icpt;
        }
        return 0;
    }

    // Implements IHPlane
    SectionEdge::Intercept const *intercept(double distance)
    {
        if(find(distance))
            return 0;

        hplane.intercepts.append(SectionEdge::Intercept(&self, distance));
        SectionEdge::Intercept *newIntercept = &hplane.intercepts.last();

        // The addition of a new intercept means we'll need to resort.
        hplane.needSortIntercepts = true;
        return newIntercept;
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        // Any work to do?
        if(!hplane.needSortIntercepts) return;

        qSort(hplane.intercepts.begin(), hplane.intercepts.end());

        hplane.needSortIntercepts = false;
    }

    // Implements IHPlane
    void clearIntercepts()
    {
        hplane.intercepts.clear();
        // An empty intercept list is logically sorted.
        hplane.needSortIntercepts = false;
    }

    // Implements IHPlane
    SectionEdge::Intercept const &at(int index) const
    {
        if(index >= 0 && index < interceptCount())
        {
            return hplane.intercepts[index];
        }
        /// @throw IHPlane::UnknownInterceptError The specified intercept index is not valid.
        throw IHPlane::UnknownInterceptError("HPlane2::at", QString("Index '%1' does not map to a known intercept (count: %2)")
                                                                .arg(index).arg(interceptCount()));
    }

    // Implements IHPlane
    int interceptCount() const
    {
        return hplane.intercepts.count();
    }

#ifdef DENG_DEBUG
    void printIntercepts() const
    {
        uint index = 0;
        foreach(SectionEdge::Intercept const &icpt, hplane.intercepts)
        {
            LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt.distance();
        }
    }
#endif

    /**
     * Ensure all intercepts do not exceed the specified range.
     */
    void assertInterceptsInRange(coord_t low, coord_t hi) const
    {
#ifdef DENG_DEBUG
        foreach(SectionEdge::Intercept const &icpt, hplane.intercepts)
        {
            DENG2_ASSERT(icpt.distance() >= low && icpt.distance() <= hi);
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    void addPlaneIntercepts(coord_t bottom, coord_t top)
    {
        DENG_ASSERT(top > bottom);

        // Retrieve the start owner node.
        LineOwner *base = R_GetVtxLineOwner(lineVertex, &lineSide->line());
        if(!base) return;

        // Check for neighborhood division?
        if(section == Line::Side::Middle && lineSide->back().hasSections())
            return;

        Sector const *frontSec = lineSide->sectorPtr();
        LineOwner *own = base;
        bool stopScan = false;
        do
        {
            own = &own->navigate(neighborScanDirection);

            if(own == base)
            {
                stopScan = true;
            }
            else
            {
                Line *iter = &own->line();

                if(iter->isSelfReferencing())
                    continue;

                uint i = 0;
                do
                {
                    // First front, then back.
                    Sector *scanSec = 0;
                    if(!i && iter->hasFrontSections() && iter->frontSectorPtr() != frontSec)
                        scanSec = iter->frontSectorPtr();
                    else if(i && iter->hasBackSections() && iter->backSectorPtr() != frontSec)
                        scanSec = iter->backSectorPtr();

                    if(scanSec)
                    {
                        if(scanSec->ceiling().visHeight() - scanSec->floor().visHeight() > 0)
                        {
                            for(int j = 0; j < scanSec->planeCount() && !stopScan; ++j)
                            {
                                Plane const &plane = scanSec->plane(j);

                                if(plane.visHeight() > bottom && plane.visHeight() < top)
                                {
                                    if(intercept(plane.visHeight()))
                                    {
                                        // Have we reached the div limit?
                                        if(interceptCount() == SECTIONEDGE_MAX_INTERCEPTS)
                                            stopScan = true;
                                    }
                                }

                                if(!stopScan)
                                {
                                    // Clip a range bound to this height?
                                    if(plane.type() == Plane::Floor && plane.visHeight() > bottom)
                                        bottom = plane.visHeight();
                                    else if(plane.type() == Plane::Ceiling && plane.visHeight() < top)
                                        top = plane.visHeight();

                                    // All clipped away?
                                    if(bottom >= top)
                                        stopScan = true;
                                }
                            }
                        }
                        else
                        {
                            /**
                             * A zero height sector is a special case. In this
                             * instance, the potential division is at the height
                             * of the back ceiling. This is because elsewhere
                             * we automatically fix the case of a floor above a
                             * ceiling by lowering the floor.
                             */
                            coord_t z = scanSec->ceiling().visHeight();

                            if(z > bottom && z < top)
                            {
                                if(intercept(z))
                                {
                                    // All clipped away.
                                    stopScan = true;
                                }
                            }
                        }
                    }
                } while(!stopScan && ++i < 2);

                // Stop the scan when a single sided line is reached.
                if(!iter->hasFrontSections() || !iter->hasBackSections())
                    stopScan = true;
            }
        } while(!stopScan);
    }

private:
    Instance &operator = (Instance const &); // no assignment
};

SectionEdge::SectionEdge(Line::Side &lineSide, int section,
    coord_t lineOffset, Vertex &lineVertex, ClockDirection neighborScanDirection,
    Sector *frontSec, Sector *backSec)
    : d(new Instance(this, &lineSide, section, neighborScanDirection,
                           lineOffset, &lineVertex, frontSec, backSec))
{
    DENG_ASSERT(lineSide.hasSections());
}

SectionEdge::SectionEdge(HEdge &hedge, int section, int edge)
    : d(new Instance(this, &hedge.lineSide(), section,
                           edge? Anticlockwise : Clockwise,
                           hedge.lineOffset() + (edge? hedge.length() : 0),
                           &hedge.vertex(edge),
                           hedge.wallSectionSector(),
                           hedge.wallSectionSector(HEdge::Back)))
{
    DENG_ASSERT(hedge.hasLineSide() && hedge.lineSide().hasSections());
}

SectionEdge::SectionEdge(SectionEdge const &other)
    : d(new Instance(this, *other.d))
{}

bool SectionEdge::isValid() const
{
    return d->isValid;
}

Line::Side &SectionEdge::lineSide() const
{
    return *d->lineSide;
}

Surface &SectionEdge::surface() const
{
    return d->lineSide->surface(d->section);
}

int SectionEdge::section() const
{
    return d->section;
}

Vector2d const &SectionEdge::origin() const
{
    return d->partition().origin;
}

coord_t SectionEdge::lineOffset() const
{
    return d->lineOffset;
}

int SectionEdge::divisionCount() const
{
    return d->isValid? d->interceptCount() - 2 : 0;
}

SectionEdge::Intercept const &SectionEdge::bottom() const
{
    d->verifyValid();
    return d->hplane.intercepts[0];
}

SectionEdge::Intercept const &SectionEdge::top() const
{
    d->verifyValid();
    return d->hplane.intercepts[d->interceptCount()-1];
}

int SectionEdge::firstDivision() const
{
    d->verifyValid();
    return 1;
}

int SectionEdge::lastDivision() const
{
    d->verifyValid();
    return d->interceptCount()-2;
}

Vector2f const &SectionEdge::materialOrigin() const
{
    d->verifyValid();
    return d->materialOrigin;
}

void SectionEdge::prepare()
{
    coord_t bottom, top;
    R_SideSectionCoords(*d->lineSide, d->section, d->frontSec, d->backSec,
                        &bottom, &top, &d->materialOrigin);

    d->isValid = (top >= bottom);
    if(!d->isValid) return;

    d->materialOrigin.x += d->lineOffset;

    d->configure(Partition(d->lineVertex->origin(), Vector2d(0, top - bottom)));

    // Intercepts are sorted in ascending distance order.

    // The first intercept is the bottom.
    d->intercept(bottom);

    if(top > bottom)
    {
        // Add intercepts for neighboring planes (the "divisions").
        d->addPlaneIntercepts(bottom, top);
    }

    // The last intercept is the top.
    d->intercept(top);

    if(d->interceptCount() > 2) // First two are always sorted.
    {
        // Sorting may be required. This shouldn't take too long...
        // There seldom are more than two or three intercepts.
        d->sortAndMergeIntercepts();
    }

    // Sanity check.
    d->assertInterceptsInRange(bottom, top);
}

SectionEdge::Intercept const &SectionEdge::at(int index) const
{
    d->verifyValid();
    return d->hplane.intercepts[index];
}

SectionEdge::Intercepts const &SectionEdge::intercepts() const
{
    d->verifyValid();
    return d->hplane.intercepts;
}
