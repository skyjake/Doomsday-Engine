/** @file surfacedecorator.cpp World surface decorator.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <QMap>
#include <QSet>

#include <de/memoryzone.h>

#include <de/Observers>
#include <de/Vector>

#include "de_platform.h"

#include "world/map.h"
#include "BspLeaf"

#include "Material"
#include "MaterialSnapshot"

#include "render/rend_main.h" // Rend_MapSurfaceMaterialSpec()
#include "WallEdge"

#include "render/surfacedecorator.h"

using namespace de;

static uint decorCount;
static Decoration *decorFirst;
static Decoration *decorCursor;

static Decoration *allocDecoration()
{
    // If the cursor is NULL, new decorations must be allocated.
    if(!decorCursor)
    {
        // Allocate a new entry.
        Decoration *decor = (Decoration *) Z_Calloc(sizeof(Decoration), PU_APPSTATIC, NULL);

        if(!decorFirst)
        {
            decorFirst = decor;
        }
        else
        {
            decor->next = decorFirst;
            decorFirst = decor;
        }

        return decor;
    }
    else
    {
        // Reuse an existing decoration.
        Decoration *decor = decorCursor;

        // Advance the cursor.
        decorCursor = decorCursor->next;

        return decor;
    }
}

typedef QSet<Surface *> SurfaceSet;
typedef QMap<Material *, SurfaceSet> MaterialSurfaceMap;

DENG2_PIMPL(SurfaceDecorator),
DENG2_OBSERVES(Material, DimensionsChange),
DENG2_OBSERVES(MaterialAnimation, DecorationStageChange)
{
    MaterialSurfaceMap decorated; ///< All surfaces being looked after.

    Instance(Public *i) : Base(i)
    {}

    void newDecoration(SurfaceDecorSource &source)
    {
        // Out of sources?
        if(decorCount >= MAX_DECOR_LIGHTS) return;

        Decoration *decor = allocDecoration();

        decor->setSource(&source);

        decorCount += 1;
    }

    void generateDecorations(MaterialSnapshotDecoration &matDecor,
        Vector2i const &patternOffset, Vector2i const &patternSkip, Surface &suf,
        Material &material, Vector3d const &topLeft_, Vector3d const &/*bottomRight*/,
        Vector2d sufDimensions, Vector3d const &delta, int axis,
        Vector2f const &matOffset, Sector *containingSector = 0)
    {
        // Skip values must be at least one.
        Vector2i skip = Vector2i(patternSkip.x + 1, patternSkip.y + 1)
                            .max(Vector2i(1, 1));

        Vector2f repeat = material.dimensions() * skip;
        if(repeat == Vector2f(0, 0))
            return;

        Vector3d topLeft = topLeft_ + suf.normal() * matDecor.elevation;

        float s = de::wrap(matDecor.pos[0] - material.width() * patternOffset.x + matOffset.x,
                           0.f, repeat.x);

        // Plot decorations.
        for(; s < sufDimensions.x; s += repeat.x)
        {
            // Determine the topmost point for this row.
            float t = de::wrap(matDecor.pos[1] - material.height() * patternOffset.y + matOffset.y,
                               0.f, repeat.y);

            for(; t < sufDimensions.y; t += repeat.y)
            {
                float const offS = s / sufDimensions.x;
                float const offT = t / sufDimensions.y;
                Vector3d offset(offS, axis == VZ? offT : offS, axis == VZ? offS : offT);

                Vector3d origin = topLeft + delta * offset;
                if(containingSector)
                {
                    // The point must be inside the correct sector.
                    BspLeaf &bspLeaf = suf.map().bspLeafAt(origin);
                    if(bspLeaf.sectorPtr() != containingSector
                       || !bspLeaf.polyContains(origin))
                        continue;
                }

                suf.newDecorSource(matDecor, origin);
            }
        }
    }

    void plotSources(Surface &suf, Vector2f const &offset, Vector3d const &topLeft,
                     Vector3d const &bottomRight, Sector *containingSector = 0)
    {
        Vector3d delta = bottomRight - topLeft;
        if(de::fequal(delta.length(), 0)) return;

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

        // Generate a number of lights.
        MaterialSnapshot const &ms = suf.material().prepare(Rend_MapSurfaceMaterialSpec());

        Material::Decorations const &decorations = suf.material().decorations();
        for(int i = 0; i < decorations.count(); ++i)
        {
            MaterialSnapshotDecoration &decor = ms.decoration(i);
            MaterialDecoration const *def = decorations[i];

            generateDecorations(decor, def->patternOffset(), def->patternSkip(),
                                suf, suf.material(), topLeft, bottomRight, sufDimensions,
                                delta, axis, offset, containingSector);
        }
    }

    void markSurfacesForRedecoration(Material &material)
    {
        MaterialSurfaceMap::const_iterator found = decorated.constFind(&material);
        if(found != decorated.constEnd())
        foreach(Surface *surface, found.value())
        {
            surface->markAsNeedingDecorationUpdate();
        }
    }

    /// Observes Material DimensionsChange
    void materialDimensionsChanged(Material &material)
    {
        markSurfacesForRedecoration(material);
    }

    /// Observes MaterialAnimation DecorationStageChange
    void materialAnimationDecorationStageChanged(MaterialAnimation &anim,
        Material::Decoration &decor)
    {
        markSurfacesForRedecoration(decor.material());
        DENG2_UNUSED(anim);
    }
};

