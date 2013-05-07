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

#include "Plane"
#include "Sector"

#include "map/r_world.h"
#include "map/lineowner.h"

#include "render/shadowedge.h"

namespace de {

DENG2_PIMPL_NOREF(ShadowEdge)
{
    BspLeaf *bspLeaf;
    Line::Side *side;
    int edge;

    Vector3d inner;
    Vector3d outer;
    float openness;
    float sideOpenness;

    Instance(BspLeaf &bspLeaf, Line::Side &side, int edge)
        : bspLeaf(&bspLeaf),
          side(&side),
          edge(edge),
          openness(0),
          sideOpenness(0)
    {}

    uint radioEdgeHackType(Sector const *front, Sector const *back, bool isCeiling, float fz, float bz)
    {
        Surface const &surface = side->surface(isCeiling? Line::Side::Top : Line::Side::Bottom);

        if(fz < bz && !surface.hasMaterial())
            return 3; // Consider it fully open.

        // Is the back sector closed?
        if(front->floor().visHeight() >= back->ceiling().visHeight())
        {
            if(front->planeSurface(isCeiling? Plane::Floor : Plane::Ceiling).hasSkyMaskedMaterial())
            {
                if(back->planeSurface(isCeiling? Plane::Floor : Plane::Ceiling).hasSkyMaskedMaterial())
                    return 3; // Consider it fully open.
            }
            else
            {
                return 1; // Consider it fully closed.
            }
        }

        // Check for unmasked midtextures on twosided lines that completely
        // fill the gap between floor and ceiling (we don't want to give away
        // the location of any secret areas (false walls)).
        if(R_MiddleMaterialCoversOpening(*side))
            return 1; // Consider it fully closed.

        return 0;
    }
};

ShadowEdge::ShadowEdge(BspLeaf &bspLeaf, Line::Side &side, int edge)
    : d(new Instance(bspLeaf, side, edge))
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
 * Returns a value in the range of 0...2, which depicts how open the
 * specified edge is. Zero means that the edge is completely closed: it is
 * facing a wall or is relatively distant from the edge on the other side.
 * Values between zero and one describe how near the other edge is. An
 * openness value of one means that the other edge is at the same height as
 * this one. 2 means that the other edge is past our height ("clearly open").
 */
static float radioEdgeOpenness(float fz, float bz, float bhz)
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

void ShadowEdge::prepare(int planeIndex)
{
    Line const &line = d->side->line();

    Plane const &plane = d->side->sector().plane(planeIndex);
    coord_t plnHeight = plane.visHeight();

    // Determine the openness of the line. If this edge is open,
    // there won't be a shadow at all. Open neighbours cause some
    // changes in the polygon corner vertices (placement, color).
    Sector const *front = 0;
    Sector const *back = 0;
    coord_t fz = 0, bz = 0, bhz = 0;
    if(line.hasBackSections())
    {
        front = d->side->sectorPtr();
        back  = d->side->back().sectorPtr();
        setRelativeHeights(front, back, planeIndex, &fz, &bz, &bhz);

        uint hackType = d->radioEdgeHackType(front, back, planeIndex == Plane::Ceiling, fz, bz);
        if(hackType)
        {
            d->openness = hackType - 1;
        }
        else
        {
            d->openness = radioEdgeOpenness(fz, bz, bhz);
        }
    }

    // Only calculate the remaining values when the edge is at least partially open.
    if(d->openness >= 1) return;

    // Find the neighbor of this edge and determine its 'openness'.

    LineOwner *vo = line.vertexOwner(d->side->lineSideId() ^ d->edge)->_link[d->edge ^ 1];
    Line *neighbor = &vo->line();

    if(neighbor != &line && !neighbor->hasBackSections() &&
       (neighbor->isBspWindow()) &&
       neighbor->frontSectorPtr() != d->bspLeaf->sectorPtr())
    {
        // A one-way window, open side.
        d->sideOpenness = 1;
    }
    else if(!(neighbor == &line || !neighbor->hasBackSections()))
    {
        int otherSide = (&d->side->vertex(d->edge) == &neighbor->from()? d->edge : d->edge ^ 1);
        Sector *othersec = neighbor->sectorPtr(otherSide);

        if(R_MiddleMaterialCoversOpening(neighbor->side(otherSide ^ 1)))
        {
            d->sideOpenness = 0;
        }
        else if(neighbor->isSelfReferencing())
        {
            d->sideOpenness = 1;
        }
        else
        {
            // Its a normal neighbor.
            if(neighbor->sectorPtr(otherSide) != d->side->sectorPtr() &&
               !((plane.type() == Plane::Floor && othersec->ceiling().visHeight() <= plane.visHeight()) ||
                 (plane.type() == Plane::Ceiling && othersec->floor().height() >= plane.visHeight())))
            {
                front = d->side->sectorPtr();
                back  = neighbor->sectorPtr(otherSide);

                setRelativeHeights(front, back, planeIndex, &fz, &bz, &bhz);
                d->sideOpenness = radioEdgeOpenness(fz, bz, bhz);
            }
        }
    }

    if(d->sideOpenness < 1)
    {
        vo = line.vertexOwner(d->side->lineSideId() ^ d->edge);
        if(d->edge) vo = &vo->prev();

        d->inner = Vector3d(d->side->vertex(d->edge).origin() + vo->innerShadowOffset(),
                            plnHeight);
    }
    else
    {
        d->inner = Vector3d(d->side->vertex(d->edge).origin() + vo->extendedShadowOffset(),
                            plnHeight);
    }

    d->outer = Vector3d(d->side->vertex(d->edge).origin(), plnHeight);
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

float ShadowEdge::sideOpenness() const
{
    return d->sideOpenness;
}

} // namespace de
