/** @file render/shadowedge.cpp FakeRadio Shadow Edge Geometry
 *
 * @authors Copyright &copy; 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "map/r_world.h"
#include "map/lineowner.h"

#include "render/rend_main.h"

#include "render/shadowedge.h"

namespace de {

DENG2_PIMPL_NOREF(ShadowEdge)
{
    HEdge *leftMostHEdge;
    int edge;

    Vector3d inner;
    Vector3d outer;
    float sectorOpenness;
    float openness;

    Instance(HEdge &leftMostHEdge, int edge)
        : leftMostHEdge(&leftMostHEdge),
          edge(edge),
          sectorOpenness(0),
          openness(0)
    {}
};

ShadowEdge::ShadowEdge(HEdge &leftMostHEdge, int edge)
    : d(new Instance(leftMostHEdge, edge))
{}

static void setRelativeHeights(Sector const *front, Sector const *back, int planeIndex,
    coord_t *fz, coord_t *bz, coord_t *bhz)
{
    if(fz)
    {
        *fz = front->plane(planeIndex).visHeight();
        if(planeIndex != Plane::Floor)
            *fz = -(*fz);
    }
    if(bz)
    {
        *bz = back->plane(planeIndex).visHeight();
        if(planeIndex != Plane::Floor)
            *bz = -(*bz);
    }
    if(bhz)
    {
        int otherPlaneIndex = planeIndex == Plane::Floor? Plane::Ceiling : Plane::Floor;
        *bhz = back->plane(otherPlaneIndex).visHeight();
        if(planeIndex != Plane::Floor)
            *bhz = -(*bhz);
    }
}

/**
 * Returns a value in the range of 0..2, representing how 'open' the edge is.
 *
 * @c =0 Completely closed, it is facing a wall or is relatively distant from
 *       the edge on the other side.
 * @c >0 && <1 How near the 'other' edge is.
 * @c =1 At the same height as "this" one.
 * @c >1 The 'other' edge is past our height (clearly 'open').
 */
static float opennessFactor(float fz, float bz, float bhz)
{
    if(fz <= bz - SHADOWEDGE_OPEN_THRESHOLD || fz >= bhz)
        return 0; // Fully closed.

    if(fz >= bhz - SHADOWEDGE_OPEN_THRESHOLD)
        return (bhz - fz) / SHADOWEDGE_OPEN_THRESHOLD;

    if(fz <= bz)
        return 1 - (bz - fz) / SHADOWEDGE_OPEN_THRESHOLD;

    if(fz <= bz + SHADOWEDGE_OPEN_THRESHOLD)
        return 1 + (fz - bz) / SHADOWEDGE_OPEN_THRESHOLD;

    // Fully open!
    return 2;
}

static bool middleMaterialCoversOpening(Line::Side &side)
{
    if(!side.hasSector()) return false; // Never.

    if(!side.hasSections()) return false;
    if(!side.middle().hasMaterial()) return false;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

    if(ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        coord_t openRange, openBottom, openTop;

        // Stretched middles always cover the opening.
        if(side.isFlagged(SDF_MIDDLE_STRETCH))
            return true;

        // Might the material cover the opening?
        openRange = R_VisOpenRange(side, &openBottom, &openTop);
        if(ms.height() >= openRange)
        {
            // Possibly; check the placement.
            coord_t bottom, top;
            R_SideSectionCoords(side, Line::Side::Middle, &bottom, &top);
            return (top > bottom && top >= openTop && bottom <= openBottom);
        }
    }

    return false;
}

