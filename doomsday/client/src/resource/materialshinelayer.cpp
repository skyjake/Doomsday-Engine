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
#include "r_util.h" // R_NameForBlendMode

using namespace de;

static Texture *findTextureForShineLayerStage(ded_shine_stage_t const &def, bool findMask)
{
    if(de::Uri *resourceUri = (findMask? def.maskTexture : def.texture))
    {
        try
        {
            return &App_ResourceSystem()
                        .textureScheme(findMask? "Masks" : "Reflections")
                            .findByResourceUri(*resourceUri).texture();
        }
        catch(TextureManifest::MissingTextureError const &)
        {} // Ignore this error.
        catch(TextureScheme::NotFoundError const &)
        {} // Ignore this error.
    }
    return nullptr;
}

DENG2_PIMPL_NOREF(MaterialShineLayer::AnimationStage)
{
    Texture *texture = nullptr;      ///< Not owned.
    Texture *maskTexture = nullptr;  ///< Not owned.
    Vector2f maskDimensions;
    float shininess = 0;
    blendmode_t blendMode;
    Vector3f minColor;

    Instance() {}
    Instance(Instance const &other)
        : IPrivate()
        , texture       (other.texture)
        , maskTexture   (other.maskTexture)
        , maskDimensions(other.maskDimensions)
        , shininess     (other.shininess)
        , blendMode     (other.blendMode)
        , minColor      (other.minColor)
    {}
};

MaterialShineLayer::AnimationStage::AnimationStage(Texture *texture, int tics, float variance,
    Texture *maskTexture, blendmode_t blendMode, float shininess, Vector3f const &minColor,
    Vector2f const &maskDimensions)
    : Stage(tics, variance)
    , d(new Instance)
{
    d->texture        = texture;
    d->maskTexture    = maskTexture;
    d->maskDimensions = maskDimensions;
    d->shininess      = shininess;
    d->blendMode      = blendMode;
    d->minColor       = minColor;
}

MaterialShineLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Stage(other)
    , d(new Instance(*other.d))
{}

MaterialShineLayer::AnimationStage::~AnimationStage()
{}

MaterialShineLayer::AnimationStage *
MaterialShineLayer::AnimationStage::fromDef(ded_shine_stage_t const &def)
{
    Texture *texture     = findTextureForShineLayerStage(def, false/*not mask*/);
    Texture *maskTexture = findTextureForShineLayerStage(def, true/*mask*/);

    return new AnimationStage(texture, def.tics, def.variance, maskTexture,
                              def.blendMode, def.shininess, Vector3f(def.minColor),
                              Vector2f(def.maskWidth, def.maskHeight));
}

Texture *MaterialShineLayer::AnimationStage::texture() const
{
    return d->texture;
}

Texture *MaterialShineLayer::AnimationStage::maskTexture() const
{
    return d->maskTexture;
}

Vector2f const &MaterialShineLayer::AnimationStage::maskDimensions() const
{
    return d->maskDimensions;
}

float MaterialShineLayer::AnimationStage::shininess() const
{
    return d->shininess;
}

blendmode_t MaterialShineLayer::AnimationStage::blendMode() const
{
    return d->blendMode;
}

Vector3f const &MaterialShineLayer::AnimationStage::minColor() const
{
    return d->minColor;
}

String MaterialShineLayer::AnimationStage::description() const
{
    String const path     = (d->texture    ? d->texture    ->manifest().composeUri().asText() : "(prev)");
    String const maskPath = (d->maskTexture? d->maskTexture->manifest().composeUri().asText() : "(none)");

    return String(_E(l)   "Texture: \"")      + _E(.) + path + "\""
                + _E(l) + " MaskTexture: \""  + _E(.) + maskPath + "\""
                + _E(l) + " MaskDimensions: " + _E(.) + d->maskDimensions.asText()
                + _E(l) + "\nTics: "          + _E(.) + (tics > 0? String("%1 (~%2)").arg(tics).arg(variance, 0, 'f', 2) : "-1")
                + _E(l) + " Shininess: "      + _E(.) + String::number(d->shininess, 'f', 2)
                + _E(l) + " MinColor: "       + _E(.) + d->minColor.asText()
                + _E(l) + " BlendMode: "      + _E(.) + R_NameForBlendMode(d->blendMode);
}

// ------------------------------------------------------------------------------------

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

MaterialShineLayer::AnimationStage &MaterialShineLayer::stage(int index) const
{
    return static_cast<AnimationStage &>(Layer::stage(index));
}

String MaterialShineLayer::describe() const
{
    return "Shine layer";
}
