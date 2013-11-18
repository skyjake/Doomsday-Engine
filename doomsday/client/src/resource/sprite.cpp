/** @file sprite.cpp  3D-Sprite resource.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "resource/sprites.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "TextureManifest"

#  include "gl/gl_tex.h"
#  include "gl/gl_texmanager.h"

#  include "Lumobj"
#  include "render/billboard.h"
#endif

using namespace de;

Sprite::Sprite()
    : _rotate(0)
{
    zap(_mats);
    zap(_flip);
}

Material *Sprite::material(int rotation, bool *flipX, bool *flipY) const
{
    if(flipX) *flipX = false;
    if(flipY) *flipY = false;

    if(rotation < 0 || rotation >= max_angles)
    {
        return 0;
    }

    if(flipX) *flipX = CPP_BOOL(_flip[rotation]);

    return _mats[rotation];
}

Material *Sprite::material(angle_t mobjAngle, angle_t angleToEye,
    bool noRotation, bool *flipX, bool *flipY) const
{
    int rotation = 0; // Use single rotation for all viewing angles (default).

    if(!noRotation && _rotate)
    {
        // Rotation is determined by the relative angle to the viewer.
        rotation = (angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) >> 29;
    }

    return material(rotation, flipX, flipY);
}

#ifdef __CLIENT__
Lumobj *Sprite::generateLumobj() const
{
    // Always use rotation zero.
    /// @todo We could do better here...
    Material *mat = material();
    if(!mat) return 0;

    // Ensure we have up-to-date information about the material.
    MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec());
    if(!ms.hasTexture(MTU_PRIMARY)) return 0; // Unloadable texture?
    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();

    pointlight_analysis_t const *pl = reinterpret_cast<pointlight_analysis_t const *>(ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::BrightPointAnalysis));
    if(!pl) throw Error("Sprite::generateLumobj", QString("Texture \"%1\" has no BrightPointAnalysis").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()));

    // Apply the auto-calculated color.
    return &(new Lumobj(Vector3d(), pl->brightMul, pl->color.rgb))
                    ->setZOffset(-tex.origin().y - pl->originY * ms.height());
}
#endif // __CLIENT__
