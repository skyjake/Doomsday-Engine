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

using namespace de;

DENG2_PIMPL_NOREF(MaterialTextureLayer::AnimationStage)
{
    Texture *texture = nullptr;  ///< Not owned.
    Vector2f origin;
    float glowStrength = 0;
    float glowStrengthVariance = 0;

    Instance() {}
    Instance(Instance const &other)
        : IPrivate()
        , texture             (other.texture)
        , origin              (other.origin)
        , glowStrength        (other.glowStrength)
        , glowStrengthVariance(other.glowStrengthVariance)
    {}
};

MaterialTextureLayer::AnimationStage::AnimationStage(Texture *texture, int tics,
    float variance, float glowStrength, float glowStrengthVariance, Vector2f const origin)
    : Stage(tics, variance)
    , d(new Instance)
{
    d->texture              = texture;
    d->origin               = origin;
    d->glowStrength         = glowStrength;
    d->glowStrengthVariance = glowStrengthVariance;
}

MaterialTextureLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Stage(other)
    , d(new Instance(*other.d))
{}

MaterialTextureLayer::AnimationStage::~AnimationStage()
{}

MaterialTextureLayer::AnimationStage *
MaterialTextureLayer::AnimationStage::fromDef(ded_material_layer_stage_t const &def)
{
    Texture *texture = (def.texture? App_ResourceSystem().texturePtr(*def.texture) : nullptr);
    return new AnimationStage(texture, def.tics, def.variance, def.glowStrength,
                              def.glowStrengthVariance, Vector2f(def.texOrigin));
}

Texture *MaterialTextureLayer::AnimationStage::texture() const
{
    return d->texture;
}

Vector2f const &MaterialTextureLayer::AnimationStage::origin() const
{
    return d->origin;
}

float MaterialTextureLayer::AnimationStage::glowStrength() const
{
    return d->glowStrength;
}

float MaterialTextureLayer::AnimationStage::glowStrengthVariance() const
{
    return d->glowStrengthVariance;
}

String MaterialTextureLayer::AnimationStage::description() const
{
    String const path = (d->texture? d->texture->manifest().composeUri().asText() : "(prev)");

    return String(_E(l)   "Texture: \"") + _E(.) + path + "\""
                + _E(l) + " Tics: "      + _E(.) + (tics > 0? String("%1 (~%2)").arg(tics).arg(variance, 0, 'f', 2) : "-1")
                + _E(l) + " Origin: "    + _E(.) + d->origin.asText()
                + _E(l) + " Glow: "      + _E(.) + String("%1 (~%2)").arg(d->glowStrength, 0, 'f', 2).arg(d->glowStrengthVariance, 0, 'f', 2);
}

void MaterialTextureLayer::AnimationStage::setTexture(Texture *newTexture)
{
    d->texture = newTexture;
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
        if(stage(i).glowStrength() > .0001f)
            return true;
    }
    return false;
}

String MaterialTextureLayer::describe() const
{
    return "Texture layer";
}
