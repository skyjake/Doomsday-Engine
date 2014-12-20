/** @file materialtexturelayer.cpp   Logical material, texture layer.
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

#include "resource/materialtexturelayer.h"
#include "dd_main.h"
#include "r_util.h" // R_NameForBlendMode

using namespace de;

MaterialTextureLayer::AnimationStage::AnimationStage(de::Uri const &texture, int tics,
    float variance, float glowStrength, float glowStrengthVariance, Vector2f const origin,
    de::Uri const &maskTexture, Vector2f const &maskDimensions, blendmode_t blendMode, float opacity)
    : Record()
    , Stage(tics, variance)
{
    resetToDefaults();

    set("origin", new ArrayValue(origin));
    set("texture", texture.asText());
    set("maskTexture", maskTexture.asText());
    set("maskDimensions", new ArrayValue(maskDimensions));
    set("blendMode", blendMode);
    set("opacity", opacity);

    set("glowStrength", glowStrength);
    set("glowStrengthVariance", glowStrengthVariance);
}

MaterialTextureLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Record(other)
    , Stage(other)
{}

MaterialTextureLayer::AnimationStage::~AnimationStage()
{}

void MaterialTextureLayer::AnimationStage::resetToDefaults()
{
    addArray ("origin", new ArrayValue(Vector2f(0, 0)));
    addText  ("texture", "");
    addText  ("maskTexture", "");
    addArray ("maskDimensions", new ArrayValue(Vector2f(0, 0)));
    addNumber("blendMode", BM_NORMAL);
    addNumber("opacity", 1);
    addNumber("glowStrength", 0);
    addNumber("glowStrengthVariance", 0);
}

MaterialTextureLayer::AnimationStage *
MaterialTextureLayer::AnimationStage::fromDef(ded_material_layer_stage_t const &def)
{
    de::Uri const texture = (def.texture? *def.texture : de::Uri());
    return new AnimationStage(texture, def.tics, def.variance, def.glowStrength,
                              def.glowStrengthVariance, Vector2f(def.texOrigin));
}

String MaterialTextureLayer::AnimationStage::description() const
{
    /// @todo Record::asText() formatting is not intended for end users.
    return asText();
}

// ------------------------------------------------------------------------------------

MaterialTextureLayer *MaterialTextureLayer::fromDef(ded_material_layer_t const &def)
{
    auto *layer = new MaterialTextureLayer();
    for(int i = 0; i < def.stages.size(); ++i)
    {
        layer->_stages.append(AnimationStage::fromDef(def.stages[i]));
    }
    return layer;
}

int MaterialTextureLayer::addStage(MaterialTextureLayer::AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

MaterialTextureLayer::AnimationStage &MaterialTextureLayer::stage(int index) const
{
    return static_cast<AnimationStage &>(Layer::stage(index));
}

bool MaterialTextureLayer::hasGlow() const
{
    for(int i = 0; i < stageCount(); ++i)
    {
        if(stage(i).getf("glowStrength") > .0001f)
            return true;
    }
    return false;
}

String MaterialTextureLayer::describe() const
{
    return "Texture layer";
}
