/** @file render/skyfixedge.cpp Sky Fix Edge Geometry.
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

#include "HEdge"
#include "Plane"
#include "Sector"

#include "map/gamemap.h"
#include "map/p_players.h"
#include "map/r_world.h"

#include "render/rend_main.h"

#include "render/skyfixedge.h"

namespace de {

static coord_t skyFixFloorZ(Plane const *frontFloor, Plane const *backFloor)
{
    DENG_UNUSED(backFloor);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontFloor->visHeight();
    return theMap->skyFixFloor();
}

static coord_t skyFixCeilZ(Plane const *frontCeil, Plane const *backCeil)
{
    DENG_UNUSED(backCeil);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontCeil->visHeight();
    return theMap->skyFixCeiling();
}

DENG2_PIMPL_NOREF(SkyFixEdge::Intercept)
{
    SkyFixEdge &owner;
    coord_t distance;

    Instance(SkyFixEdge &owner, coord_t distance)
        : owner(owner), distance(distance)
    {}
};

SkyFixEdge::Intercept::Intercept(SkyFixEdge &owner, coord_t distance)
    : d(new Instance(owner, distance))
{}

bool SkyFixEdge::Intercept::operator < (Intercept const &other) const
{
    return d->distance < other.distance();
}

coord_t SkyFixEdge::Intercept::distance() const
{
    return d->distance;
}

void SkyFixEdge::Intercept::setDistance(coord_t distance)
{
    d->distance = distance;
}

Vector3d SkyFixEdge::Intercept::origin() const
{
    return Vector3d(d->owner.origin(), d->distance);
}

DENG2_PIMPL(SkyFixEdge)
{
    HEdge *hedge;
    FixType fixType;
    int edge;

    Intercept bottom;
    Intercept top;
    Vector2f materialOrigin;
    bool isValid;

    Instance(Public *i, HEdge &hedge, FixType fixType, int edge, float materialOffsetS)
        : Base(i),
          hedge(&hedge),
          fixType(fixType),
          edge(edge),
          bottom(*i),
          top(*i),
          materialOrigin(materialOffsetS, 0),
          isValid(false)
    {}

    /**
     * Determines whether a sky fix is actually necessary.
     */
    bool wallSectionNeedsSkyFix() const
    {
        DENG_ASSERT(hedge->hasBspLeaf());

        bool const lower = fixType == SkyFixEdge::Lower;

        // Partition line segments have no map line sides.
        if(!hedge->hasLineSide()) return false;

        Sector const *frontSec = hedge->bspLeaf().sectorPtr();
        Sector const *backSec  = hedge->twin().hasBspLeaf() && !hedge->twin().bspLeaf().isDegenerate()? hedge->twin().bspLeaf().sectorPtr() : 0;

        if(!(!backSec || backSec != frontSec)) return false;

        // Select the relative planes for the fix type.
        Plane::Type relPlane = lower? Plane::Floor : Plane::Ceiling;
        Plane const *front   = &frontSec->plane(relPlane);
        Plane const *back    = backSec? &backSec->plane(relPlane) : 0;

        if(!front->surface().hasSkyMaskedMaterial())
            return false;

        bool const hasClosedBack = R_SideBackClosed(hedge->lineSide());

        if(!devRendSkyMode)
        {
            if(!P_IsInVoid(viewPlayer) &&
               !(hasClosedBack || !(back && back->surface().hasSkyMaskedMaterial())))
                return false;
        }
        else
        {
            int relSection = lower? Line::Side::Bottom : Line::Side::Top;

            if(hedge->lineSide().surface(relSection).hasMaterial() ||
               !(hasClosedBack || (back && back->surface().hasSkyMaskedMaterial())))
                return false;
        }

        coord_t fz, bz;
        R_SetRelativeHeights(frontSec, backSec, relPlane, &fz, &bz);

        coord_t planeZ = (back && back->surface().hasSkyMaskedMaterial() &&
                          fz < bz? bz : fz);

        coord_t skyZ = lower? skyFixFloorZ(front, back)
                            : -skyFixCeilZ(front, back);

        return (planeZ > skyZ);
    }
};

SkyFixEdge::SkyFixEdge(HEdge &hedge, FixType fixType, int edge, float materialOffsetS)
    : d(new Instance(this, hedge, fixType, edge, materialOffsetS))
{}

void SkyFixEdge::prepare()
{
    if(!d->wallSectionNeedsSkyFix())
    {
        d->isValid = false;
        return;
    }

    Sector const *frontSec = d->hedge->bspLeaf().sectorPtr();
    Sector const *backSec  = d->hedge->twin().hasBspLeaf() && !d->hedge->twin().bspLeaf().isDegenerate()? d->hedge->twin().bspLeaf().sectorPtr() : 0;
    Plane const *ffloor = &frontSec->floor();
    Plane const *fceil  = &frontSec->ceiling();
    Plane const *bceil  = backSec? &backSec->ceiling() : 0;
    Plane const *bfloor = backSec? &backSec->floor()   : 0;

    d->bottom.setDistance(0);
    d->top.setDistance(0);

    if(d->fixType == Upper)
    {
        d->top.setDistance(skyFixCeilZ(fceil, bceil));
        d->bottom.setDistance(de::max((backSec && bceil->surface().hasSkyMaskedMaterial() )? bceil->visHeight() : fceil->visHeight(),  ffloor->visHeight()));
    }
    else
    {
        d->top.setDistance(de::min((backSec && bfloor->surface().hasSkyMaskedMaterial())? bfloor->visHeight() : ffloor->visHeight(), fceil->visHeight()));
        d->bottom.setDistance(skyFixFloorZ(ffloor, bfloor));
    }

    d->isValid = d->bottom < d->top;
}

bool SkyFixEdge::isValid() const
{
    return d->isValid;
}

Vector2d const &SkyFixEdge::origin() const
{
    return d->edge? d->hedge->twin().origin() : d->hedge->origin();
}

SkyFixEdge::Intercept const &SkyFixEdge::from() const
{
    return d->bottom;
}

SkyFixEdge::Intercept const &SkyFixEdge::to() const
{
    return d->top;
}

Vector2f const &SkyFixEdge::materialOrigin() const
{
    return d->materialOrigin;
}

} // namespace de
