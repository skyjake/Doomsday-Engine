/** @file surfacedecorator.cpp  World surface decorator.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "render/surfacedecorator.h"

#include <QMap>
#include <QSet>
#include <de/Observers>
#include <de/Vector>

#include "world/map.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "Sector"
#include "SectorCluster"
#include "Surface"

#include "render/rend_main.h" // Rend_MapSurfaceMaterialSpec()
#include "LightDecoration"
#include "WallEdge"

using namespace de;

typedef QSet<Surface *> SurfaceSet;
typedef QMap<Material *, SurfaceSet> MaterialSurfaceMap;

DENG2_PIMPL_NOREF(SurfaceDecorator)
, DENG2_OBSERVES(Material, DimensionsChange)
, DENG2_OBSERVES(MaterialAnimation, DecorationStageChange)
{
    MaterialSurfaceMap decorated; ///< All surfaces being looked after.

    ~Instance()
    {
        for(SurfaceSet const &set : decorated)
        for(Surface *surface : set)
        {
            observeMaterial(surface->material(), false);
        }
    }

    void observeMaterial(Material &material, bool yes = true)
    {
        if(yes)
        {
            material.audienceForDimensionsChange += this;
            material.animation(MapSurfaceContext).audienceForDecorationStageChange += this;
        }
        else
        {
            material.audienceForDimensionsChange -= this;
            material.animation(MapSurfaceContext).audienceForDecorationStageChange -= this;
        }
    }

    void updateDecorations(Surface &suf, MaterialSnapshot const &materialSnapshot,
        Vector2f const &materialOrigin, Vector3d const &topLeft,
        Vector3d const &bottomRight, Sector *containingSector = nullptr)
    {
        Vector3d delta = bottomRight - topLeft;
        if(de::fequal(delta.length(), 0)) return;

        Material &material = materialSnapshot.material();
        int const axis = suf.normal().maxAxis();

        Vector2d sufDimensions;
        if(axis == 0 || axis == 1)
        {
            sufDimensions.x = std::sqrt(de::squared(delta.x) + de::squared(delta.y));
            sufDimensions.y = delta.z;
        }
        else
        {
            sufDimensions.x = std::sqrt(de::squared(delta.x));
            sufDimensions.y = delta.y;
        }

        if(sufDimensions.x < 0) sufDimensions.x = -sufDimensions.x;
        if(sufDimensions.y < 0) sufDimensions.y = -sufDimensions.y;

        // Generate a number of decorations.
        Material::Decorations const &decorations = material.decorations();
        for(int i = 0; i < decorations.count(); ++i)
        {
            MaterialSnapshotDecoration &matDecor = materialSnapshot.decoration(i);
            MaterialDecoration const *def = decorations[i];

            // Skip values must be at least one.
            Vector2i skip = Vector2i(def->patternSkip().x + 1, def->patternSkip().y + 1)
                                .max(Vector2i(1, 1));

            Vector2f repeat = material.dimensions() * skip;
            if(repeat == Vector2f(0, 0))
                return;

            Vector3d origin = topLeft + suf.normal() * matDecor.elevation;

            float s = de::wrap(matDecor.pos[0] - material.width() * def->patternOffset().x + materialOrigin.x,
                               0.f, repeat.x);

            // Plot decorations.
            for(; s < sufDimensions.x; s += repeat.x)
            {
                // Determine the topmost point for this row.
                float t = de::wrap(matDecor.pos[1] - material.height() * def->patternOffset().y + materialOrigin.y,
                                   0.f, repeat.y);

                for(; t < sufDimensions.y; t += repeat.y)
                {
                    float const offS = s / sufDimensions.x;
                    float const offT = t / sufDimensions.y;
                    Vector3d patternOffset(offS, axis == VZ? offT : offS, axis == VZ? offS : offT);

                    Vector3d decorOrigin     = origin + delta * patternOffset;
                    ConvexSubspace *subspace = suf.map().bspLeafAt(decorOrigin).subspacePtr();

                    if(!subspace) continue;
                    if(!subspace->contains(decorOrigin)) continue;

                    if(containingSector)
                    {
                        // The point must be in the correct sector.
                        if(containingSector != &subspace->sector())
                            continue;
                    }

                    suf.addDecoration(new LightDecoration(matDecor, decorOrigin));
                }
            }
        }
    }

    void markSurfacesForRedecoration(Material &material)
    {
        MaterialSurfaceMap::const_iterator found = decorated.constFind(&material);
        if(found != decorated.constEnd())
        for(Surface *surface : found.value())
        {
            surface->markForDecorationUpdate();
        }
    }

    /// Observes Material DimensionsChange
    void materialDimensionsChanged(Material &material)
    {
        markSurfacesForRedecoration(material);
    }

    /// Observes MaterialAnimation DecorationStageChange
    void materialAnimationDecorationStageChanged(MaterialAnimation & /*anim*/,
        MaterialDecoration &decor)
    {
        markSurfacesForRedecoration(decor.material());
    }
};

SurfaceDecorator::SurfaceDecorator() : d(new Instance)
{}

