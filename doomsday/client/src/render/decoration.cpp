/** @file decoration.cpp World surface decoration.
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

#include "de_platform.h"
#include "de_console.h"
#include "de_render.h"

#include "def_main.h"

#include "world/map.h"
#include "BspLeaf"
#include "Surface"

#include "render/decoration.h"

using namespace de;

static float angleFadeFactor = .1f; ///< cvar
static float brightFactor    = 1;   ///< cvar

/**
 * @return  @c > 0 if @a lightlevel passes the min max limit condition.
 */
static float checkLightLevel(float lightlevel, float min, float max)
{
    // Has a limit been set?
    if(de::fequal(min, max)) return 1;
    return de::clamp(0.f, (lightlevel - min) / float(max - min), 1.f);
}

DENG2_PIMPL_NOREF(Decoration)
{
    MaterialSnapshotDecoration *source;
    Vector3d origin;
    BspLeaf *bspLeaf;   ///< BSP leaf at @ref origin in the map (not owned).
    Surface *surface;
    int lumIdx;         ///< Generated lumobj index (or Lumobj::NoIndex).
    float fadeMul;      ///< Intensity multiplier (lumobj and flare).

    Instance(MaterialSnapshotDecoration &source, Vector3d const &origin)
        : source(&source),
          origin(origin),
          bspLeaf(0),
          surface(0),
          lumIdx(Lumobj::NoIndex),
          fadeMul(1)
    {}
};

Decoration::Decoration(MaterialSnapshotDecoration &source, Vector3d const &origin)
    : d(new Instance(source, origin))
{}

MaterialSnapshotDecoration &Decoration::source()
{
    return *d->source;
}

MaterialSnapshotDecoration const &Decoration::source() const
{
    return *d->source;
}

bool Decoration::hasSurface() const
{
    return d->surface != 0;
}

Surface &Decoration::surface()
{
    if(d->surface != 0)
    {
        return *d->surface;
    }
    /// @throw MissingSurfaceError Attempted with no surface attributed.
    throw MissingSurfaceError("Decoration::surface", "No surface is attributed");
}

Surface const &Decoration::surface() const
{
    if(d->surface != 0)
    {
        return *d->surface;
    }
    /// @throw MissingSurfaceError Attempted with no surface attributed.
    throw MissingSurfaceError("Decoration::surface", "No surface is attributed");
}

void Decoration::setSurface(Surface *newSurface)
{
    d->surface = newSurface;
}

Vector3d const &Decoration::origin() const
{
    return d->origin;
}

BspLeaf &Decoration::bspLeafAtOrigin() const
{
    if(!d->bspLeaf)
    {
        // Determnine this now.
        d->bspLeaf = &surface().map().bspLeafAt(d->origin);
    }
    return *d->bspLeaf;
}

void Decoration::generateLumobj()
{
    d->lumIdx = Lumobj::NoIndex;

    // Decorations with zero color intensity produce no light.
    if(d->source->color == Vector3f(0, 0, 0))
        return;

    // Does it pass the ambient light limitation?
    float lightLevel = bspLeafAtOrigin().sector().lightLevel();
    Rend_ApplyLightAdaptation(lightLevel);

    float intensity = checkLightLevel(lightLevel, d->source->lightLevels[0],
                                      d->source->lightLevels[1]);

    if(intensity < .0001f)
        return;

    // Apply the brightness factor (was calculated using sector lightlevel).
    d->fadeMul = intensity * ::brightFactor;
    d->lumIdx  = Lumobj::NoIndex;

    if(d->fadeMul <= 0)
        return;

    Lumobj &lum = surface().map().addLumobj(
        Lumobj(d->origin, d->source->radius, d->source->color * d->fadeMul,
               MAX_DECOR_DISTANCE));

    // Any lightmaps to configure?
    lum.setLightmap(Lumobj::Side, d->source->tex)
       .setLightmap(Lumobj::Down, d->source->floorTex)
       .setLightmap(Lumobj::Up,   d->source->ceilTex);

    // Remember the light's unique index (for projecting a flare).
    d->lumIdx = lum.indexInMap();
}

void Decoration::generateFlare()
{
    // Only decorations with active light sources produce flares.
    Lumobj *lum = surface().map().lumobj(d->lumIdx);
    if(!lum) return; // Huh?

    // Is the point in range?
    double distance = R_ViewerLumobjDistance(lum->indexInMap());
    if(distance > lum->maxDistance())
        return;

    vissprite_t *vis = R_NewVisSprite(VSPR_FLARE);

    vis->origin   = lum->origin();
    vis->distance = distance;

    vis->data.flare.isDecoration = true;
    vis->data.flare.tex          = d->source->flareTex;
    vis->data.flare.size         =
        d->source->haloRadius > 0? de::max(1.f, d->source->haloRadius * 60 * (50 + haloSize) / 100.0f) : 0;

    // Color is taken from the lumobj.
    V3f_Set(vis->data.flare.color, lum->color().x, lum->color().y, lum->color().z);

    vis->data.flare.lumIdx       = lum->indexInMap();

    // Fade out as distance from viewer increases.
    vis->data.flare.mul          = lum->attenuation(distance);

    // Halo brightness drops as the angle gets too big.
    if(d->source->elevation < 2 && ::angleFadeFactor > 0) // Close the surface?
    {
        Vector3d const eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);
        Vector3d const vecFromOriginToEye = (lum->origin() - eye).normalize();

        float dot = float( -surface().normal().dot(vecFromOriginToEye) );
        if(dot < ::angleFadeFactor / 2)
        {
            vis->data.flare.mul = 0;
        }
        else if(dot < 3 * ::angleFadeFactor)
        {
            vis->data.flare.mul = (dot - ::angleFadeFactor / 2)
                                / (2.5f * ::angleFadeFactor);
        }
    }
}

void Decoration::consoleRegister() // static
{
    C_VAR_FLOAT("rend-light-decor-angle",  &::angleFadeFactor, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &::brightFactor,    0, 0, 10);
}

float Decoration::angleFadeFactor() // static
{
    return ::angleFadeFactor;
}

float Decoration::brightFactor() // static
{
    return ::brightFactor;
}
