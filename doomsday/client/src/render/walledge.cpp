/** @file render/walledge.cpp Wall Edge Geometry.
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
#include "Segment"

#include "world/r_world.h" // R_SideSectionCoords
#include "world/lineowner.h"

#include "render/walledge.h"

using namespace de;

/**
 * Determines whether normal smoothing should be performed for the given pair of
 * map surfaces (which are assumed to share an edge).
 *
 * Yes if the angle between the two surfaces is less than 45 degrees.
 * @todo Should be user customizable with a Material property. -ds
 *
 * @param sufA       The "left"  map surface which shares an edge with @a sufB.
 * @param sufB       The "right" map surface which shares an edge with @a sufA.
 * @param angleDiff  Angle difference (i.e., normal delta) between the two surfaces.
 */
static bool shouldSmoothNormals(Surface &sufA, Surface &sufB, binangle_t angleDiff)
{
    DENG2_UNUSED2(sufA, sufB);
    return INRANGE_OF(angleDiff, BANG_180, BANG_45);
}

DENG2_PIMPL_NOREF(WallEdge::Event)
{
    /// Wall edge instance which owns "this" event.
    WallEdge &owner;

    Instance(WallEdge &owner) : owner(owner) {}
};

WallEdge::Event::Event(WallEdge &owner, double distance)
    : WorldEdge::Event(),
      IHPlane::IIntercept(distance),
      d(new Instance(owner))
{}

bool WallEdge::Event::operator < (Event const &other) const
{
    return distance() < other.distance();
}

double WallEdge::Event::distance() const
{
    return IHPlane::IIntercept::distance();
}

Vector3d WallEdge::Event::origin() const
{
    return d->owner.pOrigin() + d->owner.pDirection() * distance();
}

static bool eventSorter(WorldEdge::Event *a, WorldEdge::Event *b)
{
    return *a < *b;
}

