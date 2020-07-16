/** @file materialshinelayer.cpp  Logical material, shine/reflection layer.
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

#include "doomsday/world/shinetexturemateriallayer.h"
#include "doomsday/res/texturescheme.h"
#include "doomsday/res/textures.h"

using namespace de;

namespace world {

static res::Uri findTextureForShineStage(const ded_shine_stage_t &def, bool findMask)
{
    if (res::Uri *resourceUri = (findMask? def.maskTexture : def.texture))
    {
        try
        {
            return res::Textures::get()
                       .textureScheme(findMask? "Masks" : "Reflections")
                           .findByResourceUri(*resourceUri)
                               .composeUri();
        }
        catch (const res::TextureScheme::NotFoundError &)
        {} // Ignore this error.
    }
    return res::Uri();
}

ShineTextureMaterialLayer::AnimationStage::AnimationStage(const res::Uri &texture, int tics,
    float variance, const res::Uri &maskTexture, blendmode_t blendMode, float opacity,
    const Vec3f &minColor, const Vec2f &maskDimensions)
    : TextureMaterialLayer::AnimationStage(texture, tics, variance, 0, 0, Vec2f(0, 0),
                                           maskTexture, maskDimensions, blendMode, opacity)
    , minColor(minColor)
{
    //set("minColor", new ArrayValue(minColor));

}

ShineTextureMaterialLayer::AnimationStage::AnimationStage(const AnimationStage &other)
    : TextureMaterialLayer::AnimationStage(other)
{}

ShineTextureMaterialLayer::AnimationStage::~AnimationStage()
{}

void ShineTextureMaterialLayer::AnimationStage::resetToDefaults()
{
    TextureMaterialLayer::AnimationStage::resetToDefaults();
    //addArray("minColor", new ArrayValue(Vec3f(0.0f)));
    minColor = Vec3f();
}

ShineTextureMaterialLayer::AnimationStage *
ShineTextureMaterialLayer::AnimationStage::fromDef(const ded_shine_stage_t &def)
{
    const res::Uri texture     = findTextureForShineStage(def, false/*not mask*/);
    const res::Uri maskTexture = findTextureForShineStage(def, true/*mask*/);

    return new AnimationStage(texture, def.tics, def.variance, maskTexture,
                              def.blendMode, def.shininess, Vec3f(def.minColor),
                              Vec2f(def.maskWidth, def.maskHeight));
}

// ------------------------------------------------------------------------------------

ShineTextureMaterialLayer::ShineTextureMaterialLayer()
    : TextureMaterialLayer()
{}

ShineTextureMaterialLayer::~ShineTextureMaterialLayer()
{}

ShineTextureMaterialLayer *ShineTextureMaterialLayer::fromDef(const ded_reflection_t &layerDef)
{
    auto *layer = new ShineTextureMaterialLayer();
    // Only the one stage.
    layer->_stages.append(AnimationStage::fromDef(layerDef.stage));
    return layer;
}

int ShineTextureMaterialLayer::addStage(const ShineTextureMaterialLayer::AnimationStage &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

String ShineTextureMaterialLayer::describe() const
{
    return "Shine layer";
}

} // namespace world
