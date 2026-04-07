/** @file skyfixedge.cpp  Sky Fix Edge Geometry.
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

#include "render/skyfixedge.h"

#include "world/map.h"
#include "world/maputil.h" // R_SideBackClosed, remove me
#include "world/p_players.h"
#include "world/subsector.h"
#include "world/convexsubspace.h"
#include "world/plane.h"
#include "world/surface.h"
#include "resource/materialanimator.h"

#include "render/rend_main.h"

#include "client/clskyplane.h"

#include <doomsday/mesh/face.h>
#include <doomsday/world/sector.h>

using namespace de;

static double skyFixFloorZ(const Plane *frontFloor, const Plane *backFloor)
{
    DE_UNUSED(backFloor);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontFloor->heightSmoothed();
    return frontFloor->map().skyFloor().height();
}

static double skyFixCeilZ(const Plane *frontCeil, const Plane *backCeil)
{
    DE_UNUSED(backCeil);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontCeil->heightSmoothed();
    return frontCeil->map().skyCeiling().height();
}

DE_PIMPL_NOREF(SkyFixEdge::Event)
{
    SkyFixEdge &owner;
    double distance;
    Impl(SkyFixEdge &owner, double distance)
        : owner(owner), distance(distance)
    {}
};

SkyFixEdge::Event::Event(SkyFixEdge &owner, double distance)
    : d(new Impl(owner, distance))
{}

bool SkyFixEdge::Event::operator < (const Event &other) const
{
    return d->distance < other.distance();
}

double SkyFixEdge::Event::distance() const
{
    return d->distance;
}

Vec3d SkyFixEdge::Event::origin() const
{
    return d->owner.pOrigin() + d->owner.pDirection() * distance();
}

DE_PIMPL(SkyFixEdge)
{
    mesh::HEdge *hedge;
    FixType fixType;
    dint edge;

    Vec3d pOrigin;
    Vec3d pDirection;

    coord_t lo, hi;

    Event bottom;
    Event top;
    bool isValid;

    Vec2f materialOrigin;

    Impl(Public *i, mesh::HEdge &hedge, FixType fixType, int edge,
             const Vec2f &materialOrigin)
        : Base(i),
          hedge(&hedge),
          fixType(fixType),
          edge(edge),
          bottom(*i, 0),
          top(*i, 1),
          isValid(false),
          materialOrigin(materialOrigin)
    {}

    /**
     * Determines whether a sky fix is actually necessary.
     */
    bool wallSectionNeedsSkyFix() const
    {
        DE_ASSERT(hedge->hasFace());

        const bool lower = fixType == SkyFixEdge::Lower;

        // Only edges with line segments need fixes.
        if (!hedge->hasMapElement()) return false;

        const auto &lineSeg  = hedge->mapElementAs<LineSideSegment>();
        const auto &lineSide = lineSeg.lineSide().as<LineSide>();

        const auto *space     = &hedge->face().mapElementAs<ConvexSubspace>();
        const auto *backSpace = hedge->twin().hasFace() ? &hedge->twin().face().mapElementAs<ConvexSubspace>()
                                                        : nullptr;

        const auto *subsec     = &space->subsector().as<Subsector>();
        const auto *backSubsec = backSpace && backSpace->hasSubsector() ? &backSpace->subsector().as<Subsector>()
                                                                        : nullptr;

        if (backSubsec && &backSubsec->sector() == &subsec->sector())
            return false;

        if (lineSide.middle().hasMaterial() &&
            !lineSide.middle().as<Surface>().materialAnimator()->isOpaque() &&
            ((!lower && !lineSide.top().hasMaterial()) ||
             (lower  && !lineSide.bottom().hasMaterial())))
        {
            // Icarus: force fields render hack
            return false;
        }

        // Select the relative planes for the fix type.
        dint relPlane = lower ? world::Sector::Floor : world::Sector::Ceiling;
        const Plane *front   = &subsec->visPlane(relPlane);
        const Plane *back    = backSubsec ? &backSubsec->visPlane(relPlane) : nullptr;

        if (!front->surface().hasSkyMaskedMaterial())
            return false;

        const bool hasClosedBack = R_SideBackClosed(lineSide);

        if (!devRendSkyMode)
        {
            if (!P_IsInVoid(viewPlayer)
                && !(hasClosedBack || !(back && back->surface().hasSkyMaskedMaterial())))
                return false;
        }
        else
        {
            dint relSection = lower ? LineSide::Bottom : LineSide::Top;

            if (lineSide.surface(relSection).hasMaterial()
                || !(hasClosedBack || (back && back->surface().hasSkyMaskedMaterial())))
                return false;
        }

        // Figure out the relative plane heights.
        coord_t fz = front->heightSmoothed();
        if (relPlane == world::Sector::Ceiling)
            fz = -fz;

        coord_t bz = 0;
        if (back)
        {
            bz = back->heightSmoothed();
            if(relPlane == world::Sector::Ceiling)
                bz = -bz;
        }

        coord_t planeZ = (back && back->surface().hasSkyMaskedMaterial() &&
                          fz < bz? bz : fz);

        coord_t skyZ = lower ? skyFixFloorZ(front, back)
                             : -skyFixCeilZ(front, back);

        return (planeZ > skyZ);
    }

    void prepare()
    {
        if (!wallSectionNeedsSkyFix())
        {
            isValid = false;
            return;
        }

        const auto *subspace     = &hedge->face().mapElementAs<ConvexSubspace>();
        const auto *backSubspace = hedge->twin().hasFace() ? &hedge->twin().face().mapElementAs<ConvexSubspace>() : nullptr;

        const auto *subsec       = &subspace->subsector().as<Subsector>();
        const auto *backSubsec   = backSubspace && backSubspace->hasSubsector() ? &backSubspace->subsector().as<Subsector>()
                                                                                : nullptr;

        const Plane *ffloor = &subsec->visFloor();
        const Plane *fceil  = &subsec->visCeiling();
        const Plane *bceil  = backSubsec ? &backSubsec->visCeiling() : nullptr;
        const Plane *bfloor = backSubsec ? &backSubsec->visFloor()   : nullptr;

        if (fixType == Upper)
        {
            hi = skyFixCeilZ(fceil, bceil);
            lo = de::max((backSubsec && bceil->surface().hasSkyMaskedMaterial()) ? bceil->heightSmoothed()
                                                                                 : fceil->heightSmoothed()
                         , ffloor->heightSmoothed());
        }
        else
        {
            hi = de::min((backSubsec && bfloor->surface().hasSkyMaskedMaterial()) ? bfloor->heightSmoothed()
                                                                                  : ffloor->heightSmoothed()
                         , fceil->heightSmoothed());
            lo = skyFixFloorZ(ffloor, bfloor);
        }

        isValid = hi > lo;
        if (!isValid) return;

        pOrigin = Vec3d(self().origin(), lo);
        pDirection = Vec3d(0, 0, hi - lo);
    }
};

