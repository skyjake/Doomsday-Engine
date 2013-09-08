/** @file rend_decor.cpp Surface Decorations.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
#include "de_console.h"
#include "de_render.h"

#include "dd_main.h" // App_World(), remove me
#include "def_main.h"

#include "world/map.h"
#include "BspLeaf"

#include "render/vissprite.h"
#include "WallEdge"

#include "render/rend_decor.h"

using namespace de;

/// Quite a bit of decorations, there!
#define MAX_DECOR_LIGHTS        (16384)

/// No decorations are visible beyond this.
#define MAX_DECOR_DISTANCE      (2048)

byte useLightDecorations     = true;
float decorLightBrightFactor = 1;
float decorLightFadeAngle    = .1f;

struct Decoration
{
    Surface::DecorSource *_source;
    Surface *_surface;
    int _lumIdx; // Index of linked lumobj, or Lumobj::NoIndex.
    float _fadeMul;

    Surface::DecorSource &source()
    {
        DENG_ASSERT(_source != 0);
        return *_source;
    }

    Surface::DecorSource const &source() const
    {
        DENG_ASSERT(_source != 0);
        return *_source;
    }

    Surface &surface()
    {
        DENG_ASSERT(_surface != 0);
        return *_surface;
    }

    Surface const &surface() const
    {
        DENG_ASSERT(_surface != 0);
        return *_surface;
    }

    Map &map()
    {
        return surface().map();
    }

    Map const &map() const
    {
        return surface().map();
    }

    Vector3d const &origin() const
    {
        return source().origin;
    }

    BspLeaf &bspLeafAtOrigin() const
    {
        return *source().bspLeaf;
    }

    /**
     * Generates a lumobj for the decoration.
     */
    void generateLumobj()
    {
        _lumIdx = Lumobj::NoIndex;

        MaterialSnapshot::Decoration const &matDecor = materialDecoration();

        // Decorations with zero color intensity produce no light.
        if(matDecor.color == Vector3f(0, 0, 0))
            return;

        // Does it pass the ambient light limitation?
        float lightLevel = lightLevelAtOrigin();
        Rend_ApplyLightAdaptation(lightLevel);

        float intensity = checkLightLevel(lightLevel, matDecor.lightLevels[0],
                                          matDecor.lightLevels[1]);

        if(intensity < .0001f)
            return;

        // Apply the brightness factor (was calculated using sector lightlevel).
        _fadeMul = intensity * decorLightBrightFactor;
        _lumIdx = Lumobj::NoIndex;

        if(_fadeMul <= 0)
            return;

        Lumobj &lum = map().addLumobj(
            Lumobj(origin(), matDecor.radius, matDecor.color * _fadeMul,
                   MAX_DECOR_DISTANCE));

        // Any lightmaps to configure?
        lum.setLightmap(Lumobj::Side, matDecor.tex)
           .setLightmap(Lumobj::Down, matDecor.floorTex)
           .setLightmap(Lumobj::Up,   matDecor.ceilTex);

        // Remember the light's unique index (for projecting a flare).
        _lumIdx = lum.indexInMap();
    }

    /**
     * Generates a VSPR_FLARE vissprite for the decoration.
     */
    void generateFlare()
    {
        // Only decorations with active light sources produce flares.
        Lumobj *lum = lumobj();
        if(!lum) return; // Huh?

        // Is the point in range?
        double distance = R_ViewerLumobjDistance(lum->indexInMap());
        if(distance > lum->maxDistance())
            return;

        MaterialSnapshot::Decoration const &matDecor = materialDecoration();

        vissprite_t *vis = R_NewVisSprite(VSPR_FLARE);

        vis->origin   = lum->origin();
        vis->distance = distance;

        vis->data.flare.isDecoration = true;
        vis->data.flare.tex          = matDecor.flareTex;
        vis->data.flare.size         =
            matDecor.haloRadius > 0? de::max(1.f, matDecor.haloRadius * 60 * (50 + haloSize) / 100.0f) : 0;

        // Color is taken from the lumobj.
        V3f_Set(vis->data.flare.color, lum->color().x, lum->color().y, lum->color().z);

        vis->data.flare.lumIdx       = lum->indexInMap();

        // Fade out as distance from viewer increases.
        vis->data.flare.mul          = lum->attenuation(distance);

        // Halo brightness drops as the angle gets too big.
        if(matDecor.elevation < 2 && decorLightFadeAngle > 0) // Close the surface?
        {
            Vector3d const eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);
            Vector3d const vecFromOriginToEye = (lum->origin() - eye).normalize();

            float dot = float( -surface().normal().dot(vecFromOriginToEye) );
            if(dot < decorLightFadeAngle / 2)
            {
                vis->data.flare.mul = 0;
            }
            else if(dot < 3 * decorLightFadeAngle)
            {
                vis->data.flare.mul = (dot - decorLightFadeAngle / 2)
                                    / (2.5f * decorLightFadeAngle);
            }
        }
    }

