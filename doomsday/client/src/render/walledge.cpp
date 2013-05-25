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

#include "map/r_world.h" // R_SideSectionCoords
#include "map/lineowner.h"

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

static bool interceptSorter(WallEdge::Event *a, WallEdge::Event *b)
{
    return *a < *b;
}

DENG2_PIMPL_NOREF(WallEdge::Event)
{
    /// Wall edge instance which owns "this" event.
    WallEdge &owner;

    Instance(WallEdge &owner) : owner(owner) {}
};

WallEdge::Event::Event(WallEdge &owner, double distance)
    : IHPlane::IIntercept(distance),
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

void WallEdge::Event::setDistance(double newDistance)
{
    IHPlane::IIntercept::_distance = newDistance;
}

Vector3d WallEdge::Event::origin() const
{
    return Vector3d(d->owner.origin, distance());
}

DENG2_PIMPL(WallEdge), public IHPlane
{
    WallSpec spec;
    int edge;

    Line::Side *mapSide;
    coord_t mapSideOffset;
    Vertex *mapVertex;

    bool isValid;

    /// Events for the bottom and top points are allocated with "this".
    WallEdge::Event bottom;
    WallEdge::Event top;

    /// Data for the IHPlane model.
    struct HPlane
    {
        /// The partition line.
        Partition partition;

        /// All the intercept events along the half-plane.
        WallEdge::Events intercepts;

        /// Set to @c true when @var intercepts requires sorting.
        bool needSortIntercepts;

        HPlane() : needSortIntercepts(false) {}

        HPlane(HPlane const & other)
            : partition         (other.partition),
              intercepts        (other.intercepts),
              needSortIntercepts(other.needSortIntercepts)
        {
#ifdef DENG2_QT_4_7_OR_NEWER
            intercepts.reserve(2 + 2);
#endif
        }

        ~HPlane() { DENG_ASSERT(intercepts.isEmpty()); }

    } hplane;

    Instance(Public *i, WallSpec const &spec, Line::Side *mapSide, int edge,
             coord_t sideOffset, Vertex *mapVertex)
        : Base(i),
          spec(spec),
          edge(edge),
          mapSide(mapSide),
          mapSideOffset(sideOffset),
          mapVertex(mapVertex),
          isValid(false),
          bottom(*i),
          top(*i)
    {
        coord_t lo, hi;
        R_SideSectionCoords(*mapSide, spec.section, spec.flags.testFlag(WallSpec::SkyClip),
                            &lo, &hi, &self.materialOrigin);
        bottom.setDistance(lo);
        top.setDistance(hi);

        isValid = (top.distance() >= bottom.distance());
    }

    ~Instance() { clearIntercepts(); }

    void verifyValid() const
    {
        if(!isValid)
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw WallEdge::InvalidError("WallEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    WallEdge::EventIndex toEventIndex(double distance)
    {
        for(WallEdge::EventIndex i = 0; i < hplane.intercepts.count(); ++i)
        {
            WallEdge::Event *icpt = hplane.intercepts[i];
            if(de::fequal(icpt->distance(), distance))
                return i;
        }
        return WallEdge::InvalidIndex;
    }

    inline bool haveEvent(double distance) {
        return toEventIndex(distance) != WallEdge::InvalidIndex;
    }

    WallEdge::Event &createEvent(double distance)
    {
        return *intercept(distance);
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

    // Implements IHPlane
    WallEdge::Event *intercept(double distance)
    {
        hplane.intercepts.append(new WallEdge::Event(self, distance));
        WallEdge::Event *newIntercept = hplane.intercepts.last();

        // The addition of a new intercept means we'll need to resort.
        hplane.needSortIntercepts = true;
        return newIntercept;
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        // Any work to do?
        if(!hplane.needSortIntercepts) return;

        qSort(hplane.intercepts.begin(), hplane.intercepts.end(), interceptSorter);
        hplane.needSortIntercepts = false;
    }

    // Implements IHPlane
    void clearIntercepts()
    {
        foreach(WallEdge::Event *icpt, hplane.intercepts)
        {
            if(icpt == &bottom || icpt == &top) continue;
            delete icpt;
        }
        hplane.intercepts.clear();
        // An empty intercept list is logically sorted.
        hplane.needSortIntercepts = false;
    }

    // Implements IHPlane
    WallEdge::Event const &at(WallEdge::EventIndex index) const
    {
        if(index >= 0 && index < interceptCount())
        {
            return *hplane.intercepts[index];
        }
        /// @throw IHPlane::UnknownInterceptError The specified intercept index is not valid.
        throw IHPlane::UnknownInterceptError("HPlane::at", QString("Index '%1' does not map to a known intercept (count: %2)")
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
        WallEdge::EventIndex index = 0;
        foreach(WallEdge::Event const *icpt, hplane.intercepts)
        {
            LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt->distance();
        }
    }
#endif

    /**
     * Ensure all intercepts do not exceed the specified closed range.
     */
    void assertInterceptsInRange(coord_t low, coord_t hi) const
    {
#ifdef DENG_DEBUG
        foreach(WallEdge::Event const *icpt, hplane.intercepts)
        {
            DENG2_ASSERT(icpt->distance() >= low && icpt->distance() <= hi);
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    void addPlaneIntercepts(coord_t bottom, coord_t top)
    {
        DENG_ASSERT(top > bottom);

        // Retrieve the start owner node.
        LineOwner *base = mapSide->line().vertexOwner(*mapVertex);
        if(!base) return;

        // Check for neighborhood division?
        if(spec.section == Line::Side::Middle && mapSide->back().hasSector())
            return;

        Sector const *frontSec = mapSide->sectorPtr();
        LineOwner *own = base;
        bool stopScan = false;
        do
        {
            own = &own->navigate(edge? Anticlockwise : Clockwise);

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
                                    coord_t distance = plane.visHeight();

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
                                if(!haveEvent(z))
                                {
                                    createEvent(z);

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

        LineOwner const *farVertOwner = mapSide->line().vertexOwner(mapSide->lineSideId() ^ edge);
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

        self.materialOrigin.x += mapSideOffset;

        coord_t const lo = bottom.distance();
        coord_t const hi = top.distance();

        configure(Partition(mapVertex->origin(), Vector2d(0, hi - lo)));

        // Intercepts are sorted in ascending distance order.

        if(!de::fequal(hi, lo))
        {
            // Add intercepts for neighboring planes (the "divisions").
            addPlaneIntercepts(lo, hi);

            // Sorting may be required. This shouldn't take too long...
            // There seldom are more than two or three intercepts.
            sortAndMergeIntercepts();
        }

        // The first intercept is the bottom.
        hplane.intercepts.prepend(&bottom);

        // The last intercept is the top.
        hplane.intercepts.append(&top);

        // Sanity check.
        assertInterceptsInRange(lo, hi);

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

WallEdge::WallEdge(WallSpec const &spec, HEdge &hedge, int edge)
    : WorldEdge(EdgeAttribs((edge? hedge.twin() : hedge.vertex()).origin())),
      d(new Instance(this, spec, &hedge.lineSide(), edge,
                           hedge.lineOffset() + (edge? hedge.length() : 0),
                           edge? &hedge.twin().vertex() : &hedge.vertex()))
{
    DENG_ASSERT(hedge.hasLineSide() && hedge.lineSide().hasSections());

    /// @todo Defer until necessary.
    d->prepare();
}

bool WallEdge::isValid() const
{
    return d->isValid;
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

WallEdge::Event const &WallEdge::first() const
{
    d->verifyValid();
    return *d->hplane.intercepts[0];
}

WallEdge::Event const &WallEdge::last() const
{
    d->verifyValid();
    return *d->hplane.intercepts[d->interceptCount()-1];
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
    return d->hplane.intercepts;
}

WallSpec const &WallEdge::spec() const
{
    return d->spec;
}