SkyFixEdge::SkyFixEdge(mesh::HEdge &hedge, FixType fixType, int edge, float materialOffsetS)
    : WorldEdge((edge? hedge.twin() : hedge).origin()),
      d(new Impl(this, hedge, fixType, edge, Vec2f(materialOffsetS, 0)))
{
    /// @todo Defer until necessary.
    d->prepare();
}

const Vec3d &SkyFixEdge::pOrigin() const
{
    return d->pOrigin;
}

const Vec3d &SkyFixEdge::pDirection() const
{
    return d->pDirection;
}

Vec2f SkyFixEdge::materialOrigin() const
{
    return d->materialOrigin;
}

bool SkyFixEdge::isValid() const
{
    return d->isValid;
}

const SkyFixEdge::Event &SkyFixEdge::first() const
{
    return d->bottom;
}

const SkyFixEdge::Event &SkyFixEdge::last() const
{
    return d->top;
}

const SkyFixEdge::Event &SkyFixEdge::at(EventIndex index) const
{
    if(index >= 0 && index < 2)
    {
        return index == 0? d->bottom : d->top;
    }
    /// @throw InvalidIndexError The specified event index is not valid.
    throw Error("SkyFixEdge::at", stringf("Index '%i' does not map to a known event (count: 2)", index));
}