private:
    float lightLevelAtOrigin() const
    {
        return bspLeafAtOrigin().sector().lightLevel();
    }

    MaterialSnapshot::Decoration const &materialDecoration() const
    {
        return *source().matDecor;
    }

    Lumobj *lumobj() const
    {
        return map().lumobj(_lumIdx);
    }

    /**
     * @return  @c > 0 if @a lightlevel passes the min max limit condition.
     */
    static float checkLightLevel(float lightlevel, float min, float max)
    {
        // Has a limit been set?
        if(de::fequal(min, max)) return 1;
        return de::clamp(0.f, (lightlevel - min) / float(max - min), 1.f);
    }

public:
    Decoration *next;
};

static uint decorCount;
static Decoration *decorFirst;
static Decoration *decorCursor;

static void recycleDecorations()
{
    decorCount = 0;
    decorCursor = decorFirst;
}

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

class LightDecorator
{
public:
    /**
     * Decorate @a surface.
     */
    static void decorate(Surface &surface)
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
                            plotSources(side.surface(section), -leftEdge.materialOrigin(),
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

                    plotSources(surface, offset, topLeft, bottomRight, &sector);
                }
            }

            surface._decorationData.needsUpdate = false;
        }

        Surface::DecorSource *sources = surface._decorationData.sources;
        for(int i = 0; i < surface.decorationCount(); ++i)
        {
            newDecoration(surface, sources[i]);
        }
    }

private:
    static void newDecoration(Surface &suf, Surface::DecorSource &source)
    {
        // Out of sources?
        if(decorCount >= MAX_DECOR_LIGHTS) return;

        Decoration *decor = allocDecoration();

        decor->_source  = &source;
        decor->_surface = &suf;
        decor->_lumIdx  = Lumobj::NoIndex;
        decor->_fadeMul = 1;

        decorCount += 1;
    }

    static uint generateDecorations(MaterialSnapshot::Decoration const &decor,
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

                if(Surface::DecorSource *source = suf.newDecoration())
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

    static void plotSources(Surface &suf, Vector2f const &offset,
        Vector3d const &topLeft, Vector3d const &bottomRight, Sector *sec = 0)
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

void Rend_DecorRegister()
{
    C_VAR_BYTE ("rend-light-decor",        &useLightDecorations,    0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-angle",  &decorLightFadeAngle,    0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &decorLightBrightFactor, 0, 0, 10);
}

void Rend_DecorInitForMap(Map &map)
{
    DENG_UNUSED(map);
    recycleDecorations();
}

void Rend_DecorBeginFrame()
{
    // This only needs to be done if decorations have been enabled.
    if(!useLightDecorations) return;

    recycleDecorations();

    /// @todo fixme: do not assume the current map.
    Map &map = App_World().map();
    foreach(Surface *surface, map.decoratedSurfaces())
    {
        LightDecorator::decorate(*surface);
    }
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