static bool prepareGeometry(Surface &surface, Vector3d &topLeft,
    Vector3d &bottomRight, Vector2f &materialOrigin)
{
    if(surface.parent().type() == DMU_SIDE)
    {
        LineSide &side = surface.parent().as<LineSide>();
        int section = &side.middle() == &surface? LineSide::Middle
                    : &side.bottom() == &surface? LineSide::Bottom : LineSide::Top;

        if(!side.hasSections()) return false;

        HEdge *leftHEdge  = side.leftHEdge();
        HEdge *rightHEdge = side.rightHEdge();

        if(!leftHEdge || !rightHEdge) return false;

        // Is the wall section potentially visible?
        WallSpec const wallSpec = WallSpec::fromMapSide(side, section);
        WallEdge leftEdge (wallSpec, *leftHEdge, Line::From);
        WallEdge rightEdge(wallSpec, *rightHEdge, Line::To);

        if(!leftEdge.isValid() || !rightEdge.isValid() ||
           de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
            return false;

        topLeft        = leftEdge.top().origin();
        bottomRight    = rightEdge.bottom().origin();
        materialOrigin = -leftEdge.materialOrigin();

        return true;
    }

    if(surface.parent().type() == DMU_PLANE)
    {
        Plane &plane = surface.parent().as<Plane>();
        AABoxd const &sectorAABox = plane.sector().aaBox();

        topLeft = Vector3d(sectorAABox.minX,
                           plane.isSectorFloor()? sectorAABox.maxY : sectorAABox.minY,
                           plane.heightSmoothed());

        bottomRight = Vector3d(sectorAABox.maxX,
                               plane.isSectorFloor()? sectorAABox.minY : sectorAABox.maxY,
                               plane.heightSmoothed());

        materialOrigin = Vector2f(-fmod(sectorAABox.minX, 64) - surface.materialOriginSmoothed().x,
                                  -fmod(sectorAABox.minY, 64) - surface.materialOriginSmoothed().y);

        return true;
    }

    return false;
}

static inline Sector *containingSector(Surface &surface)
{
    if(surface.parent().type() == DMU_PLANE)
        return &surface.parent().as<Plane>().sector();
    return nullptr;
}

void SurfaceDecorator::decorate(Surface &surface)
{
    if(!surface.hasMaterial())
        return; // Huh?

    if(!surface.needsDecorationUpdate())
        return;

    surface.markForDecorationUpdate(false);
    surface.clearDecorations();

    Vector3d topLeft, bottomRight;
    Vector2f materialOrigin;

    if(prepareGeometry(surface, topLeft, bottomRight, materialOrigin))
    {
        MaterialSnapshot const &materialSnapshot =
            surface.material().prepare(Rend_MapSurfaceMaterialSpec());

        d->updateDecorations(surface, materialSnapshot, materialOrigin,
                             topLeft, bottomRight, containingSector(surface));
    }
}

void SurfaceDecorator::redecorate()
{
    MaterialSurfaceMap::iterator i = d->decorated.begin();
    while(i != d->decorated.end())
    {
        MaterialSnapshot const *materialSnapshot = nullptr;

        SurfaceSet const &surfaceSet = i.value();
        for(Surface *surface : surfaceSet)
        {
            if(!surface->needsDecorationUpdate())
                continue;

            // Time to prepare the material?
            if(!materialSnapshot)
            {
                Material &material = *i.key();
                materialSnapshot = &material.prepare(Rend_MapSurfaceMaterialSpec());
            }

            surface->markForDecorationUpdate(false);
            surface->clearDecorations();

            Vector3d topLeft, bottomRight;
            Vector2f materialOrigin;

            if(prepareGeometry(*surface, topLeft, bottomRight, materialOrigin))
            {
                d->updateDecorations(*surface, *materialSnapshot, materialOrigin,
                                     topLeft, bottomRight, containingSector(*surface));
            }
        }
        ++i;
    }
}

void SurfaceDecorator::reset()
{
    d->decorated.clear();
}

void SurfaceDecorator::remove(Surface *surface)
{
    if(!surface) return;

    // First try the set for the currently assigned material.
    if(surface->hasMaterial())
    {
        MaterialSurfaceMap::iterator found = d->decorated.find(&surface->material());
        if(found != d->decorated.end())
        {
            SurfaceSet &surfaceSet = found.value();
            if(surfaceSet.remove(surface))
            {
                if(surfaceSet.isEmpty())
                {
                    d->decorated.remove(&surface->material());
                    d->observeMaterial(surface->material(), false);
                }
                return;
            }
        }
    }

    // The material may have changed.
    MaterialSurfaceMap::iterator i = d->decorated.begin();
    while(i != d->decorated.end())
    {
        SurfaceSet &surfaceSet = i.value();
        if(surfaceSet.remove(surface))
        {
            if(surfaceSet.isEmpty())
            {
                Material *material = i.key();
                d->decorated.remove(material);
                d->observeMaterial(surface->material(), false);
            }
            return;
        }
        ++i;
    }
}

void SurfaceDecorator::add(Surface *surface)
{
    if(!surface) return;

    remove(surface);

    if(!surface->hasMaterial()) return;
    if(!surface->material().isDecorated()) return;

    d->decorated[&surface->material()].insert(surface);

    /// @todo Defer until first decorated?
    d->observeMaterial(surface->material());
}
