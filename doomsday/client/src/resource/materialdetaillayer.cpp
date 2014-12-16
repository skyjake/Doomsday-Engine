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

#include "resource/materialdetaillayer.h"
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

DENG2_PIMPL_NOREF(MaterialDetailLayer::AnimationStage)
{
    float scale = 1;
    float strength = 1;
    float maxDistance = 0;

    Instance() {}
    Instance(Instance const &other)
        : scale      (other.scale)
        , strength   (other.strength)
        , maxDistance(other.maxDistance)
    {}
};

MaterialDetailLayer::AnimationStage::AnimationStage(Texture *texture, int tics,
    float variance, float scale, float strength, float maxDistance)
    : MaterialTextureLayer::AnimationStage(texture, tics, variance)
    , d(new Instance)
{
    d->scale       = scale;
    d->strength    = strength;
    d->maxDistance = maxDistance;
}

MaterialDetailLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : MaterialTextureLayer::AnimationStage(other)
    , d(new Instance(*other.d))
{}

MaterialDetailLayer::AnimationStage::~AnimationStage()
{}

MaterialDetailLayer::AnimationStage *
MaterialDetailLayer::AnimationStage::fromDef(ded_detail_stage_t const &def)
{
    Texture *texture = findTextureForDetailLayerStage(def);
    return new AnimationStage(texture, def.tics, def.variance,
                              def.scale, def.strength, def.maxDistance);
}

float MaterialDetailLayer::AnimationStage::scale() const
{
    return d->scale;
}

float MaterialDetailLayer::AnimationStage::strength() const
{
    return d->strength;
}

float MaterialDetailLayer::AnimationStage::maxDistance() const
{
    return d->maxDistance;
}

String MaterialDetailLayer::AnimationStage::description() const
{
    return MaterialTextureLayer::AnimationStage::description()
                + _E(l) + "\nScale: "      + _E(.) + String::number(d->scale, 'f', 2)
                + _E(l) + " Strength: "    + _E(.) + String::number(d->strength, 'f', 2)
                + _E(l) + " MaxDistance: " + _E(.) + String::number(d->maxDistance, 'f', 2);
}

// ------------------------------------------------------------------------------------

MaterialDetailLayer *MaterialDetailLayer::fromDef(ded_detailtexture_t const &layerDef)
{
    auto *layer = new MaterialDetailLayer();
    // Only the one stage.
    layer->_stages.append(AnimationStage::fromDef(layerDef.stage));
    return layer;
}

int MaterialDetailLayer::addStage(MaterialDetailLayer::AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

MaterialDetailLayer::AnimationStage &MaterialDetailLayer::stage(int index) const
{
    return static_cast<AnimationStage &>(Layer::stage(index));
}

String MaterialDetailLayer::describe() const
{
    return "Detail layer";
}