SurfaceDecorator::SurfaceDecorator() : d(new Instance(this))
{}

void SurfaceDecorator::decorate(Surface &surface)
{
    if(surface._needDecorationUpdate)
    {
        surface.clearDecorSources();

        if(surface.hasMaterial())
        {
            if(surface.parent().type() == DMU_SIDE)
            {
                LineSide &side = surface.parent().as<LineSide>();
                DENG_ASSERT(side.hasSections());

                HEdge *leftHEdge  = side.leftHEdge();
                HEdge *rightHEdge = side.rightHEdge();

                if(leftHEdge && rightHEdge)
                {
                    // Is the wall section potentially visible?
                    int const section = &side.middle() == &surface? LineSide::Middle
                                      : &side.bottom() == &surface? LineSide::Bottom : LineSide::Top;

                    WallSpec const wallSpec = WallSpec::fromMapSide(side, section);
                    WallEdge leftEdge (wallSpec, *leftHEdge, Line::From);
                    WallEdge rightEdge(wallSpec, *rightHEdge, Line::To);

                    if(leftEdge.isValid() && rightEdge.isValid()
                       && !de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
                    {
                        d->plotSources(side.surface(section), -leftEdge.materialOrigin(),
                                       leftEdge.top().origin(), rightEdge.bottom().origin());
                    }
                }
            }

            if(surface.parent().type() == DMU_PLANE)
            {
                Plane &plane = surface.parent().as<Plane>();

                Sector &sector = plane.sector();
                AABoxd const &sectorAABox = sector.aaBox();

                Vector3d topLeft(sectorAABox.minX,
                                 plane.isSectorFloor()? sectorAABox.maxY : sectorAABox.minY,
                                 plane.heightSmoothed());

                Vector3d bottomRight(sectorAABox.maxX,
                                     plane.isSectorFloor()? sectorAABox.minY : sectorAABox.maxY,
                                     plane.heightSmoothed());

                Vector2f offset(-fmod(sectorAABox.minX, 64) - surface.materialOriginSmoothed().x,
                                -fmod(sectorAABox.minY, 64) - surface.materialOriginSmoothed().y);

                d->plotSources(surface, offset, topLeft, bottomRight, &sector);
            }
        }

        surface._needDecorationUpdate = false;
    }

    foreach(SurfaceDecorSource *source, surface.decorSources())
    {
        d->newDecoration(*source);
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
    surface->material().audienceForDimensionsChange += d;
    surface->material().animation(MapSurfaceContext).audienceForDecorationStageChange += d;
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
                    surface->material().audienceForDimensionsChange -= d;
                    surface->material().animation(MapSurfaceContext).audienceForDecorationStageChange -= d;
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
                material->audienceForDimensionsChange -= d;
                material->animation(MapSurfaceContext).audienceForDecorationStageChange -= d;
            }
            return;
        }
        ++i;
    }
}

void SurfaceDecorator::reset()
{
    d->decorated.clear();
}

void SurfaceDecorator::redecorate()
{
    foreach(SurfaceSet const &set, d->decorated)
    foreach(Surface *surface, set)
    {
        decorate(*surface);
    }
}

/// @todo Refactor away --------------------------------------------------------

void Rend_DecorReset()
{
    decorCount = 0;
    decorCursor = decorFirst;
}

void Rend_DecorAddLuminous()
{
    if(!useLightDecorations) return;

    for(Decoration *decor = decorFirst; decor != decorCursor; decor = decor->next)
    {
        decor->generateLumobj();
    }
}

void Rend_DecorProject()
{
    if(!useLightDecorations) return;

    for(Decoration *decor = decorFirst; decor != decorCursor; decor = decor->next)
    {
        decor->generateFlare();
    }
}
