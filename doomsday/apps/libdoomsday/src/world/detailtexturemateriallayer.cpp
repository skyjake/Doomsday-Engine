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

#include <doomsday/res/TextureScheme>
#include <doomsday/res/Textures>

using namespace de;

namespace world {

static de::Uri findTextureForDetailStage(ded_detail_stage_t const &def)
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
    catch (res::TextureScheme::NotFoundError const &)
    {} // Ignore this error.
    return de::Uri();
}

DetailTextureMaterialLayer::AnimationStage::AnimationStage(de::Uri const &texture, int tics,
    float variance, float scale, float strength, float maxDistance)
    : TextureMaterialLayer::AnimationStage(texture, tics, variance)
{
    set("scale", scale);
    set("strength", strength);
    set("maxDistance", maxDistance);
}

DetailTextureMaterialLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : TextureMaterialLayer::AnimationStage(other)
{}

DetailTextureMaterialLayer::AnimationStage::~AnimationStage()
{}

void DetailTextureMaterialLayer::AnimationStage::resetToDefaults()
{
    TextureMaterialLayer::AnimationStage::resetToDefaults();
    addNumber("scale", 1);
    addNumber("strength", 1);
    addNumber("maxDistance", 0);
}

DetailTextureMaterialLayer::AnimationStage *
DetailTextureMaterialLayer::AnimationStage::fromDef(ded_detail_stage_t const &def)
{
    de::Uri const texture = findTextureForDetailStage(def);
    return new AnimationStage(texture, def.tics, def.variance,
                              def.scale, def.strength, def.maxDistance);
}

// ------------------------------------------------------------------------------------

DetailTextureMaterialLayer *DetailTextureMaterialLayer::fromDef(ded_detailtexture_t const &layerDef)
{
    auto *layer = new DetailTextureMaterialLayer();
    // Only the one stage.
    layer->_stages.append(AnimationStage::fromDef(layerDef.stage));
    return layer;
}

int DetailTextureMaterialLayer::addStage(DetailTextureMaterialLayer::AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

String DetailTextureMaterialLayer::describe() const
{
    return "Detail layer";
}

} // namespace world