void ShadowEdge::prepare(int planeIndex)
{
    Line::Side &side = d->leftMostHEdge->lineSide();
    Plane const &plane = side.sector().plane(planeIndex);
    int const otherPlaneIndex = planeIndex == Plane::Floor? Plane::Ceiling : Plane::Floor;

    d->sectorOpenness = 0; // Default is fully closed.
    d->openness = 0; // Default is fully closed.

    // Determine the 'openness' of the wall edge sector. If the sector is open,
    // there won't be a shadow at all. Open neighbor sectors cause some changes
    // in the polygon corner vertices (placement, opacity).

    if(d->leftMostHEdge->hasTwin() && d->leftMostHEdge->twin().bspLeafSectorPtr() != 0 &&
       d->leftMostHEdge->twin().hasLineSide() && d->leftMostHEdge->twin().lineSide().hasSections())
    {
        Surface const &wallEdgeSurface = side.surface(planeIndex == Plane::Ceiling? Line::Side::Top : Line::Side::Bottom);

        Sector const *frontSec = d->leftMostHEdge->bspLeafSectorPtr();
        Sector const *backSec  = d->leftMostHEdge->twin().bspLeafSectorPtr();

        coord_t fz = 0, bz = 0, bhz = 0;
        setRelativeHeights(frontSec, backSec, planeIndex, &fz, &bz, &bhz);

        if(fz < bz && !wallEdgeSurface.hasMaterial())
        {
            d->sectorOpenness = 2; // Consider it fully open.
        }
        // Is the back sector a closed yet sky-masked surface?
        else if(frontSec->floor().visHeight() >= backSec->ceiling().visHeight() &&
                frontSec->planeSurface(otherPlaneIndex).hasSkyMaskedMaterial() &&
                backSec->planeSurface(otherPlaneIndex).hasSkyMaskedMaterial())
        {
            d->sectorOpenness = 2; // Consider it fully open.
        }
        else
        {
            // Does the middle material completely cover the open range (we do
            // not want to give away the location of any secret areas)?
            if(!middleMaterialCoversOpening(side))
            {
                d->sectorOpenness = opennessFactor(fz, bz, bhz);
            }
        }
    }

    // Only calculate the remaining values when the edge is at least partially open.
    if(d->sectorOpenness >= 1) return;

    // Find the neighbor of this wall section and determine the relative
    // 'openness' of it's plane heights vs those of "this" wall section.

    LineOwner *vo = side.line().vertexOwner(side.lineSideId() ^ d->edge)->_link[d->edge ^ 1];
    Line *neighbor = &vo->line();

    if(neighbor == &side.line())
    {
        d->openness = 1; // Fully open.
    }
    else if(neighbor->isSelfReferencing()) /// @todo Skip over these? -ds
    {
        d->openness = 1;
    }
    else
    {
        // Choose the correct side of the neighbor (determined by which vertex is shared).
        int x = side.lineSideId() ^ d->edge;
        Line::Side *otherSide = &neighbor->side(&side.line().vertex(x) == &neighbor->from()? d->edge ^ 1 : d->edge);

        if(!otherSide->hasSections() && otherSide->back().hasSector())
        {
            // A one-way window, open side.
            d->openness = 1;
        }
        else if(!otherSide->hasSector() ||
                (otherSide->back().hasSector() && middleMaterialCoversOpening(*otherSide)))
        {
            d->openness = 0;
        }
        else if(otherSide->back().hasSector())
        {
            // Its a normal neighbor.
            Sector const *backSec = otherSide->back().sectorPtr();
            if(backSec != d->leftMostHEdge->bspLeafSectorPtr() &&
               !((plane.type() == Plane::Floor && backSec->ceiling().visHeight() <= plane.visHeight()) ||
                 (plane.type() == Plane::Ceiling && backSec->floor().height() >= plane.visHeight())))
            {
                Sector const *frontSec = d->leftMostHEdge->bspLeafSectorPtr();

                coord_t fz = 0, bz = 0, bhz = 0;
                setRelativeHeights(frontSec, backSec, planeIndex, &fz, &bz, &bhz);

                d->openness = opennessFactor(fz, bz, bhz);
            }
        }
    }

    if(d->openness < 1)
    {
        vo = side.line().vertexOwner(side.lineSideId() ^ d->edge);
        if(d->edge) vo = &vo->prev();

        d->inner = Vector3d(side.vertex(d->edge).origin() + vo->innerShadowOffset(),
                            plane.visHeight());
    }
    else
    {
        d->inner = Vector3d(side.vertex(d->edge).origin() + vo->extendedShadowOffset(),
                            plane.visHeight());
    }

    d->outer = Vector3d(side.vertex(d->edge).origin(), plane.visHeight());
}

Vector3d const &ShadowEdge::inner() const
{
    return d->inner;
}

Vector3d const &ShadowEdge::outer() const
{
    return d->outer;
}

float ShadowEdge::openness() const
{
    return d->openness;
}

float ShadowEdge::sectorOpenness() const
{
    return d->sectorOpenness;
}

} // namespace de
