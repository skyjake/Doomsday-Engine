/** @file lightdecoration.cpp  World surface light decoration.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/lightdecoration.h"
#include "render/rend_main.h" // Rend_ApplyLightAdaptation

#include "world/convexsubspace.h"
#include "world/subsector.h"
#include "world/map.h"
#include "world/surface.h"
#include "def_main.h"

#include <doomsday/console/var.h>

using namespace de;

static float angleFadeFactor = .1f; ///< cvar
static float brightFactor    = 1;   ///< cvar

LightDecoration::LightDecoration(const MaterialAnimator::Decoration &source, const Vec3d &origin)
    : Decoration(source, origin)
    , Source()
{}

String LightDecoration::description() const
{
    String desc;
#ifdef DE_DEBUG
    desc.prepend(Stringf(_E(b) "LightDecoration " _E(.) "[%p]\n", this));
#endif
    return Decoration::description() + "\n" + desc;
}

float LightDecoration::occlusion(const Vec3d &eye) const
{
    // Halo brightness drops as the angle gets too big.
    if (source().elevation() < 2 && ::angleFadeFactor > 0) // Close the surface?
    {
        const Vec3d vecFromOriginToEye = (origin() - eye).normalize();

        auto dot = float( -surface().normal().dot(vecFromOriginToEye) );
        if (dot < ::angleFadeFactor / 2)
        {
            return 0;
        }
        else if (dot < 3 * ::angleFadeFactor)
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
    if (de::fequal(min, max)) return 1;
    return de::clamp(0.f, (lightlevel - min) / float(max - min), 1.f);
}

Lumobj *LightDecoration::generateLumobj() const
{
    // Decorations with zero color intensity produce no light.
    if (source().color() == Vec3f(0.0f))
        return nullptr;

    auto *subspace = bspLeafAtOrigin().subspacePtr();
    if (!subspace) return nullptr;

    // Does it pass the ambient light limitation?
    float intensity = subspace->subsector().as<Subsector>().lightSourceIntensity();
    Rend_ApplyLightAdaptation(intensity);

    float lightLevels[2];
    source().lightLevels(lightLevels[0], lightLevels[1]);

    intensity = checkLightLevel(intensity, lightLevels[0], lightLevels[1]);
    if (intensity < .0001f)
        return nullptr;

    // Apply the brightness factor (was calculated using sector lightlevel).
    float fadeMul = intensity * ::brightFactor;
    if (fadeMul <= 0)
        return nullptr;

    Lumobj *lum = new Lumobj(origin(), source().radius(), source().color() * fadeMul);

    lum->setSource(this);

    lum->setMaxDistance (MAX_DECOR_DISTANCE)
        .setLightmap    (Lumobj::Side, source().tex())
        .setLightmap    (Lumobj::Down, source().floorTex())
        .setLightmap    (Lumobj::Up,   source().ceilTex())
        .setFlareSize   (source().flareSize())
        .setFlareTexture(source().flareTex());

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
