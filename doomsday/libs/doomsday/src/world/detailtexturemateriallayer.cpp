/** @file materialdetaillayer.cpp  Logical material, detail-texture layer.
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

#include "doomsday/world/detailtexturemateriallayer.h"

#include <doomsday/res/texturescheme.h>
#include <doomsday/res/textures.h>

using namespace de;

namespace world {

static res::Uri findTextureForDetailStage(const ded_detail_stage_t &def)
{
    try
    {
        if (def.texture)
        {
            return res::Textures::get().textureScheme("Details")
                       .findByResourceUri(*def.texture)
                           .composeUri();
        }
    }
    catch (const res::TextureScheme::NotFoundError &)
    {} // Ignore this error.
    return res::Uri();
}

DetailTextureMaterialLayer::AnimationStage::AnimationStage(const res::Uri &texture, int tics,
    float variance, float scale, float strength, float maxDistance)
    : TextureMaterialLayer::AnimationStage(texture, tics, variance)
    , scale(scale)
    , strength(strength)
    , maxDistance(maxDistance)
{
//    set("scale", scale);
//    set("strength", strength);
//    set("maxDistance", maxDistance);
}

DetailTextureMaterialLayer::AnimationStage::AnimationStage(const AnimationStage &other)
    : TextureMaterialLayer::AnimationStage(other)
{}

DetailTextureMaterialLayer::AnimationStage::~AnimationStage()
{}

void DetailTextureMaterialLayer::AnimationStage::resetToDefaults()
{
    TextureMaterialLayer::AnimationStage::resetToDefaults();
//    addNumber("scale", 1);
//    addNumber("strength", 1);
//    addNumber("maxDistance", 0);
    scale = 1;
    strength = 1;
    maxDistance = 0;
}

DetailTextureMaterialLayer::AnimationStage *
DetailTextureMaterialLayer::AnimationStage::fromDef(const ded_detail_stage_t &def)
{
    const res::Uri texture = findTextureForDetailStage(def);
    return new AnimationStage(texture, def.tics, def.variance,
                              def.scale, def.strength, def.maxDistance);
}

// ------------------------------------------------------------------------------------

DetailTextureMaterialLayer *DetailTextureMaterialLayer::fromDef(const ded_detailtexture_t &layerDef)
{
    auto *layer = new DetailTextureMaterialLayer();
    // Only the one stage.
    layer->_stages.append(AnimationStage::fromDef(layerDef.stage));
    return layer;
}

int DetailTextureMaterialLayer::addStage(const DetailTextureMaterialLayer::AnimationStage &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

String DetailTextureMaterialLayer::describe() const
{
    return "Detail layer";
}

} // namespace world
