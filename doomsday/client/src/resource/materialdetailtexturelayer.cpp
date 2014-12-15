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

DENG2_PIMPL_NOREF(MaterialDetailTextureLayer::AnimationStage)
{
    Texture *texture = nullptr;  ///< Not owned.
    float scale = 1;
    float strength = 1;
    float maxDistance = 0;

    Instance() {}
    Instance(Instance const &other)
        : texture    (other.texture)
        , scale      (other.scale)
        , strength   (other.strength)
        , maxDistance(other.maxDistance)
    {}
};

MaterialDetailTextureLayer::AnimationStage::AnimationStage(Texture *texture, int tics,
    float variance, float scale, float strength, float maxDistance)
    : Stage(tics, variance)
    , d(new Instance)
{
    d->texture     = texture;
    d->scale       = scale;
    d->strength    = strength;
    d->maxDistance = maxDistance;
}

MaterialDetailTextureLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Stage(other)
    , d(new Instance(*other.d))
{}

MaterialDetailTextureLayer::AnimationStage::~AnimationStage()
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
    return d->texture;
}

float MaterialDetailTextureLayer::AnimationStage::scale() const
{
    return d->scale;
}

float MaterialDetailTextureLayer::AnimationStage::strength() const
{
    return d->strength;
}

float MaterialDetailTextureLayer::AnimationStage::maxDistance() const
{
    return d->maxDistance;
}

String MaterialDetailTextureLayer::AnimationStage::description() const
{
    String const path = (d->texture? d->texture->manifest().composeUri().asText() : "(prev)");

    return String(_E(l)   "Texture: \"")   + _E(.) + path + "\""
                + _E(l) + " Tics: "        + _E(.) + (tics > 0? String("%1 (~%2)").arg(tics).arg(variance, 0, 'f', 2) : "-1")
                + _E(l) + " Scale: "       + _E(.) + String::number(d->scale, 'f', 2)
                + _E(l) + " Strength: "    + _E(.) + String::number(d->strength, 'f', 2)
                + _E(l) + " MaxDistance: " + _E(.) + String::number(d->maxDistance, 'f', 2);
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
