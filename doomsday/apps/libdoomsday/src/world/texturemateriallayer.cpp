/** @file texturemateriallayer.cpp   Logical material, texture layer.
 *
 * @authors Copyright © 2011-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/texturemateriallayer.h"
#include "doomsday/defs/material.h"

using namespace de;

namespace world {

TextureMaterialLayer::AnimationStage::AnimationStage(de::Uri const &texture, int tics,
    float variance, float glowStrength, float glowStrengthVariance, Vector2f const origin,
    de::Uri const &maskTexture, Vector2f const &maskDimensions, blendmode_t blendMode, float opacity)
    : Record()
    , Stage(tics, variance)
{
    resetToDefaults();

    set("origin", new ArrayValue(origin));
    set("texture", texture.compose());
    set("maskTexture", maskTexture.compose());
    set("maskDimensions", new ArrayValue(maskDimensions));
    set("blendMode", blendMode);
    set("opacity", opacity);

    set("glowStrength", glowStrength);
    set("glowStrengthVariance", glowStrengthVariance);
}

TextureMaterialLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Record(other)
    , Stage(other)
{}

TextureMaterialLayer::AnimationStage::~AnimationStage()
{}

void TextureMaterialLayer::AnimationStage::resetToDefaults()
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

TextureMaterialLayer::AnimationStage *
TextureMaterialLayer::AnimationStage::fromDef(Record const &stageDef)
{
    return new AnimationStage(de::Uri(stageDef.gets("texture"), RC_NULL),
                              stageDef.geti("tics"), stageDef.getf("variance"),
                              stageDef.getf("glowStrength"),
                              stageDef.getf("glowStrengthVariance"),
                              Vector2f(stageDef.geta("texOrigin")));
}

String TextureMaterialLayer::AnimationStage::description() const
{
    /// @todo Record::asText() formatting is not intended for end users.
    return asText();
}

// ------------------------------------------------------------------------------------

TextureMaterialLayer *TextureMaterialLayer::fromDef(Record const &definition)
{
    defn::MaterialLayer layerDef(definition);
    auto *layer = new TextureMaterialLayer();
    for (int i = 0; i < layerDef.stageCount(); ++i)
    {
        layer->_stages.append(AnimationStage::fromDef(layerDef.stage(i)));
    }
    return layer;
}

int TextureMaterialLayer::addStage(TextureMaterialLayer::AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

TextureMaterialLayer::AnimationStage &TextureMaterialLayer::stage(int index) const
{
    return static_cast<AnimationStage &>(Layer::stage(index));
}

bool TextureMaterialLayer::hasGlow() const
{
    for (int i = 0; i < stageCount(); ++i)
    {
        if (stage(i).getf("glowStrength") > .0001f)
            return true;
    }
    return false;
}

String TextureMaterialLayer::describe() const
{
    return "Texture layer";
}

} // namespace world
