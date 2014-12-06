/** @file materialdetailtexturelayer.cpp  Logical material, detail-texture layer.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#include "resource/materialdetailtexturelayer.h"
#include "TextureScheme"
#include "dd_main.h"

using namespace de;

static Texture *findTextureForDetailLayerStage(ded_detail_stage_t const &def)
{
    try
    {
        if(def.texture)
        {
            return &App_ResourceSystem().textureScheme("Details")
                        .findByResourceUri(*def.texture).texture();
        }
    }
    catch(TextureManifest::MissingTextureError const &)
    {} // Ignore this error.
    catch(TextureScheme::NotFoundError const &)
    {} // Ignore this error.
    return nullptr;
}

MaterialDetailTextureLayer::AnimationStage::AnimationStage(Texture *texture, int tics,
    float variance, float scale, float strength, float maxDistance)
    : Stage(tics, variance)
    , _texture   (texture)
    , scale      (scale)
    , strength   (strength)
    , maxDistance(maxDistance)
{}

MaterialDetailTextureLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Stage(other)
    , _texture   (other._texture)
    , scale      (other.scale)
    , strength   (other.strength)
    , maxDistance(other.maxDistance)
{}

MaterialDetailTextureLayer::AnimationStage *
MaterialDetailTextureLayer::AnimationStage::fromDef(ded_detail_stage_t const &def)
{
    Texture *texture = findTextureForDetailLayerStage(def);
    return new AnimationStage(texture, def.tics, def.variance,
                              def.scale, def.strength, def.maxDistance);
}

Texture *MaterialDetailTextureLayer::AnimationStage::texture() const
{
    return _texture;
}

String MaterialDetailTextureLayer::AnimationStage::description() const
{
    String const path = (_texture? _texture->manifest().composeUri().asText() : "(prev)");

    return String("Texture:\"%1\" Tics:%2 (~%3)"
                  "\n       Scale:%4 Strength:%5 MaxDistance:%6")
               .arg(path)
               .arg(tics)
               .arg(variance,    0, 'g', 2)
               .arg(scale,       0, 'g', 2)
               .arg(strength,    0, 'g', 2)
               .arg(maxDistance, 0, 'g', 2);
}

// ------------------------------------------------------------------------------------

MaterialDetailTextureLayer *MaterialDetailTextureLayer::fromDef(ded_detailtexture_t const &layerDef)
{
    auto *layer = new MaterialDetailTextureLayer();
    // Only the one stage.
    layer->_stages.append(AnimationStage::fromDef(layerDef.stage));
    return layer;
}

int MaterialDetailTextureLayer::addStage(MaterialDetailTextureLayer::AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

MaterialDetailTextureLayer::AnimationStage &MaterialDetailTextureLayer::stage(int index) const
{
    return static_cast<AnimationStage &>(Layer::stage(index));
}

String MaterialDetailTextureLayer::describe() const
{
    return "Detail texture layer";
}
