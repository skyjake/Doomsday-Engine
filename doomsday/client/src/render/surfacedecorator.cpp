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

#include <de/memoryzone.h>

#include <de/Vector>

#include "de_platform.h"

#include "world/map.h"
#include "BspLeaf"

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

DENG2_PIMPL(SurfaceDecorator)
{
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

    uint generateDecorations(MaterialSnapshot::Decoration const &decor,
        Vector2i const &patternOffset, Vector2i const &patternSkip, Surface &suf,
        Material &material, Vector3d const &topLeft_, Vector3d const &/*bottomRight*/,
        Vector2d sufDimensions, Vector3d const &delta, int axis,
        Vector2f const &matOffset, Sector *containingSector)
    {
        // Skip values must be at least one.
        Vector2i skip = Vector2i(patternSkip.x + 1, patternSkip.y + 1)
                            .max(Vector2i(1, 1));

        Vector2f repeat = material.dimensions() * skip;
        if(repeat == Vector2f(0, 0))
            return 0;

        Vector3d topLeft = topLeft_ + suf.normal() * decor.elevation;

        float s = de::wrap(decor.pos[0] - material.width() * patternOffset.x + matOffset.x,
                           0.f, repeat.x);

        // Plot decorations.
        uint plotted = 0;
        for(; s < sufDimensions.x; s += repeat.x)
        {
            // Determine the topmost point for this row.
            float t = de::wrap(decor.pos[1] - material.height() * patternOffset.y + matOffset.y,
                               0.f, repeat.y);

            for(; t < sufDimensions.y; t += repeat.y)
            {
                float const offS = s / sufDimensions.x;
                float const offT = t / sufDimensions.y;
                Vector3d offset(offS, axis == VZ? offT : offS, axis == VZ? offS : offT);

                Vector3d origin  = topLeft + delta * offset;
                BspLeaf &bspLeaf = suf.map().bspLeafAt(origin);
                if(containingSector)
                {
                    // The point must be inside the correct sector.
                    if(bspLeaf.sectorPtr() != containingSector
                       || !bspLeaf.polyContains(origin))
                        continue;
                }

                if(SurfaceDecorSource *source = suf.newDecoration())
                {
                    source->origin   = origin;
                    source->bspLeaf  = &bspLeaf;
                    source->matDecor = &decor;

                    plotted += 1;
                }
            }
        }

        return plotted;
    }

    void plotSources(Surface &suf, Vector2f const &offset, Vector3d const &topLeft,
                     Vector3d const &bottomRight, Sector *sec = 0)
    {
        Vector3d delta = bottomRight - topLeft;
        if(de::fequal(delta.length(), 0)) return;

        int const axis = suf.normal().maxAxis();

        Vector2d sufDimensions;
        if(axis == 0 || axis == 1)
        {
            sufDimensions.x = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            sufDimensions.y = delta.z;
        }
        else
        {
            sufDimensions.x = std::sqrt(delta.x * delta.x);
            sufDimensions.y = delta.y;
        }

        if(sufDimensions.x < 0) sufDimensions.x = -sufDimensions.x;
        if(sufDimensions.y < 0) sufDimensions.y = -sufDimensions.y;

        // Generate a number of lights.
        MaterialSnapshot const &ms = suf.material().prepare(Rend_MapSurfaceMaterialSpec());

        Material::Decorations const &decorations = suf.material().decorations();
        for(int i = 0; i < decorations.count(); ++i)
        {
            MaterialSnapshot::Decoration const &decor = ms.decoration(i);
            MaterialDecoration const *def = decorations[i];

            generateDecorations(decor, def->patternOffset(), def->patternSkip(),
                                suf, suf.material(), topLeft, bottomRight, sufDimensions,
                                delta, axis, offset, sec);
        }
    }

};

SurfaceDecorator::SurfaceDecorator() : d(new Instance(this))
{}

void SurfaceDecorator::decorate(Surface &surface)
{
    if(surface._decorationData.needsUpdate)
    {
        surface.clearDecorations();

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
                                 plane.visHeight());

                Vector3d bottomRight(sectorAABox.maxX,
                                     plane.isSectorFloor()? sectorAABox.minY : sectorAABox.maxY,
                                     plane.visHeight());

                Vector2f offset(-fmod(sectorAABox.minX, 64) - surface.visMaterialOrigin().x,
                                -fmod(sectorAABox.minY, 64) - surface.visMaterialOrigin().y);

                d->plotSources(surface, offset, topLeft, bottomRight, &sector);
            }
        }

        surface._decorationData.needsUpdate = false;
    }

    Surface::DecorSource *sources = surface._decorationData.sources;
    for(int i = 0; i < surface.decorationCount(); ++i)
    {
        d->newDecoration(sources[i]);
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