DENG2_PIMPL(WallEdge), public IHPlane
{
    WallSpec spec;
    int edge;

    Line::Side *mapSide;
    coord_t mapSideOffset;
    LineOwner *mapLineOwner;

    /// The half-plane which partitions the surface coordinate space.
    Partition hplane;

    Vector3d pOrigin;
    Vector3d pDirection;

    coord_t lo, hi;

    /// Events for the special termination points are allocated with "this".
    Event bottom;
    Event top;

    /// All events along the partition line.
    Events events;

    /// Set to @c true when the events require sorting.
    bool needSortEvents;

    bool isValid;

    Instance(Public *i, WallSpec const &spec, Line::Side *mapSide, int edge,
             coord_t sideOffset, LineOwner *mapLineOwner)
        : Base(i),
          spec(spec),
          edge(edge),
          mapSide(mapSide),
          mapSideOffset(sideOffset),
          mapLineOwner(mapLineOwner),
          lo(0), hi(0),
          bottom(*i, 0),
          top(*i, 1),
          needSortEvents(false),
          isValid(false)
    {
        Vector2f materialOffset;
        R_SideSectionCoords(*mapSide, spec.section, spec.flags.testFlag(WallSpec::SkyClip),
                            &lo, &hi, &materialOffset);

        isValid = (hi > lo);

        self.materialOrigin += materialOffset;

#ifdef DENG2_QT_4_7_OR_NEWER
        events.reserve(2 + 2);
#endif

        // The first event is the bottom termination event.
        events.append(&bottom);

        // The last event is the top termination event.
        events.append(&top);
    }

    ~Instance()
    {
        clearIntercepts();
    }

    void verifyValid() const
    {
        if(!isValid)
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw InvalidError("WallEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    EventIndex toEventIndex(double distance)
    {
        for(EventIndex i = 0; i < events.count(); ++i)
        {
            Event *icpt = events[i];
            if(de::fequal(icpt->distance(), distance))
                return i;
        }
        return InvalidIndex;
    }

    inline bool haveEvent(double distance) {
        return toEventIndex(distance) != InvalidIndex;
    }

    Event &createEvent(double distance)
    {
        return *intercept(distance);
    }

    // Implements IHPlane
    void configure(Partition const &newPartition)
    {
        hplane = newPartition;
    }

    // Implements IHPlane
    Partition const &partition() const
    {
        return hplane;
    }

    // Implements IHPlane
    Event *intercept(double distance)
    {
        Event *newEvent = new Event(self, distance);
        events.append(newEvent);

        // We'll need to resort the events.
        needSortEvents = true;

        return newEvent;
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        // Any work to do?
        if(!needSortEvents) return;

        qSort(events.begin(), events.end(), eventSorter);
        needSortEvents = false;
    }

    // Implements IHPlane
    void clearIntercepts()
    {
        while(!events.isEmpty())
        {
            Event *event = events.takeLast();
            if(!(event == &bottom || event == &top))
                delete event;
        }

        // An empty event list is logically sorted.
        needSortEvents = false;
    }

    // Implements IHPlane
    Event const &at(EventIndex index) const
    {
        if(index >= 0 && index < interceptCount())
        {
            return *events[index];
        }
        /// @throw UnknownInterceptError The specified intercept index is not valid.
        throw UnknownInterceptError("WallEdge::at", QString("Index '%1' does not map to a known intercept (count: %2)")
                                                        .arg(index).arg(interceptCount()));
    }

    // Implements IHPlane
    int interceptCount() const
    {
        return events.count();
    }

#ifdef DENG_DEBUG
    void printIntercepts() const
    {
        EventIndex index = 0;
        foreach(Event const *icpt, events)
        {
            LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt->distance();
        }
    }
#endif

    /**
     * Ensure all intercepts do not exceed the specified closed range.
     */
    void assertInterceptsInRange(double low, double hi) const
    {
#ifdef DENG_DEBUG
        foreach(Event const *icpt, events)
        {
            DENG2_ASSERT(icpt->distance() >= low && icpt->distance() <= hi);
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    inline double distanceTo(coord_t worldHeight) const
    {
        return (worldHeight - lo) / (hi - lo);
    }

    /**
     * @todo: Use the half-edge rings instead of LineOwners.
     */
    void addNeighborIntercepts(coord_t bottom, coord_t top)
    {
        if(!mapLineOwner) return;

        Sector const *frontSec = mapSide->sectorPtr();
        LineOwner *own = mapLineOwner;
        bool stopScan = false;
        do
        {
            own = &own->navigate(edge? Anticlockwise : Clockwise);

            if(own == mapLineOwner)
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
                    if(!i && iter->hasFrontSector() && iter->frontSectorPtr() != frontSec)
                        scanSec = iter->frontSectorPtr();
                    else if(i && iter->hasBackSector() && iter->backSectorPtr() != frontSec)
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
                                    double distance = distanceTo(plane.visHeight());

                                    if(!haveEvent(distance))
                                    {
                                        createEvent(distance);

                                        // Have we reached the div limit?
                                        if(interceptCount() == WALLEDGE_MAX_INTERCEPTS)
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
                                double distance = distanceTo(z);

                                if(!haveEvent(distance))
                                {
                                    createEvent(distance);

                                    // All clipped away.
                                    stopScan = true;
                                }
                            }
                        }
                    }
                } while(!stopScan && ++i < 2);

                // Stop the scan when a side with no back sector is reached.
                if(!iter->hasFrontSector() || !iter->hasBackSector())
                    stopScan = true;
            }
        } while(!stopScan);
    }

    /**
     * Find the neighbor surface for the edge which we will use to calculate the
     * "blend" properties (e.g., smoothed edge normal).
     *
     * @todo: Use the half-edge rings instead of LineOwners.
     */
    Surface *findBlendNeighbor(binangle_t &diff)
    {
        diff = 0;

        // Are we not blending?
        if(spec.flags.testFlag(WallSpec::NoEdgeNormalSmoothing))
            return 0;

        // Polyobj lines have no owner rings.
        if(mapSide->line().definesPolyobj())
            return 0;

        LineOwner const *farVertOwner = mapSide->line().vertexOwner(mapSide->sideId() ^ edge);
        Line *neighbor;
        if(R_SideBackClosed(*mapSide))
        {
            neighbor = R_FindSolidLineNeighbor(mapSide->sectorPtr(), &mapSide->line(),
                                               farVertOwner, edge, &diff);
        }
        else
        {
            neighbor = R_FindLineNeighbor(mapSide->sectorPtr(), &mapSide->line(),
                                          farVertOwner, edge, &diff);
        }

        // No suitable line neighbor?
        if(!neighbor) return 0;

        // Choose the correct side of the neighbor (determined by which vertex is shared).
        Line::Side *otherSide;
        if(&neighbor->vertex(edge ^ 1) == &mapSide->vertex(edge))
            otherSide = &neighbor->front();
        else
            otherSide = &neighbor->back();

        // We can only blend if the neighbor has a surface.
        if(!otherSide->hasSections()) return 0;

        /// @todo Do not assume the neighbor is the middle section of @var otherSide.
        return &otherSide->middle();
    }

    void prepare()
    {
        if(!isValid) return;

        pOrigin = Vector3d(self.origin(), lo);
        pDirection = Vector3d(0, 0, hi - lo);

        // Add intecepts for neighbor planes?
        if(!spec.flags.testFlag(WallSpec::NoEdgeDivisions) && !de::fequal(hi, lo))
        {
            configure(Partition(Vector2d(0, hi - lo)));

            // Add intercepts (the "divisions") in ascending distance order.
            addNeighborIntercepts(lo, hi);

            // Sorting may be required. This shouldn't take too long...
            // There seldom are more than two or three intercepts.
            sortAndMergeIntercepts();
        }

        // Sanity check.
        assertInterceptsInRange(0, 1);

        // Determine the edge normal.
        /// @todo Cache the smoothed normal value somewhere.
        Surface &surface = mapSide->surface(spec.section);
        binangle_t angleDiff;
        Surface *blendSurface = findBlendNeighbor(angleDiff);

        if(blendSurface && shouldSmoothNormals(surface, *blendSurface, angleDiff))
        {
            // Average normals.
            self.normal = Vector3f(surface.normal() + blendSurface->normal()) / 2;
        }
        else
        {
            self.normal = surface.normal();
        }
    }
};

