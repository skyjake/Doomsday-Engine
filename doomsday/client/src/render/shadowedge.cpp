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

#include "Face"
#include "HEdge"

#include "BspLeaf"
#include "Plane"
#include "Sector"
#include "Surface"

#include "world/lineowner.h"

#include "render/rend_main.h"
#include "WallEdge"

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

/// @todo fixme: Should use the visual plane heights of sector clusters.
static bool middleMaterialCoversOpening(LineSide const &side)
{
    if(!side.hasSector()) return false; // Never.

    if(!side.hasSections()) return false;
    if(!side.middle().hasMaterial()) return false;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

    // Might the material cover the opening?
    if(ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        // Stretched middles always cover the opening.
        if(side.isFlagged(SDF_MIDDLE_STRETCH))
            return true;

        Sector const &frontSec = side.sector();
        Sector const *backSec  = side.back().sectorPtr();

        // Determine the opening between the visual sector planes at this edge.
        coord_t openBottom;
        if(backSec && backSec->floor().heightSmoothed() > frontSec.floor().heightSmoothed())
        {
            openBottom = backSec->floor().heightSmoothed();
        }
        else
        {
            openBottom = frontSec.floor().heightSmoothed();
        }

        coord_t openTop;
        if(backSec && backSec->ceiling().heightSmoothed() < frontSec.ceiling().heightSmoothed())
        {
            openTop = backSec->ceiling().heightSmoothed();
        }
        else
        {
            openTop = frontSec.ceiling().heightSmoothed();
        }

        if(ms.height() >= openTop - openBottom)
        {
            // Possibly; check the placement.
            if(side.leftHEdge()) // possibility of degenerate BSP leaf
            {
                WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                              *side.leftHEdge(), Line::From);
                return (edge.isValid() && edge.top().z() > edge.bottom().z()
                        && edge.top().z() >= openTop && edge.bottom().z() <= openBottom);
            }
        }
    }

    return false;
}

void ShadowEdge::prepare(int planeIndex)
{
    int const otherPlaneIndex = planeIndex == Sector::Floor? Sector::Ceiling : Sector::Floor;
    HEdge const &hedge = *d->leftMostHEdge;
    SectorCluster const &cluster = hedge.face().mapElementAs<BspLeaf>().cluster();
    Plane const &plane = cluster.visPlane(planeIndex);

    LineSide const &lineSide = hedge.mapElementAs<LineSideSegment>().lineSide();

    d->sectorOpenness = 0; // Default is fully closed.
    d->openness = 0; // Default is fully closed.

    // Determine the 'openness' of the wall edge sector. If the sector is open,
    // there won't be a shadow at all. Open neighbor sectors cause some changes
    // in the polygon corner vertices (placement, opacity).

    if(hedge.twin().hasFace() &&
       hedge.twin().face().mapElementAs<BspLeaf>().hasCluster())
    {
        SectorCluster const &backCluster = hedge.twin().face().mapElementAs<BspLeaf>().cluster();
        Plane const &backPlane = backCluster.visPlane(planeIndex);
        Surface const &wallEdgeSurface =
            lineSide.back().hasSector()? lineSide.surface(planeIndex == Sector::Ceiling? LineSide::Top : LineSide::Bottom)
                                       : lineSide.middle();

        // Figure out the relative plane heights.
        coord_t fz = plane.heightSmoothed();
        if(planeIndex == Sector::Ceiling)
            fz = -fz;

        coord_t bz = backPlane.heightSmoothed();
        if(planeIndex == Sector::Ceiling)
            bz = -bz;

        coord_t bhz = backCluster.plane(otherPlaneIndex).heightSmoothed();
        if(planeIndex == Sector::Ceiling)
            bhz = -bhz;

        // Determine openness.
        if(fz < bz && !wallEdgeSurface.hasMaterial())
        {
            d->sectorOpenness = 2; // Consider it fully open.
        }
        // Is the back sector a closed yet sky-masked surface?
        else if(cluster.visFloor().heightSmoothed() >= backCluster.visCeiling().heightSmoothed() &&
                cluster.visPlane(otherPlaneIndex).surface().hasSkyMaskedMaterial() &&
                backCluster.visPlane(otherPlaneIndex).surface().hasSkyMaskedMaterial())
        {
            d->sectorOpenness = 2; // Consider it fully open.
        }
        else
        {
            // Does the middle material completely cover the open range (we do
            // not want to give away the location of any secret areas)?
            if(!middleMaterialCoversOpening(lineSide))
            {
                d->sectorOpenness = opennessFactor(fz, bz, bhz);
            }
        }
    }

    // Only calculate the remaining values when the edge is at least partially open.
    if(d->sectorOpenness >= 1)
        return;

    // Find the neighbor of this wall section and determine the relative
    // 'openness' of it's plane heights vs those of "this" wall section.
    /// @todo fixme: Should use the visual plane heights of sector clusters.

    int const edge = lineSide.sideId() ^ d->edge;
    LineOwner const *vo = &lineSide.line().vertexOwner(edge)->navigate(ClockDirection(d->edge ^ 1));
    Line const &neighborLine = vo->line();

    if(&neighborLine == &lineSide.line())
    {
        d->openness = 1; // Fully open.
    }
    else if(neighborLine.isSelfReferencing()) /// @todo Skip over these? -ds
    {
        d->openness = 1;
    }
    else
    {
        // Choose the correct side of the neighbor (determined by which vertex is shared).
        LineSide const &neighborLineSide = neighborLine.side(&lineSide.line().vertex(edge) == &neighborLine.from()? d->edge ^ 1 : d->edge);

        if(!neighborLineSide.hasSections() && neighborLineSide.back().hasSector())
        {
            // A one-way window, open side.
            d->openness = 1;
        }
        else if(!neighborLineSide.hasSector() ||
                (neighborLineSide.back().hasSector() && middleMaterialCoversOpening(neighborLineSide)))
        {
            d->openness = 0;
        }
        else if(neighborLineSide.back().hasSector())
        {
            // Its a normal neighbor.
            Sector const *backSec  = neighborLineSide.back().sectorPtr();
            if(backSec != &cluster.sector() &&
               !((plane.isSectorFloor() && backSec->ceiling().heightSmoothed() <= plane.heightSmoothed()) ||
                 (plane.isSectorCeiling() && backSec->floor().height() >= plane.heightSmoothed())))
            {
                // Figure out the relative plane heights.
                coord_t fz = plane.heightSmoothed();
                if(planeIndex == Sector::Ceiling)
                    fz = -fz;

                coord_t bz = backSec->plane(planeIndex).heightSmoothed();
                if(planeIndex == Sector::Ceiling)
                    bz = -bz;

                coord_t bhz = backSec->plane(otherPlaneIndex).heightSmoothed();
                if(planeIndex == Sector::Ceiling)
                    bhz = -bhz;

                d->openness = opennessFactor(fz, bz, bhz);
            }
        }
    }

    if(d->openness < 1)
    {
        LineOwner *vo = lineSide.line().vertexOwner(lineSide.sideId() ^ d->edge);
        if(d->edge) vo = &vo->prev();

        d->inner = Vector3d(lineSide.vertex(d->edge).origin() + vo->innerShadowOffset(),
                            plane.heightSmoothed());
    }
    else
    {
        d->inner = Vector3d(lineSide.vertex(d->edge).origin() + vo->extendedShadowOffset(),
                            plane.heightSmoothed());
    }

    d->outer = Vector3d(lineSide.vertex(d->edge).origin(), plane.heightSmoothed());
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
