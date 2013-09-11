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

Decoration::Decoration(SurfaceDecorSource *source)
    : next(0),
      _source(source), _bspLeaf(0), _lumIdx(Lumobj::NoIndex), _fadeMul(1)
{}

bool Decoration::hasSource() const
{
    return _source != 0;
}

SurfaceDecorSource &Decoration::source()
{
    if(_source != 0)
    {
        return *_source;
    }
    /// @throw MissingSourceError Attempted with no source attributed.
    throw MissingSourceError("Decoration::source", "No source is attributed");
}

SurfaceDecorSource const &Decoration::source() const
{
    if(_source != 0)
    {
        return *_source;
    }
    /// @throw MissingSourceError Attempted with no source attributed.
    throw MissingSourceError("Decoration::source", "No source is attributed");
}

void Decoration::setSource(SurfaceDecorSource *newSource)
{
    _source = newSource;
    // Forget the previously determnined BSP leaf and generated lumobj.
    _bspLeaf = 0;
    _lumIdx = Lumobj::NoIndex;
}

Surface &Decoration::surface()
{
    return source().surface();
}

Surface const &Decoration::surface() const
{
    return source().surface();
}

Map &Decoration::map()
{
    return surface().map();
}

Map const &Decoration::map() const
{
    return surface().map();
}

Vector3d const &Decoration::origin() const
{
    return source().origin();
}

BspLeaf &Decoration::bspLeafAtOrigin() const
{
    if(!_bspLeaf)
    {
        // Determnine this now.
        _bspLeaf = &map().bspLeafAt(origin());
    }
    return *_bspLeaf;
}

void Decoration::generateLumobj()
{
    _lumIdx = Lumobj::NoIndex;

    MaterialSnapshotDecoration const &matDecor = materialDecoration();

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
    _fadeMul = intensity * ::brightFactor;
    _lumIdx   = Lumobj::NoIndex;

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

void Decoration::generateFlare()
{
    // Only decorations with active light sources produce flares.
    Lumobj *lum = lumobj();
    if(!lum) return; // Huh?

    // Is the point in range?
    double distance = R_ViewerLumobjDistance(lum->indexInMap());
    if(distance > lum->maxDistance())
        return;

    MaterialSnapshotDecoration const &matDecor = materialDecoration();

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
    if(matDecor.elevation < 2 && ::angleFadeFactor > 0) // Close the surface?
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

float Decoration::lightLevelAtOrigin() const
{
    return bspLeafAtOrigin().sector().lightLevel();
}

MaterialSnapshotDecoration const &Decoration::materialDecoration() const
{
    return source().materialDecoration();
}

Lumobj *Decoration::lumobj() const
{
    return map().lumobj(_lumIdx);
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