WallEdge::WallEdge(WallSpec const &spec, Segment &segment, int edge)
    : WorldEdge((edge? segment.to() : segment.from()).origin(),
                EdgeAttribs(Vector2f(segment.lineSideOffset() + (edge? segment.length() : 0), 0))),
      d(new Instance(this, spec, &segment.lineSide(), edge,
                           segment.lineSideOffset() + (edge? segment.length() : 0),
                           segment.lineSide().line().vertexOwner(edge? segment.to() : segment.from())))
{
    DENG_ASSERT(segment.hasLineSide() && segment.lineSide().hasSections());

    /// @todo Defer until necessary.
    d->prepare();
}

Vector3d const &WallEdge::pOrigin() const
{
    return d->pOrigin;
}

Vector3d const &WallEdge::pDirection() const
{
    return d->pDirection;
}

WallSpec const &WallEdge::spec() const
{
    return d->spec;
}

Line::Side &WallEdge::mapSide() const
{
    return *d->mapSide;
}

coord_t WallEdge::mapSideOffset() const
{
    return d->mapSideOffset;
}

int WallEdge::divisionCount() const
{
    return d->isValid? d->interceptCount() - 2 : 0;
}

WallEdge::EventIndex WallEdge::firstDivision() const
{
    return divisionCount()? 1 : InvalidIndex;
}

WallEdge::EventIndex WallEdge::lastDivision() const
{
    return divisionCount()? d->interceptCount() - 2 : InvalidIndex;
}

WallEdge::Events const &WallEdge::events() const
{
    d->verifyValid();
    return d->events;
}

WallEdge::Event const &WallEdge::at(EventIndex index) const
{
    return *events().at(index);
}

bool WallEdge::isValid() const
{
    return d->isValid;
}

WallEdge::Event const &WallEdge::first() const
{
    return at(0);
}

WallEdge::Event const &WallEdge::last() const
{
    return at(d->events.size() - 1);
}
