/** @file lightdecoration.cpp World surface light decoration.
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

#include "render/lightdecoration.h"

using namespace de;

static float angleFadeFactor = .1f; ///< cvar
static float brightFactor    = 1;   ///< cvar

LightDecoration::LightDecoration(MaterialSnapshotDecoration &source, const Vector3d &origin)
    : Decoration(source, origin), Source()
{}

float LightDecoration::occlusion(Vector3d const &eye) const
{
    // Halo brightness drops as the angle gets too big.
    if(source().elevation < 2 && ::angleFadeFactor > 0) // Close the surface?
    {
        Vector3d const vecFromOriginToEye = (origin() - eye).normalize();

        float dot = float( -surface().normal().dot(vecFromOriginToEye) );
        if(dot < ::angleFadeFactor / 2)
        {
            return 0;
        }
        else if(dot < 3 * ::angleFadeFactor)
        {
            return (dot - ::angleFadeFactor / 2) / (2.5f * ::angleFadeFactor);
        }
    }
    return 1;
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

Lumobj *LightDecoration::generateLumobj() const
{
    // Decorations with zero color intensity produce no light.
    if(source().color == Vector3f(0, 0, 0))
        return 0;

    // Does it pass the ambient light limitation?
    float lightLevel = bspLeafAtOrigin().sector().lightLevel();
    Rend_ApplyLightAdaptation(lightLevel);

    float intensity = checkLightLevel(lightLevel,
        source().lightLevels[0], source().lightLevels[1]);
    if(intensity < .0001f)
        return 0;

    // Apply the brightness factor (was calculated using sector lightlevel).
    float fadeMul = intensity * ::brightFactor;
    if(fadeMul <= 0)
        return 0;

    Lumobj *lum = new Lumobj(origin(), source().radius, source().color * fadeMul);

    lum->setSource(this);

    lum->setMaxDistance (MAX_DECOR_DISTANCE)
        .setLightmap    (Lumobj::Side, source().tex)
        .setLightmap    (Lumobj::Down, source().floorTex)
        .setLightmap    (Lumobj::Up,   source().ceilTex)
        .setFlareSize   (source().flareSize)
        .setFlareTexture(source().flareTex);

    return lum;
}

void LightDecoration::consoleRegister() // static
{
    C_VAR_FLOAT("rend-light-decor-angle",  &::angleFadeFactor, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &::brightFactor,    0, 0, 10);
}

float LightDecoration::angleFadeFactor() // static
{
    return ::angleFadeFactor;
}

float LightDecoration::brightFactor() // static
{
    return ::brightFactor;
}
