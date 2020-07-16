/** @file surface.cpp  World map surface.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "world/surface.h"
#include "world/map.h"
#include "gl/gl_tex.h"
#include "render/rend_main.h"
#include "render/decoration.h"
#include "resource/clienttexture.h"
#include "dd_loop.h" // frameTimePos

#include <doomsday/res/texturemanifest.h>

using namespace de;

Surface::Surface(world::MapElement &owner, float opacity, const Vec3f &color)
    : world::Surface(owner, opacity, color)
{
    audienceForOriginChange() += [this]() {
        if (world::World::ddMapSetup)
        {
            // During map setup we'll apply this immediately to the visual origin also.
            _originSmoothed = origin();
            _originSmoothedDelta = Vec2f();

            _oldOrigin[0] = _oldOrigin[1] = origin();
        }
        else
        {
            map().scrollingSurfaces().insert(this);
        }
    };
}

Surface::~Surface()
{
    // Stop scroll interpolation for this surface.
    map().scrollingSurfaces().remove(this);
}

void Surface::notifyOriginSmoothedChanged()
{
    DE_NOTIFY_VAR(OriginSmoothedChange, i) i->surfaceOriginSmoothedChanged(*this);
}

MaterialAnimator *Surface::materialAnimator() const
{
    if (!hasMaterial()) return nullptr;

    if (!_matAnimator)
    {
        _matAnimator = &material().as<ClientMaterial>()
                              .getAnimator(Rend_MapSurfaceMaterialSpec());
    }
    return _matAnimator;
}

void Surface::resetLookups()
{
    _matAnimator = nullptr;
}

const Vec2f &Surface::originSmoothed() const
{
    return _originSmoothed;
}

const Vec2f &Surface::originSmoothedAsDelta() const
{
    return _originSmoothedDelta;
}

void Surface::lerpSmoothedOrigin()
{
    // $smoothmaterialorigin
    _originSmoothedDelta = _oldOrigin[0] * (1 - ::frameTimePos)
                             + origin() * ::frameTimePos - origin();

    // Visible material origin.
    _originSmoothed = origin() + _originSmoothedDelta;

    notifyOriginSmoothedChanged();
}

void Surface::resetSmoothedOrigin()
{
    // $smoothmaterialorigin
    _originSmoothed = _oldOrigin[0] = _oldOrigin[1] = origin();
    _originSmoothedDelta = Vec2f();

    notifyOriginSmoothedChanged();
}

void Surface::updateOriginTracking()
{
    // $smoothmaterialorigin
    _oldOrigin[0] = _oldOrigin[1];
    _oldOrigin[1] = origin();

    if (_oldOrigin[0] != _oldOrigin[1])
    {
        float moveDistance = de::abs(Vec2f(_oldOrigin[1] - _oldOrigin[0]).length());

        if (moveDistance >= MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            _oldOrigin[0] = _oldOrigin[1];
        }
    }
}

float Surface::glow(Vec3f &color) const
{
    if (!hasMaterial() || material().isSkyMasked())
    {
        color = Vec3f();
        return 0;
    }

    MaterialAnimator &matAnimator = *materialAnimator();

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    TextureVariant *texture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    if (!texture) return 0;
    const auto *avgColorAmplified = reinterpret_cast<const averagecolor_analysis_t *>(texture->base().analysisDataPointer(ClientTexture::AverageColorAmplifiedAnalysis));
    if (!avgColorAmplified)
    {
        return 0;
    }

    color = Vec3f(avgColorAmplified->color.rgb);
    return matAnimator.glowStrength() * glowFactor; // Global scale factor.
}

Map &Surface::map() const
{
    return world::Surface::map().as<Map>();
}
