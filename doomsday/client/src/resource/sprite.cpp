/** @file sprite.cpp  3D-Sprite resource.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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
#include "resource/sprite.h"

#ifdef __CLIENT__
#  include "TextureManifest"

#  include "gl/gl_tex.h" // pointlight_analysis_t

#  include "Lumobj"
#  include "render/billboard.h" // Rend_SpriteMaterialSpec
#endif
#include <de/Log>

using namespace de;

/**
 * @todo optimize: allocate only those ViewAngles that are in use.
 */
DENG2_PIMPL_NOREF(Sprite)
{
    bool haveRotations;    ///< @c true= use all view angles and not just rotation 0.
    ViewAngles viewAngles;

    Instance()
        : haveRotations(false)
        , viewAngles(max_angles)
    {}

    Instance(Instance const &other)
        : de::IPrivate()
        , haveRotations(other.haveRotations)
        , viewAngles(other.viewAngles)
    {}
};

Sprite::Sprite() : d(new Instance)
{}

Sprite::Sprite(Sprite const &other) : d(new Instance(*other.d))
{}

Sprite &Sprite::operator = (Sprite const &other)
{
    d.reset(new Instance(*other.d));
    return *this;
}

bool Sprite::hasViewAngle(int rotation) const
{
    if(rotation >= 0 && rotation < d->viewAngles.count())
    {
        return d->viewAngles[rotation].material != 0;
    }
    return false;
}

void Sprite::newViewAngle(Material *material, int rotation, bool mirrorX)
{
    DENG2_ASSERT(rotation <= max_angles);

    if(rotation <= 0)
    {
        // Use only one view angle for all rotations.
        d->haveRotations = false;
        for(int i = 0; i < d->viewAngles.count(); ++i)
        {
            ViewAngle &vAngle = d->viewAngles[i];
            vAngle.material = material;
            vAngle.mirrorX  = mirrorX;
        }
        return;
    }

    rotation--; // Make 0 based.

    d->haveRotations = true;
    ViewAngle &vAngle = d->viewAngles[rotation];
    vAngle.material = material;
    vAngle.mirrorX  = mirrorX;
}

Sprite::ViewAngle const &Sprite::viewAngle(int rotation) const
{
    LOG_AS("Sprite::viewAngle");
    if(rotation >= 0 && rotation < d->viewAngles.count())
    {
        return d->viewAngles[rotation];
    }
    /// @throw MissingViewAngle Specified an invalid rotation.
    throw MissingViewAngleError("Sprite::viewAngle", "Invalid rotation " + String::number(rotation));
}

Sprite::ViewAngle const &Sprite::closestViewAngle(angle_t mobjAngle, angle_t angleToEye,
    bool noRotation) const
{
    int rotation = 0; // Use single rotation for all viewing angles (default).

    if(!noRotation && d->haveRotations)
    {
        // Rotation is determined by the relative angle to the viewer.
        rotation = ((angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) - (unsigned) (ANGLE_180 / 16)) >> 28;
    }

    return viewAngle(rotation);
}

Sprite::ViewAngles const &Sprite::viewAngles() const
{
    return d->viewAngles;
}

#ifdef __CLIENT__
double Sprite::visualRadius() const
{
    if(hasViewAngle(0))
    {
        MaterialAnimator &matAnimator = viewAngle(0).material->getAnimator(Rend_SpriteMaterialSpec());
        // Ensure we've up to date info about the material.
        matAnimator.prepare();
        return matAnimator.dimensions().x / 2;
    }
    return 0;
}

Lumobj *Sprite::generateLumobj() const
{
    LOG_AS("Sprite::generateLumobj");

    // Always use rotation zero.
    /// @todo We could do better here...
    if(!hasViewAngle(0)) return nullptr;

    MaterialAnimator &matAnimator = viewAngle(0).material->getAnimator(Rend_SpriteMaterialSpec());

    // Ensure we have up-to-date information about the material.
    matAnimator.prepare();

    TextureVariant *texture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    if(!texture) return nullptr;  // Unloadable texture?

    auto const *pl = (pointlight_analysis_t const *)texture->base().analysisDataPointer(Texture::BrightPointAnalysis);
    if(!pl)
    {
        LOGDEV_RES_WARNING("Texture \"%s\" has no BrightPointAnalysis")
                << texture->base().manifest().composeUri();
        return nullptr;
    }

    // Apply the auto-calculated color.
    return &(new Lumobj(Vector3d(), pl->brightMul, pl->color.rgb))
                    ->setZOffset(-texture->base().origin().y - pl->originY * matAnimator.dimensions().y);
}
#endif  // __CLIENT__
