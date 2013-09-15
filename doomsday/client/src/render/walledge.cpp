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

#include "BspLeaf"
#include "Sector"
#include "Surface"

#include "Face"

#include "world/lineowner.h"
#include "world/p_players.h"
#include "world/maputil.h"

#include "render/rend_main.h" // devRendSkyMode, remove me

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

static inline coord_t lineSideOffset(LineSideSegment &seg, int edge)
{
    return seg.lineSideOffset() + (edge? seg.length() : 0);
}

DENG2_PIMPL(WallEdge), public IHPlane
{
    WallSpec spec;
    int edge;

    HEdge *hedge;

    /// The half-plane which partitions the surface coordinate space.
    Partition hplane;

    Vector3d pOrigin;
    Vector3d pDirection;

    coord_t lo, hi;

    /// Events for the special termination points are allocated with "this".
    Event bottom;
    Event top;

    /// All events along the partition line.
    Events *events;
    bool needSortEvents;

    Vector2f materialOrigin;

    Vector3f normal;
    bool needUpdateNormal;

    Instance(Public *i, WallSpec const &spec, HEdge &wallHEdge, int edge)
        : Base(i),
          spec(spec),
          edge(edge),
          hedge(&wallHEdge),
          lo(0),
          hi(0),
          bottom(*i, 0),
          top(*i, 1),
          events(0),
          needSortEvents(false),
          needUpdateNormal(true)
    {
        // Determine the map space Z coordinates of the wall section.
        LineSideSegment &seg   = lineSideSegment();
        Line const &line       = seg.line();
        bool const unpegBottom = (line.flags() & DDLF_DONTPEGBOTTOM) != 0;
        bool const unpegTop    = (line.flags() & DDLF_DONTPEGTOP)    != 0;

        BspLeaf const *leaf =
            line.definesPolyobj()? &line.polyobj().bspLeaf()
                                 : &hedge->face().mapElement()->as<BspLeaf>();

        if(seg.lineSide().considerOneSided())
        {
            if(spec.section == LineSide::Middle)
            {
                lo = leaf->visFloorHeightSmoothed();
                hi = leaf->visCeilingHeightSmoothed();
            }
            else
            {
                lo = hi = leaf->visFloorHeightSmoothed();
            }

            materialOrigin = seg.lineSide().middle().materialOriginSmoothed();
            if(unpegBottom)
            {
                materialOrigin.y -= hi - lo;
            }
        }
        else
        {
            // Two sided.
            BspLeaf const *backLeaf =
                line.definesPolyobj()? leaf
                                     : &hedge->twin().face().mapElement()->as<BspLeaf>();

            Plane const *ffloor = &leaf->visFloor();
            Plane const *fceil  = &leaf->visCeiling();
            Plane const *bfloor = &backLeaf->visFloor();
            Plane const *bceil  = &backLeaf->visCeiling();

            switch(spec.section)
            {
            case LineSide::Top:
                // Self-referencing lines only ever get a middle.
                if(!line.isSelfReferencing())
                {
                    // Can't go over front ceiling (would induce geometry flaws).
                    if(bceil->heightSmoothed() < ffloor->heightSmoothed())
                        lo = ffloor->heightSmoothed();
                    else
                        lo = bceil->heightSmoothed();

                    hi = fceil->heightSmoothed();

                    if(spec.flags.testFlag(WallSpec::SkyClip)
                       && fceil->surface().hasSkyMaskedMaterial()
                       && bceil->surface().hasSkyMaskedMaterial())
                    {
                        hi = lo;
                    }

                    materialOrigin = seg.lineSide().middle().materialOriginSmoothed();
                    if(!unpegTop)
                    {
                        // Align with normal middle texture.
                        materialOrigin.y -= fceil->heightSmoothed() - bceil->heightSmoothed();
                    }
                }
                break;

            case LineSide::Bottom:
                // Self-referencing lines only ever get a middle.
                if(!line.isSelfReferencing())
                {
                    bool const raiseToBackFloor =
                        (fceil->surface().hasSkyMaskedMaterial()
                         && bceil->surface().hasSkyMaskedMaterial()
                         && fceil->heightSmoothed() < bceil->heightSmoothed()
                         && bfloor->heightSmoothed() > fceil->heightSmoothed());

                    coord_t t = bfloor->heightSmoothed();

                    lo = ffloor->heightSmoothed();

                    // Can't go over the back ceiling, would induce polygon flaws.
                    if(bfloor->heightSmoothed() > bceil->heightSmoothed())
                        t = bceil->heightSmoothed();

                    // Can't go over front ceiling, would induce polygon flaws.
                    // In the special case of a sky masked upper we must extend the bottom
                    // section up to the height of the back floor.
                    if(t > fceil->heightSmoothed() && !raiseToBackFloor)
                        t = fceil->heightSmoothed();

                    hi = t;

                    if(spec.flags.testFlag(WallSpec::SkyClip)
                       && ffloor->surface().hasSkyMaskedMaterial()
                       && bfloor->surface().hasSkyMaskedMaterial())
                    {
                        lo = hi;
                    }

                    materialOrigin = seg.lineSide().bottom().materialOriginSmoothed();
                    if(bfloor->heightSmoothed() > fceil->heightSmoothed())
                    {
                        materialOrigin.y -= (raiseToBackFloor? t : fceil->heightSmoothed())
                                          - bfloor->heightSmoothed();
                    }

                    if(unpegBottom)
                    {
                        // Align with normal middle texture.
                        materialOrigin.y += (raiseToBackFloor? t : fceil->heightSmoothed())
                                          - bfloor->heightSmoothed();
                    }
                }
                break;

            case LineSide::Middle: {
                LineSide const &lineSide = seg.lineSide();
                Surface const &middle    = lineSide.middle();

                if(!line.isSelfReferencing())
                {
                    lo = de::max(bfloor->heightSmoothed(), ffloor->heightSmoothed());
                    hi = de::min(bceil->heightSmoothed(),  fceil->heightSmoothed());
                }
                else
                {
                    // Use the unmapped heights for positioning purposes.
                    lo = lineSide.sector().floor().heightSmoothed();
                    hi = lineSide.back().sector().ceiling().heightSmoothed();
                }

                materialOrigin = Vector2f(middle.materialOriginSmoothed().x, 0);

                // Perform clipping.
                if(middle.hasMaterial()
                   && !seg.lineSide().isFlagged(SDF_MIDDLE_STRETCH))
                {
                    coord_t openBottom, openTop;
                    if(!line.isSelfReferencing())
                    {
                        openBottom = lo;
                        openTop    = hi;
                    }
                    else
                    {
                        openBottom = ffloor->heightSmoothed();
                        openTop    = fceil->heightSmoothed();
                    }

                    if(openTop > openBottom)
                    {
                        if(unpegBottom)
                        {
                            lo += middle.materialOriginSmoothed().y;
                            hi = lo + middle.material().height();
                        }
                        else
                        {
                            hi += middle.materialOriginSmoothed().y;
                            lo = hi - middle.material().height();
                        }

                        if(hi > openTop)
                        {
                            materialOrigin.y = hi - openTop;
                        }

                        // Clip it?
                        bool const clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                        bool const clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());
                        if(clipTop || clipBottom)
                        {
                            if(clipBottom && lo < openBottom)
                                lo = openBottom;

                            if(clipTop && hi > openTop)
                                hi = openTop;
                        }

                        if(!clipTop)
                        {
                            materialOrigin.y = 0;
                        }
                    }
                }
                break; }
            }
        }
        materialOrigin += Vector2f(lineSideOffset(seg, edge), 0);

        pOrigin    = Vector3d(self.origin(), lo);
        pDirection = Vector3d(0, 0, hi - lo);
    }

    ~Instance()
    {
        clearIntercepts();
    }

    inline LineSideSegment &lineSideSegment()
    {
        return hedge->mapElement()->as<LineSideSegment>();
    }

    void verifyValid() const
    {
        if(!self.isValid())
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw InvalidError("WallEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    EventIndex toEventIndex(double distance)
    {
        DENG_ASSERT(events != 0);

        for(EventIndex i = 0; i < events->count(); ++i)
        {
            Event *icpt = (*events)[i];
            if(de::fequal(icpt->distance(), distance))
                return i;
        }
        return InvalidIndex;
    }

    inline bool haveEvent(double distance)
    {
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
        DENG_ASSERT(events != 0);

        Event *newEvent = new Event(self, distance);
        events->append(newEvent);

        // We'll need to resort the events.
        needSortEvents = true;

        return newEvent;
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        DENG_ASSERT(events != 0);

        // Any work to do?
        if(!needSortEvents) return;

        qSort(events->begin(), events->end(), eventSorter);
        needSortEvents = false;
    }

    // Implements IHPlane
    void clearIntercepts()
    {
        if(events)
        {
            while(!events->isEmpty())
            {
                Event *event = events->takeLast();
                if(!(event == &bottom || event == &top))
                    delete event;
            }

            delete events; events = 0;
        }

        // An empty event list is logically sorted.
        needSortEvents = false;
    }

    // Implements IHPlane
    Event const &at(EventIndex index) const
    {
        DENG_ASSERT(events != 0);

        if(index >= 0 && index < interceptCount())
        {
            return *(*events)[index];
        }
        /// @throw UnknownInterceptError The specified intercept index is not valid.
        throw UnknownInterceptError("WallEdge::at", QString("Index '%1' does not map to a known intercept (count: %2)")
                                                        .arg(index).arg(interceptCount()));
    }

    // Implements IHPlane
    int interceptCount() const
    {
        DENG_ASSERT(events != 0);

        return events->count();
    }

#ifdef DENG_DEBUG
    void printIntercepts() const
    {
        DENG_ASSERT(events != 0);

        EventIndex index = 0;
        foreach(Event const *icpt, *events)
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
        DENG_ASSERT(events != 0);
        foreach(Event const *icpt, *events)
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
     * @todo fixme: Should use the visual plane heights of sector clusters.
     */
    void addNeighborIntercepts(coord_t bottom, coord_t top)
    {
        LineSideSegment const &seg = lineSideSegment();
        LineOwner *mapLineOwner    = seg.line().vertexOwner(edge? hedge->twin().vertex() : hedge->vertex());
        if(!mapLineOwner) return;

        Sector const *frontSec = seg.lineSide().sectorPtr();
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
                        if(scanSec->ceiling().heightSmoothed() - scanSec->floor().heightSmoothed() > 0)
                        {
                            for(int j = 0; j < scanSec->planeCount() && !stopScan; ++j)
                            {
                                Plane const &plane = scanSec->plane(j);

                                if(plane.heightSmoothed() > bottom && plane.heightSmoothed() < top)
                                {
                                    double distance = distanceTo(plane.heightSmoothed());

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
                                    if(plane.isSectorFloor() && plane.heightSmoothed() > bottom)
                                        bottom = plane.heightSmoothed();
                                    else if(plane.isSectorCeiling() && plane.heightSmoothed() < top)
                                        top = plane.heightSmoothed();

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
                            coord_t z = scanSec->ceiling().heightSmoothed();

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

    void prepareEvents()
    {
        DENG_ASSERT(self.isValid());

        clearIntercepts();

        events = new Events;
#ifdef DENG2_QT_4_7_OR_NEWER
        events->reserve(2 + 2);
#endif

        // The first event is the bottom termination event.
        events->append(&bottom);

        // The last event is the top termination event.
        events->append(&top);

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

        LineSide const &lineSide = lineSideSegment().lineSide();

        // Polyobj lines have no owner rings.
        if(lineSide.line().definesPolyobj())
            return 0;

        LineOwner const *farVertOwner = lineSide.line().vertexOwner(lineSide.sideId() ^ edge);
        Line *neighbor;
        if(R_SideBackClosed(lineSide))
        {
            neighbor = R_FindSolidLineNeighbor(lineSide.sectorPtr(), &lineSide.line(),
                                               farVertOwner, edge, &diff);
        }
        else
        {
            neighbor = R_FindLineNeighbor(lineSide.sectorPtr(), &lineSide.line(),
                                          farVertOwner, edge, &diff);
        }

        // No suitable line neighbor?
        if(!neighbor) return 0;

        // Choose the correct side of the neighbor (determined by which vertex is shared).
        LineSide *otherSide;
        if(&neighbor->vertex(edge ^ 1) == &lineSide.vertex(edge))
            otherSide = &neighbor->front();
        else
            otherSide = &neighbor->back();

        // We can only blend if the neighbor has a surface.
        if(!otherSide->hasSections()) return 0;

        /// @todo Do not assume the neighbor is the middle section of @var otherSide.
        return &otherSide->middle();
    }

    /**
     * Determine the (possibly smoothed) edge normal.
     * @todo Cache the smoothed normal value somewhere...
     */
    void updateNormal()
    {
        needUpdateNormal = false;

        LineSide &lineSide = lineSideSegment().lineSide();
        Surface &surface   = lineSide.surface(spec.section);

        binangle_t angleDiff;
        Surface *blendSurface = findBlendNeighbor(angleDiff);

        if(blendSurface && shouldSmoothNormals(surface, *blendSurface, angleDiff))
        {
            // Average normals.
            normal = Vector3f(surface.normal() + blendSurface->normal()) / 2;
        }
        else
        {
            normal = surface.normal();
        }
    }
};

WallEdge::WallEdge(WallSpec const &spec, HEdge &hedge, int edge)
    : WorldEdge((edge? hedge.twin() : hedge).origin()),
      d(new Instance(this, spec, hedge, edge))
{}

Vector3d const &WallEdge::pOrigin() const
{
    return d->pOrigin;
}

Vector3d const &WallEdge::pDirection() const
{
    return d->pDirection;
}

Vector2f WallEdge::materialOrigin() const
{
    return d->materialOrigin;
}

Vector3f WallEdge::normal() const
{
    if(d->needUpdateNormal)
    {
        d->updateNormal();
    }
    return d->normal;
}

WallSpec const &WallEdge::spec() const
{
    return d->spec;
}

LineSide &WallEdge::mapLineSide() const
{
    return d->lineSideSegment().lineSide();
}

coord_t WallEdge::mapLineSideOffset() const
{
    return lineSideOffset(d->lineSideSegment(), d->edge);
}

int WallEdge::divisionCount() const
{
    if(!isValid()) return 0;
    if(!d->events)
    {
        d->prepareEvents();
    }
    return d->interceptCount() - 2;
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
    if(!d->events)
    {
        d->prepareEvents();
    }
    return *d->events;
}

WallEdge::Event const &WallEdge::at(EventIndex index) const
{
    return *events().at(index);
}

bool WallEdge::isValid() const
{
    return d->hi > d->lo;
}

WallEdge::Event const &WallEdge::first() const
{
    return d->bottom;
}

WallEdge::Event const &WallEdge::last() const
{
    return d->top;
}
