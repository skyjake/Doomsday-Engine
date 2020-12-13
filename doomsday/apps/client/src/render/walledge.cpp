/** @file walledge.cpp  Wall Edge Geometry.
 *
 * @authors Copyright © 2011-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "render/walledge.h"

#include "BspLeaf"
#include "ConvexSubspace"
#include "Sector"

#include "world/lineowner.h"
#include "world/p_players.h"
#include "world/maputil.h"
#include "world/surface.h"
#include "client/clientsubsector.h"
#include "MaterialAnimator"
#include "render/rend_main.h"  /// devRendSkyMode @todo remove me

#include "Face"

#include <QtAlgorithms>

using namespace de;
using namespace world;

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

WallEdge::Event::Event()
    : IHPlane::IIntercept(0)
    , _owner(nullptr)
{}

WallEdge::Event::Event(WallEdge &owner, ddouble distance)
    : WorldEdge::Event()
    , IHPlane::IIntercept(distance)
    , _owner(&owner)
{}

WallEdge::Event &WallEdge::Event::operator = (Event const &other)
{
    _owner    = other._owner;
    _distance = other._distance;
    return *this;
}

bool WallEdge::Event::operator < (Event const &other) const
{
    return distance() < other.distance();
}

ddouble WallEdge::Event::distance() const
{
    return IHPlane::IIntercept::distance();
}

Vector3d WallEdge::Event::origin() const
{
    return _owner->pOrigin() + _owner->pDirection() * distance();
}

static inline coord_t lineSideOffset(LineSideSegment &seg, dint edge)
{
    return seg.lineSideOffset() + (edge? seg.length() : 0);
}

QList<WallEdge::Impl *> WallEdge::recycledImpls;

struct WallEdge::Impl : public IHPlane
{
    WallEdge *self = nullptr;

    WallSpec spec;
    dint edge = 0;

    HEdge *wallHEdge = nullptr;

    /// The half-plane which partitions the surface coordinate space.
    Partition hplane;

    Vector3d pOrigin;
    Vector3d pDirection;

    coord_t lo = 0, hi = 0;

    /// Events for the special termination points are allocated with "this".
    Event bottom;
    Event top;

    /**
     * Special-purpose array whose memory is never freed while the array exists.
     * `WallEdge::Impl`s are recycled, so it would be a waste of time to keep 
     * allocating and freeing the event arrays.
     */
    struct EventArray : private QVector<Event>
    {
        using Base = QVector<Event>;

        int size = 0;

    public:
        EventArray() {}

        using Base::at;
        using Base::begin;
        using Base::iterator;
        using Base::const_iterator;

        inline bool isEmpty() const
        {
            return size == 0;
        }

        inline void clear()
        {
            size = 0;
        }

        void append(Event const &event)
        {
            if (size < Base::size())
            {
                (*this)[size++] = event;
            }
            else
            {
                Base::append(event);
                ++size;
            }
        }

        inline Event &last()
        {
            return (*this)[size - 1];
        }

        inline Base::iterator end()
        {
            return Base::begin() + size;
        }

        inline Base::const_iterator end() const
        {
            return Base::begin() + size;
        }
    };

    /// All events along the partition line.
    EventArray events;
    bool needSortEvents = false;

    Vector2f materialOrigin;

    Vector3f normal;
    bool needUpdateNormal = true;

    Impl() {}

    void deinit()
    {
        self = nullptr;
        edge = 0;
        wallHEdge = nullptr;
        lo = hi = 0;
        events.clear();
        needSortEvents = false;
        needUpdateNormal = true;
    }

    void init(WallEdge *i, WallSpec const &wallSpec, HEdge &hedge, dint edge)
    {
        self = i;

        spec       = wallSpec;
        this->edge = edge;
        wallHEdge  = &hedge;
        bottom     = Event(*self, 0);
        top        = Event(*self, 1);

        // Determine the map space Z coordinates of the wall section.
        LineSideSegment &seg   = lineSideSegment();
        Line const &line       = seg.line();
        bool const unpegBottom = (line.flags() & DDLF_DONTPEGBOTTOM) != 0;
        bool const unpegTop    = (line.flags() & DDLF_DONTPEGTOP)    != 0;

        ConvexSubspace const &space = (line.definesPolyobj() ? line.polyobj().bspLeaf().subspace()
                                                             : wallHEdge->face().mapElementAs<world::ConvexSubspace>());
        auto const &subsec = space.subsector().as<world::ClientSubsector>();

        /*
         * For reference, see "Texture aligment" in Doomwiki.org:
         * https://doomwiki.org/wiki/Texture_alignment
         */

        if (seg.lineSide().considerOneSided()
            || // Mapping errors may result in a line segment missing a back face.
               (!line.definesPolyobj() && !wallHEdge->twin().hasFace()))
        {
            if (spec.section == LineSide::Middle)
            {
                lo = subsec.visFloor().heightSmoothed();
                hi = subsec.visCeiling().heightSmoothed();
            }
            else
            {
                lo = hi = subsec.visFloor().heightSmoothed();
            }

            materialOrigin = seg.lineSide().middle().originSmoothed();
            if (unpegBottom)
            {
                materialOrigin.y -= hi - lo;
            }
        }
        else
        {
            // Two sided.
            auto const &backSubsec =
                line.definesPolyobj() ? subsec
                                      : wallHEdge->twin().face().mapElementAs<world::ConvexSubspace>()
                                            .subsector().as<world::ClientSubsector>();

            Plane const *ffloor = &subsec.visFloor();
            Plane const *fceil  = &subsec.visCeiling();
            Plane const *bfloor = &backSubsec.visFloor();
            Plane const *bceil  = &backSubsec.visCeiling();

            switch (spec.section)
            {
            case LineSide::Top:
                // Self-referencing lines only ever get a middle.
                if (!line.isSelfReferencing())
                {
                    // Can't go over front ceiling (would induce geometry flaws).
                    if (bceil->heightSmoothed() < ffloor->heightSmoothed())
                        lo = ffloor->heightSmoothed();
                    else
                        lo = bceil->heightSmoothed();

                    hi = fceil->heightSmoothed();

                    if (spec.flags.testFlag(WallSpec::SkyClip)
                        && fceil->surface().hasSkyMaskedMaterial()
                        && bceil->surface().hasSkyMaskedMaterial())
                    {
                        hi = lo;
                    }

                    materialOrigin = seg.lineSide().middle().originSmoothed();
                    if (!unpegTop)
                    {
                        // Align with normal middle texture.
                        materialOrigin.y -= fceil->heightSmoothed() - bceil->heightSmoothed();
                    }
                }
                break;

            case LineSide::Bottom:
                // Self-referencing lines only ever get a middle.
                if (!line.isSelfReferencing())
                {
                    const double bceilZ  = bceil->heightSmoothed();
                    const double bfloorZ = bfloor->heightSmoothed();
                    const double fceilZ  = fceil->heightSmoothed();
                    const double ffloorZ = ffloor->heightSmoothed();

                    const bool raiseToBackFloor =
                        (   fceil->surface().hasSkyMaskedMaterial()
                         && bceil->surface().hasSkyMaskedMaterial()
                         && fceilZ < bceilZ
                         && bfloorZ > fceilZ);

                    coord_t t = bfloorZ;

                    lo = ffloorZ;

                    // Can't go over the back ceiling, would induce polygon flaws.
                    if (bfloorZ > bceilZ)
                        t = bceilZ;

                    // Can't go over front ceiling, would induce polygon flaws.
                    // In the special case of a sky masked upper we must extend the bottom
                    // section up to the height of the back floor.
                    if (t > fceilZ && !raiseToBackFloor)
                        t = fceilZ;

                    hi = t;

                    if (spec.flags.testFlag(WallSpec::SkyClip)
                        && ffloor->surface().hasSkyMaskedMaterial()
                        && bfloor->surface().hasSkyMaskedMaterial())
                    {
                        lo = hi;
                    }

                    materialOrigin = seg.lineSide().bottom().originSmoothed();
                    if (bfloor->heightSmoothed() > fceil->heightSmoothed())
                    {
                        materialOrigin.y -= (raiseToBackFloor? t : fceilZ)
                                          - bfloorZ;
                    }

                    if (unpegBottom)
                    {
                        // Align with normal middle texture.
                        materialOrigin.y += (raiseToBackFloor ? t : de::max(fceilZ, bceilZ)) - bfloorZ;
                    }
                }
                break;

            case LineSide::Middle: {
                LineSide const &lineSide = seg.lineSide();
                Surface const &middle    = lineSide.middle();

                const bool isExtendedMasked = middle.hasMaterial() &&
                                              !middle.materialAnimator()->isOpaque() &&
                                              !lineSide.top().hasMaterial() &&
                                              lineSide.sector().ceiling().surface().material().isSkyMasked();

                if (!line.isSelfReferencing() && ffloor == &subsec.sector().floor())
                {
                    lo = de::max(bfloor->heightSmoothed(), ffloor->heightSmoothed());
                }
                else
                {
                    // Use the unmapped heights for positioning purposes.
                    lo = lineSide.sector().floor().heightSmoothed();
                }

                if (!line.isSelfReferencing() && fceil == &subsec.sector().ceiling())
                {
                    hi = de::min(bceil->heightSmoothed(), fceil->heightSmoothed());
                }
                else
                {
                    // Use the unmapped heights for positioning purposes.
                    hi = lineSide.back().sector().ceiling().heightSmoothed();
                }

                materialOrigin = Vector2f(middle.originSmoothed().x, 0);

                // Perform clipping.
                if (middle.hasMaterial()
                    && !seg.lineSide().isFlagged(SDF_MIDDLE_STRETCH))
                {
                    coord_t openBottom, openTop;
                    if (!line.isSelfReferencing())
                    {
                        openBottom = lo;
                        openTop    = hi;
                    }
                    else
                    {
                        openBottom = ffloor->heightSmoothed();
                        openTop    = fceil->heightSmoothed();
                    }

                    if (openTop > openBottom)
                    {
                        if (unpegBottom)
                        {
                            lo += middle.originSmoothed().y;
                            hi = lo + middle.material().height();
                        }
                        else
                        {
                            hi += middle.originSmoothed().y;
                            lo = hi - middle.material().height();
                        }

                        if (hi > openTop)
                        {
                            materialOrigin.y = hi - openTop;
                        }

                        // Clip it?
                        bool const clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                        bool const clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());
                        if (clipTop || clipBottom)
                        {
                            if (clipBottom && lo < openBottom)
                                lo = openBottom;

                            if (clipTop && hi > openTop)
                                hi = openTop;
                        }

                        if (!clipTop)
                        {
                            materialOrigin.y = 0;
                        }
                    }
                }

                // Icarus map01: force fields use a masked middle texture that expands above the sector
                if (isExtendedMasked)
                {
                    if (hi - lo < middle.material().height() + middle.originSmoothed().y)
                    {
                        hi = lo + middle.material().height() + middle.originSmoothed().y;
                    }
                }

                break; }
            }
        }
        materialOrigin += Vector2f(::lineSideOffset(seg, edge), 0);

        pOrigin    = Vector3d(self->origin(), lo);
        pDirection = Vector3d(0, 0, hi - lo);
    }

    inline LineSideSegment &lineSideSegment()
    {
        return wallHEdge->mapElementAs<LineSideSegment>();
    }

    void verifyValid() const
    {
        if(!self->isValid())
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw InvalidError("WallEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    EventIndex toEventIndex(ddouble distance) const
    {
        for (EventIndex i = 0; i < events.size; ++i)
        {
            if (de::fequal(events.at(i).distance(), distance))
                return i;
        }
        return InvalidIndex;
    }

    inline bool haveEvent(ddouble distance) const
    {
        return toEventIndex(distance) != InvalidIndex;
    }

    Event &createEvent(ddouble distance)
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
    Event *intercept(ddouble distance)
    {
        events.append(Event(*self, distance));

        // We'll need to resort the events.
        needSortEvents = true;

        return &events.last();
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        if (needSortEvents)
        {
            qSort(events.begin(), events.end(), [] (WorldEdge::Event const &a, WorldEdge::Event const &b) {
                return a < b;
            });
            needSortEvents = false;
        }
    }

    // Implements IHPlane
    void clearIntercepts()
    {
        events.clear();

        // An empty event list is logically sorted.
        needSortEvents = false;
    }

    // Implements IHPlane
    Event const &at(EventIndex index) const
    {
        if(index >= 0 && index < interceptCount())
        {
            return events.at(index);
        }
        /// @throw UnknownInterceptError The specified intercept index is not valid.
        throw UnknownInterceptError("WallEdge::at", String("Index '%1' does not map to a known intercept (count: %2)")
                                                        .arg(index).arg(interceptCount()));
    }

    // Implements IHPlane
    dint interceptCount() const
    {
        return events.size;
    }

#ifdef DENG2_DEBUG
    void printIntercepts() const
    {
        EventIndex index = 0;
        foreach (Event const &icpt, events)
        {
            LOGDEV_MAP_MSG(" %u: >%1.2f ") << (index++) << icpt.distance();
        }
    }
#endif

    /**
     * Ensure all intercepts do not exceed the specified closed range.
     */
    void assertInterceptsInRange(ddouble low, ddouble hi) const
    {
#ifdef DENG2_DEBUG
        foreach (Event const &icpt, events)
        {
            DENG2_ASSERT(icpt.distance() >= low && icpt.distance() <= hi);
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    inline ddouble distanceTo(coord_t worldHeight) const
    {
        return (worldHeight - lo) / (hi - lo);
    }

    void addNeighborIntercepts(ddouble bottom, ddouble top)
    {
        ClockDirection const direction = edge ? Clockwise : Anticlockwise;

        HEdge const *hedge = wallHEdge;
        while ((hedge = &SubsectorCirculator::findBackNeighbor(*hedge, direction)) != wallHEdge)
        {
            // Stop if there is no space on the back side.
            if (!hedge->hasFace() || !hedge->hasMapElement())
                break;

            auto const &backSpace = hedge->face().mapElementAs<ConvexSubspace>();
            auto const &subsec    = backSpace.subsector().as<world::ClientSubsector>();

            if (subsec.hasWorldVolume())
            {
                for (dint i = 0; i < subsec.visPlaneCount(); ++i)
                {
                    Plane const &plane = subsec.visPlane(i);

                    if (plane.heightSmoothed() > bottom && plane.heightSmoothed() < top)
                    {
                        ddouble distance = distanceTo(plane.heightSmoothed());
                        if (!haveEvent(distance))
                        {
                            createEvent(distance);

                            // Have we reached the div limit?
                            if (interceptCount() == WALLEDGE_MAX_INTERCEPTS)
                                return;
                        }
                    }

                    // Clip a range bound to this height?
                    if (plane.isSectorFloor() && plane.heightSmoothed() > bottom)
                        bottom = plane.heightSmoothed();
                    else if (plane.isSectorCeiling() && plane.heightSmoothed() < top)
                        top = plane.heightSmoothed();

                    // All clipped away?
                    if (bottom >= top)
                        return;
                }
            }
            else
            {
                // A neighbor with zero volume -- the potential division is at the height
                // of the back ceiling. This is because elsewhere we automatically fix the
                // case of a floor above a ceiling by lowering the floor.
                ddouble z = subsec.visCeiling().heightSmoothed();
                if(z > bottom && z < top)
                {
                    ddouble distance = distanceTo(z);
                    if(!haveEvent(distance))
                    {
                        createEvent(distance);
                        return; // All clipped away.
                    }
                }
            }
        }
    }

    /**
     * Determines whether the wall edge should be intercepted with neighboring
     * planes from other subsectors.
     */
    bool shouldInterceptNeighbors()
    {
        if(spec.flags & WallSpec::NoEdgeDivisions)
            return false;

        if(de::fequal(hi, lo))
            return false;

        // Subsector-internal edges won't be intercepted. This is because such an
        // edge only ever produces middle wall sections, which, do not support
        // divisions in any case (they become vissprites).
        if(Subsector::isInternalEdge(wallHEdge))
            return false;

        return true;
    }

    void prepareEvents()
    {
        DENG2_ASSERT(self->isValid());

        needSortEvents = false;

        // The first event is the bottom termination event.
        // The last event is the top termination event.
        events.append(bottom);
        events.append(top);

        // Add intecepts for neighbor planes?
        if(shouldInterceptNeighbors())
        {
            configure(Partition(Vector2d(0, hi - lo)));

            // Add intercepts (the "divisions") in ascending distance order.
            addNeighborIntercepts(lo, hi);

            // Sorting may be required. This shouldn't take too long...
            // There seldom are more than two or three intercepts.
            sortAndMergeIntercepts();
        }

#ifdef DENG2_DEBUG
        // Sanity check.
        assertInterceptsInRange(0, 1);
#endif
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
            return nullptr;

        LineSide const &lineSide = lineSideSegment().lineSide();

        // Polyobj lines have no owner rings.
        if(lineSide.line().definesPolyobj())
            return nullptr;

        ClockDirection const direction = (edge? Anticlockwise : Clockwise);
        LineOwner const &farVertOwner  = *lineSide.line().vertexOwner(lineSide.sideId() ^ edge);
        Line *neighbor;
        if(R_SideBackClosed(lineSide))
        {
            neighbor = R_FindSolidLineNeighbor(lineSide.line(), farVertOwner, direction,
                                               lineSide.sectorPtr(), &diff);
        }
        else
        {
            neighbor = R_FindLineNeighbor(lineSide.line(), farVertOwner, direction,
                                          lineSide.sectorPtr(), &diff);
        }

        // No suitable line neighbor?
        if(!neighbor) return nullptr;

        // Choose the correct side of the neighbor (determined by which vertex is shared).
        LineSide *otherSide;
        if(&neighbor->vertex(edge ^ 1) == &lineSide.vertex(edge))
            otherSide = &neighbor->front();
        else
            otherSide = &neighbor->back();

        // We can only blend if the neighbor has a surface.
        if(!otherSide->hasSections()) return nullptr;

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
    : WorldEdge((edge? hedge.twin() : hedge).origin())
    , d(getRecycledImpl())
{
    d->init(this, spec, hedge, edge);
}

WallEdge::~WallEdge()
{
    recycleImpl(d);
}

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

LineSideSegment &WallEdge::lineSideSegment() const
{
    return d->lineSideSegment();
}

coord_t WallEdge::lineSideOffset() const
{
    return ::lineSideOffset(d->lineSideSegment(), d->edge);
}

dint WallEdge::divisionCount() const
{
    if (!isValid()) return 0;
    if (d->events.isEmpty())
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
    return divisionCount()? (d->interceptCount() - 2) : InvalidIndex;
}

WallEdge::Event const &WallEdge::at(EventIndex index) const
{
    return d->events.at(index);
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

WallEdge::Impl *WallEdge::getRecycledImpl() // static
{
    if (recycledImpls.isEmpty())
    {
        return new Impl;
    }
    return recycledImpls.takeLast();
}

void WallEdge::recycleImpl(Impl *d) // static
{
    d->deinit();
    recycledImpls.append(d);
}
