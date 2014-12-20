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

#include "resource/materialshinelayer.h"
#include "dd_main.h"
#include "TextureScheme"

using namespace de;

static de::Uri findTextureForShineStage(ded_shine_stage_t const &def, bool findMask)
{
    if(de::Uri *resourceUri = (findMask? def.maskTexture : def.texture))
    {
        try
        {
            return App_ResourceSystem()
                       .textureScheme(findMask? "Masks" : "Reflections")
                           .findByResourceUri(*resourceUri)
                               .composeUri();
        }
        catch(TextureScheme::NotFoundError const &)
        {} // Ignore this error.
    }
    return de::Uri();
}

MaterialShineLayer::AnimationStage::AnimationStage(de::Uri const &texture, int tics,
    float variance, de::Uri const &maskTexture, blendmode_t blendMode, float opacity,
    Vector3f const &minColor, Vector2f const &maskDimensions)
    : MaterialTextureLayer::AnimationStage(texture, tics, variance, 0, 0, Vector2f(0, 0),
                                           maskTexture, maskDimensions, blendMode, opacity)
{
    set("minColor", new ArrayValue(minColor));
}

MaterialShineLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : MaterialTextureLayer::AnimationStage(other)
{}

MaterialShineLayer::AnimationStage::~AnimationStage()
{}

void MaterialShineLayer::AnimationStage::resetToDefaults()
{
    MaterialTextureLayer::AnimationStage::resetToDefaults();
    addArray("minColor", new ArrayValue(Vector3f(0, 0, 0)));
}

MaterialShineLayer::AnimationStage *
MaterialShineLayer::AnimationStage::fromDef(ded_shine_stage_t const &def)
{
    de::Uri const texture     = findTextureForShineStage(def, false/*not mask*/);
    de::Uri const maskTexture = findTextureForShineStage(def, true/*mask*/);

    return new AnimationStage(texture, def.tics, def.variance, maskTexture,
                              def.blendMode, def.shininess, Vector3f(def.minColor),
                              Vector2f(def.maskWidth, def.maskHeight));
}

// ------------------------------------------------------------------------------------

MaterialShineLayer::MaterialShineLayer()
    : MaterialTextureLayer()
{}

MaterialShineLayer::~MaterialShineLayer()
{}

MaterialShineLayer *MaterialShineLayer::fromDef(ded_reflection_t const &layerDef)
{
    auto *layer = new MaterialShineLayer();
    // Only the one stage.
    layer->_stages.append(AnimationStage::fromDef(layerDef.stage));
    return layer;
}

int MaterialShineLayer::addStage(MaterialShineLayer::AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

String MaterialShineLayer::describe() const
{
    return "Shine layer";
}
