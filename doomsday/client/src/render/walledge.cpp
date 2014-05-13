/** @file walledge.cpp  Wall Edge Geometry.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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
#include "render/walledge.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "Face"
#include "Plane"
#include "Sector"
#include "SectorCluster"
#include "Surface"
#include "world/map.h"
#include "world/lineowner.h"
#include "world/maputil.h"
#include "world/p_players.h"
#include "render/rend_main.h"
#include "WallSpec"

using namespace de;

/// Maximum number of intercepts in a WallEdge.
#define WALLEDGE_MAX_INTERCEPTS  64

static bool eventSorter(WallEdge::Event *a, WallEdge::Event *b)
{
    return *a < *b;
}

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

WallEdge::Event::Event(double distance)
    : IHPlane::IIntercept(distance)
    , _section(0)
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
    return section().pOrigin() + section().pDirection() * distance();
}

WallEdge::WallSection &WallEdge::Event::section() const
{
    DENG2_ASSERT(_section != 0);
    return *_section;
}

DENG2_PIMPL(WallEdge::WallSection), public IHPlane
{
    WallEdge &owner;
    SectionId id;
    WallSpec spec;

    Vector3d pOrigin;
    Vector3d pDirection;

    /// Events for the special termination points are allocated with "this".
    WallEdge::Event bottom;
    WallEdge::Event top;

    Partition hplane;     ///< Half-plane partition of surface space.
    Events *events;       ///< All events along the partition line.
    bool needSortEvents;

    /// Edge attributes:
    Vector2f materialOrigin;
    Vector3f normal;
    bool needUpdateNormal;

    bool needPrepare;

    Instance(Public *i, WallEdge &owner, SectionId id, WallSpec const &spec)
        : Base            (i)
        , owner           (owner)
        , id              (id)
        , spec            (spec)
        , bottom          (0)
        , top             (1)
        , events          (0)
        , needSortEvents  (false)
        , needUpdateNormal(true)
        , needPrepare     (true)
    {
        bottom._section = thisPublic;
        top._section    = thisPublic;
    }

    ~Instance() { clearIntercepts(); }

    inline LineSide &lineSide() const {
        return owner.hedge().mapElementAs<LineSideSegment>().lineSide();
    }

    Surface &lineSideSurface()
    {
        switch(id)
        {
        case WallMiddle: return lineSide().middle();
        case WallBottom: return lineSide().bottom();
        case WallTop:    return lineSide().top();
        }
        throw Error("lineSideSurface", "Invalid id");
    }

    void prepareIfNeeded()
    {
        if(!needPrepare) return;
        needPrepare = false;

        coord_t lo = 0, hi = 0;

        // Determine the map space Z coordinates of the wall section.
        Line const &line       = lineSide().line();
        bool const unpegBottom = (line.flags() & DDLF_DONTPEGBOTTOM) != 0;
        bool const unpegTop    = (line.flags() & DDLF_DONTPEGTOP)    != 0;

        SectorCluster const *cluster =
            (line.definesPolyobj()? &line.polyobj().bspLeaf().subspace()
                                  : &owner.hedge().face().mapElementAs<ConvexSubspace>())->clusterPtr();

        if(lineSide().considerOneSided() ||
           // Mapping errors may result in a line segment missing a back face.
           (!line.definesPolyobj() && !owner.hedge().twin().hasFace()))
        {
            if(id == WallMiddle)
            {
                lo = cluster->visFloor().heightSmoothed();
                hi = cluster->visCeiling().heightSmoothed();
            }
            else
            {
                lo = hi = cluster->visFloor().heightSmoothed();
            }

            materialOrigin = lineSide().middle().materialOriginSmoothed();
            if(unpegBottom)
            {
                materialOrigin.y -= hi - lo;
            }
        }
        else
        {
            // Two sided.
            SectorCluster const *backCluster =
                line.definesPolyobj()? cluster
                                     : owner.hedge().twin().face().mapElementAs<ConvexSubspace>().clusterPtr();

            Plane const *ffloor = &cluster->visFloor();
            Plane const *fceil  = &cluster->visCeiling();
            Plane const *bfloor = &backCluster->visFloor();
            Plane const *bceil  = &backCluster->visCeiling();

            switch(id)
            {
            case WallTop:
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

                    materialOrigin = lineSide().middle().materialOriginSmoothed();
                    if(!unpegTop)
                    {
                        // Align with normal middle texture.
                        materialOrigin.y -= fceil->heightSmoothed() - bceil->heightSmoothed();
                    }
                }
                break;

            case WallBottom:
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

                    materialOrigin = lineSide().bottom().materialOriginSmoothed();
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

            case WallMiddle: {
                Surface const &middle = lineSide().middle();

                if(!line.isSelfReferencing() && ffloor == &cluster->sector().floor())
                {
                    lo = de::max(bfloor->heightSmoothed(), ffloor->heightSmoothed());
                }
                else
                {
                    // Use the unmapped heights for positioning purposes.
                    lo = lineSide().sector().floor().heightSmoothed();
                }

                if(!line.isSelfReferencing() && fceil == &cluster->sector().ceiling())
                {
                    hi = de::min(bceil->heightSmoothed(),  fceil->heightSmoothed());
                }
                else
                {
                    // Use the unmapped heights for positioning purposes.
                    hi = lineSide().back().sector().ceiling().heightSmoothed();
                }

                materialOrigin = Vector2f(middle.materialOriginSmoothed().x, 0);

                // Perform clipping.
                if(middle.hasMaterial()
                   && !lineSide().isFlagged(SDF_MIDDLE_STRETCH))
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
        materialOrigin += Vector2f(owner.lineSideOffset(), 0);

        pOrigin    = Vector3d(owner.origin(), lo);
        pDirection = Vector3d(0, 0, hi - lo);
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
        DENG2_ASSERT(events != 0);

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

    WallEdge::Event &createEvent(double distance)
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
    WallEdge::Event *intercept(double distance)
    {
        DENG2_ASSERT(events != 0);

        WallEdge::Event *newEvent = new WallEdge::Event(distance);
        newEvent->_section = thisPublic;
        events->append(newEvent);

        // We'll need to resort the events.
        needSortEvents = true;

        return newEvent;
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        DENG2_ASSERT(events != 0);

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
    WallEdge::Event const &at(EventIndex index) const
    {
        DENG2_ASSERT(events != 0);

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
        DENG2_ASSERT(events != 0);

        return events->count();
    }

#ifdef DENG_DEBUG
    void printIntercepts() const
    {
        DENG2_ASSERT(events != 0);

        EventIndex index = 0;
        foreach(Event const *icpt, *events)
        {
            LOGDEV_MAP_MSG(" %u: >%1.2f ") << (index++) << icpt->distance();
        }
    }
#endif

    /**
     * Ensure all intercepts do not exceed the specified closed range.
     */
    void assertInterceptsInRange(double low, double hi) const
    {
#ifdef DENG2_DEBUG
        DENG2_ASSERT(events != 0);
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
        return (worldHeight - bottom.z()) / (top.z() - bottom.z());
    }

    void addNeighborIntercepts(coord_t bottom, coord_t top)
    {
        ClockDirection const direction = owner.side()? Clockwise : Anticlockwise;

        HEdge const *base  = &owner.hedge();
        HEdge const *hedge = base;
        while((hedge = &SectorClusterCirculator::findBackNeighbor(*hedge, direction)) != base)
        {
            // Stop if there is no back subspace.
            ConvexSubspace const *backSubspace = hedge->hasFace()? &hedge->face().mapElementAs<ConvexSubspace>() : 0;
            if(!backSubspace)
                break;

            SectorCluster const &cluster = backSubspace->cluster();
            if(cluster.hasWorldVolume())
            {
                for(int i = 0; i < cluster.visPlaneCount(); ++i)
                {
                    Plane const &plane = cluster.visPlane(i);

                    if(plane.heightSmoothed() > bottom && plane.heightSmoothed() < top)
                    {
                        ddouble distance = distanceTo(plane.heightSmoothed());
                        if(!haveEvent(distance))
                        {
                            createEvent(distance);

                            // Have we reached the div limit?
                            if(interceptCount() == WALLEDGE_MAX_INTERCEPTS)
                                return;
                        }
                    }

                    // Clip a range bound to this height?
                    if(plane.isSectorFloor() && plane.heightSmoothed() > bottom)
                        bottom = plane.heightSmoothed();
                    else if(plane.isSectorCeiling() && plane.heightSmoothed() < top)
                        top = plane.heightSmoothed();

                    // All clipped away?
                    if(bottom >= top)
                        return;
                }
            }
            else
            {
                /*
                 * A neighbor with zero volume is a special case -- the potential
                 * division is at the height of the back ceiling. This is because
                 * elsewhere we automatically fix the case of a floor above a
                 * ceiling by lowering the floor.
                 */
                coord_t z = cluster.visCeiling().heightSmoothed();

                if(z > bottom && z < top)
                {
                    ddouble distance = distanceTo(z);
                    if(!haveEvent(distance))
                    {
                        createEvent(distance);
                        // All clipped away.
                        return;
                    }
                }
            }
        }
    }

    /**
     * Determines whether the wall edge should be intercepted with neighboring
     * planes from other sector clusters.
     */
    bool shouldInterceptNeighbors()
    {
        if(spec.flags & WallSpec::NoEdgeDivisions)
            return false;

        if(de::fequal(top.z(), bottom.z()))
            return false;

        // Cluster-internal edges won't be intercepted. This is because such an
        // edge only ever produces middle wall sections, which, do not support
        // divisions in any case (they become vissprites).
        if(SectorCluster::isInternalEdge(&owner.hedge()))
            return false;

        return true;
    }

    void prepareEventsIfNeeded()
    {
        if(events) return;

        DENG2_ASSERT(self.isValid());

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
        if(shouldInterceptNeighbors())
        {
            configure(Partition(Vector2d(0, top.z() - bottom.z())));

            // Add intercepts (the "divisions") in ascending distance order.
            addNeighborIntercepts(bottom.z(), top.z());

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

        // Polyobj lines have no owner rings.
        if(lineSide().line().definesPolyobj())
            return 0;

        LineOwner const *farVertOwner = lineSide().line().vertexOwner(lineSide().sideId() ^ owner.side());
        Line *neighbor;
        if(R_SideBackClosed(lineSide()))
        {
            neighbor = R_FindSolidLineNeighbor(lineSide().sectorPtr(), &lineSide().line(),
                                               farVertOwner, owner.side(), &diff);
        }
        else
        {
            neighbor = R_FindLineNeighbor(lineSide().sectorPtr(), &lineSide().line(),
                                          farVertOwner, owner.side(), &diff);
        }

        // No suitable line neighbor?
        if(!neighbor) return 0;

        // Choose the correct side of the neighbor (determined by which vertex is shared).
        LineSide *otherSide;
        if(&neighbor->vertex(owner.side() ^ 1) == &lineSide().vertex(owner.side()))
        {
            otherSide = &neighbor->front();
        }
        else
        {
            otherSide = &neighbor->back();
        }

        // We can only blend if the neighbor has a surface.
        if(!otherSide->hasSections()) return 0;

        /// @todo Do not assume the neighbor is the middle section of @var otherSide.
        return &otherSide->middle();
    }

    /**
     * Determine the (possibly smoothed) edge normal.
     * @todo Cache the smoothed normal value somewhere...
     */
    void updateNormalIfNeeded()
    {
        if(!needUpdateNormal) return;
        needUpdateNormal = false;

        Surface &surface = lineSideSurface();

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

WallEdge::WallSection::WallSection(WallEdge &owner, SectionId id, WallSpec const &spec)
    : d(new Instance(this, owner, id, spec))
{}

WallEdge &WallEdge::WallSection::edge() const
{
    return d->owner;
}

Vector3d const &WallEdge::WallSection::pOrigin() const
{
    d->prepareIfNeeded();
    return d->pOrigin;
}

Vector3d const &WallEdge::WallSection::pDirection() const
{
    d->prepareIfNeeded();
    return d->pDirection;
}

Vector2f WallEdge::WallSection::materialOrigin() const
{
    d->prepareIfNeeded();
    return d->materialOrigin;
}

Vector3f WallEdge::WallSection::normal() const
{
    d->prepareIfNeeded();
    d->updateNormalIfNeeded();
    return d->normal;
}

WallSpec const &WallEdge::WallSection::spec() const
{
    return d->spec;
}

WallEdge::SectionId WallEdge::WallSection::id() const
{
    return d->id;
}

int WallEdge::WallSection::divisionCount() const
{
    d->prepareIfNeeded();
    if(!isValid()) return 0;
    d->prepareEventsIfNeeded();
    return d->interceptCount() - 2;
}

WallEdge::WallSection::EventIndex WallEdge::WallSection::firstDivision() const
{
    return divisionCount()? 1 : InvalidIndex;
}

WallEdge::WallSection::EventIndex WallEdge::WallSection::lastDivision() const
{
    return divisionCount()? d->interceptCount() - 2 : InvalidIndex;
}

WallEdge::WallSection::Events const &WallEdge::WallSection::events() const
{
    d->prepareIfNeeded();
    d->verifyValid();
    d->prepareEventsIfNeeded();
    return *d->events;
}

WallEdge::Event const &WallEdge::WallSection::at(EventIndex index) const
{
    return *events().at(index);
}

bool WallEdge::WallSection::isValid() const
{
    d->prepareIfNeeded();
    return d->top.z() > d->bottom.z();
}

WallEdge::Event const &WallEdge::WallSection::first() const
{
    d->prepareIfNeeded();
    return d->bottom;
}

WallEdge::Event const &WallEdge::WallSection::last() const
{
    d->prepareIfNeeded();
    return d->top;
}

DENG2_PIMPL_NOREF(WallEdge)
{
    HEdge &hedge;
    int side;

    WallSection middle;
    WallSection bottom;
    WallSection top;

    Instance(WallEdge &self, HEdge &hedge, int side, LineSide &lineSide)
        : hedge (hedge)
        , side  (side)
        , middle(self, WallMiddle, WallSpec::fromLineSide(lineSide, LineSide::Middle))
        , bottom(self, WallBottom, WallSpec::fromLineSide(lineSide, LineSide::Bottom))
        , top   (self, WallTop,    WallSpec::fromLineSide(lineSide, LineSide::Top))
    {}
};

WallEdge::WallEdge(HEdge &hedge, int side)
{
    LineSide &lineSide = hedge.mapElementAs<LineSideSegment>().lineSide();
    d.reset(new Instance(*this, hedge, side, lineSide));
}

Vector2d const &WallEdge::origin() const
{
    return (d->side? d->hedge.twin() : d->hedge).origin();
}

LineSide &WallEdge::lineSide() const
{
    return d->hedge.mapElementAs<LineSideSegment>().lineSide();
}

coord_t WallEdge::lineSideOffset() const
{
    LineSideSegment const &seg = d->hedge.mapElementAs<LineSideSegment>();
    return seg.lineSideOffset() + (d->side? seg.length() : 0);
}

HEdge &WallEdge::hedge() const
{
    return d->hedge;
}

int WallEdge::side() const
{
    return d->side;
}

WallEdge::WallSection &WallEdge::section(SectionId id)
{
    switch(id)
    {
    case WallMiddle: return d->middle;
    case WallBottom: return d->bottom;
    case WallTop:    return d->top;
    }
    /// @throw UnknownSectionError We do not know this section.
    throw UnknownSectionError("WallEdge::section", "Invalid id " + String::number(id));
}

WallEdge::SectionId WallEdge::sectionIdFromLineSideSection(int section) //static
{
    switch(section)
    {
    case LineSide::Middle: return WallMiddle;
    case LineSide::Bottom: return WallBottom;
    case LineSide::Top:    return WallTop;
    default: break;
    }
    /// @throw UnknownSectionError We do not know this section.
    throw UnknownSectionError("WallEdge::sectionIdFromLineSideSection", "Unknown line side section " + String::number(section));
}
