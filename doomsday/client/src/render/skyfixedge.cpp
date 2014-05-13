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

#include "de_platform.h"
#include "render/skyfixedge.h"
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

namespace de {

static coord_t skyFixFloorZ(Plane const *frontFloor, Plane const *backFloor)
{
    DENG2_UNUSED(backFloor);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontFloor->heightSmoothed();
    return frontFloor->map().skyFixFloor();
}

static coord_t skyFixCeilZ(Plane const *frontCeil, Plane const *backCeil)
{
    DENG2_UNUSED(backCeil);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontCeil->heightSmoothed();
    return frontCeil->map().skyFixCeiling();
}

SkyFixEdge::Event::Event(double distance)
    : AbstractEdge::Event()
    , _distance(distance)
    , _section (0)
{}

bool SkyFixEdge::Event::operator < (Event const &other) const
{
    return _distance < other.distance();
}

double SkyFixEdge::Event::distance() const
{
    return _distance;
}

Vector3d SkyFixEdge::Event::origin() const
{
    return section().pOrigin() + section().pDirection() * distance();
}

SkyFixEdge::SkySection &SkyFixEdge::Event::section() const
{
    DENG2_ASSERT(_section != 0);
    return *_section;
}

DENG2_PIMPL(SkyFixEdge::SkySection)
{
    SkyFixEdge &owner;
    SectionId id;

    Vector3d pOrigin;
    Vector3d pDirection;

    /// Events for the special termination points are allocated with "this".
    SkyFixEdge::Event bottom;
    SkyFixEdge::Event top;

    /// Edge attributes:
    Vector2f materialOrigin;

    bool needPrepare;

    Instance(Public *i, SkyFixEdge &owner, SectionId id, Vector2f const &materialOrigin)
        : Base(i)
        , owner         (owner)
        , id            (id)
        , bottom        (0)
        , top           (1)
        , materialOrigin(materialOrigin)
        , needPrepare   (true)
    {
        bottom._section = thisPublic;
        top._section    = thisPublic;
    }

    /**
     * Determines whether a sky fix is actually necessary.
     */
    bool wallSectionNeedsSkyFix() const
    {
        HEdge &hedge = owner.hedge();

        DENG2_ASSERT(hedge.hasFace());

        // Only edges with line segments need fixes.
        if(!hedge.hasMapElement()) return false;

        SectorCluster const *cluster     = hedge.face().mapElementAs<ConvexSubspace>().clusterPtr();
        SectorCluster const *backCluster = hedge.twin().hasFace()? hedge.twin().face().mapElementAs<ConvexSubspace>() .clusterPtr() : 0;

        if(backCluster && &backCluster->sector() == &cluster->sector())
            return false;

        // Select the relative planes for the fix type.
        int relPlane = id == SkyBottom? Sector::Floor : Sector::Ceiling;
        Plane const *front   = &cluster->visPlane(relPlane);
        Plane const *back    = backCluster? &backCluster->visPlane(relPlane) : 0;

        if(!front->surface().hasSkyMaskedMaterial())
            return false;

        LineSide const &lineSide = hedge.mapElementAs<LineSideSegment>().lineSide();
        bool const hasClosedBack = R_SideBackClosed(lineSide);

        if(!devRendSkyMode)
        {
            if(!P_IsInVoid(viewPlayer) &&
               !(hasClosedBack || !(back && back->surface().hasSkyMaskedMaterial())))
                return false;
        }
        else
        {
            int relSection = id == SkyBottom? LineSide::Bottom : LineSide::Top;

            if(lineSide.surface(relSection).hasMaterial() ||
               !(hasClosedBack || (back && back->surface().hasSkyMaskedMaterial())))
                return false;
        }

        // Figure out the relative plane heights.
        coord_t fz = front->heightSmoothed();
        if(relPlane == Sector::Ceiling)
            fz = -fz;

        coord_t bz = 0;
        if(back)
        {
            bz = back->heightSmoothed();
            if(relPlane == Sector::Ceiling)
                bz = -bz;
        }

        coord_t planeZ = (back && back->surface().hasSkyMaskedMaterial() &&
                          fz < bz? bz : fz);

        coord_t skyZ = id == SkyBottom? skyFixFloorZ(front, back)
                                      : -skyFixCeilZ(front, back);

        return (planeZ > skyZ);
    }

    void prepareIfNeeded()
    {
        if(!needPrepare) return;
        needPrepare = false;

        coord_t lo = 0, hi = 0;

        if(wallSectionNeedsSkyFix())
        {
            HEdge &hedge = owner.hedge();

            SectorCluster const *cluster     = hedge.face().mapElementAs<ConvexSubspace>().clusterPtr();
            SectorCluster const *backCluster =
                hedge.twin().hasFace()? hedge.twin().face().mapElementAs<ConvexSubspace>().clusterPtr() : 0;

            Plane const *ffloor = &cluster->visFloor();
            Plane const *fceil  = &cluster->visCeiling();
            Plane const *bceil  = backCluster? &backCluster->visCeiling() : 0;
            Plane const *bfloor = backCluster? &backCluster->visFloor()   : 0;

            if(id == SkyTop)
            {
                hi = skyFixCeilZ(fceil, bceil);
                lo = de::max((backCluster && bceil->surface().hasSkyMaskedMaterial())? bceil->heightSmoothed()
                                                                                     : fceil->heightSmoothed(),
                             ffloor->heightSmoothed());
            }
            else
            {
                hi = de::min((backCluster && bfloor->surface().hasSkyMaskedMaterial())? bfloor->heightSmoothed()
                                                                                      : ffloor->heightSmoothed(),
                             fceil->heightSmoothed());
                lo = skyFixFloorZ(ffloor, bfloor);
            }
        }

        pOrigin    = Vector3d(owner.origin(), lo);
        pDirection = Vector3d(0, 0, hi - lo);
    }
};

SkyFixEdge::SkySection::SkySection(SkyFixEdge &owner, SectionId id, float materialOffsetS)
    : AbstractEdge()
    , d(new Instance(this, owner, id, Vector2f(materialOffsetS, 0)))
{}

Vector3d const &SkyFixEdge::SkySection::pOrigin() const
{
    d->prepareIfNeeded();
    return d->pOrigin;
}

Vector3d const &SkyFixEdge::SkySection::pDirection() const
{
    d->prepareIfNeeded();
    return d->pDirection;
}

Vector2f SkyFixEdge::SkySection::materialOrigin() const
{
    //d->prepareIfNeeded();
    return d->materialOrigin;
}

bool SkyFixEdge::SkySection::isValid() const
{
    d->prepareIfNeeded();
    return d->top.z() > d->bottom.z();
}

SkyFixEdge::Event const &SkyFixEdge::SkySection::first() const
{
    d->prepareIfNeeded();
    return d->bottom;
}

SkyFixEdge::Event const &SkyFixEdge::SkySection::last() const
{
    d->prepareIfNeeded();
    return d->top;
}

SkyFixEdge::Event const &SkyFixEdge::SkySection::at(EventIndex index) const
{
    d->prepareIfNeeded();
    if(index >= 0 && index < 2)
    {
        return index == 0? d->bottom : d->top;
    }
    /// @throw InvalidIndexError The specified event index is not valid.
    throw Error("SkyFixEdge::SkySection::at", QString("Index '%1' does not map to a known event (count: 2)").arg(index));
}

DENG2_PIMPL_NOREF(SkyFixEdge)
{
    HEdge &hedge;
    int side;

    SkySection bottom;
    SkySection top;

    Instance(SkyFixEdge &self, HEdge &hedge, int side, float materialOffsetS)
        : hedge (hedge)
        , side  (side)
        , bottom(self, SkyBottom, materialOffsetS)
        , top   (self, SkyTop,    materialOffsetS)
    {}
};

SkyFixEdge::SkyFixEdge(HEdge &hedge, int side, float materialOffsetS)
{
    d.reset(new Instance(*this, hedge, side, materialOffsetS));
}

Vector2d const &SkyFixEdge::origin() const
{
    return(d->side? d->hedge.twin() : d->hedge).origin();
}

LineSide &SkyFixEdge::lineSide() const
{
    return d->hedge.mapElementAs<LineSideSegment>().lineSide();
}

coord_t SkyFixEdge::lineSideOffset() const
{
    LineSideSegment const &seg = d->hedge.mapElementAs<LineSideSegment>();
    return seg.lineSideOffset() + (d->side? seg.length() : 0);
}

HEdge &SkyFixEdge::hedge() const
{
    return d->hedge;
}

int SkyFixEdge::side() const
{
    return d->side;
}

SkyFixEdge::SkySection &SkyFixEdge::section(SectionId id)
{
    switch(id)
    {
    case SkyTop:    return d->top;
    case SkyBottom: return d->bottom;
    }
    /// @throw UnknownSectionError We do not know this section.
    throw UnknownSectionError("SkyFixEdge::section", "Invalid id " + String::number(id));
}

} // namespace de
