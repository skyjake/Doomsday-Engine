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

MaterialShineLayer::AnimationStage::AnimationStage(Texture *texture, int tics, float variance,
    Texture *maskTexture, blendmode_t blendMode, float shininess, Vector3f const &minColor,
    Vector2f const &maskDimensions)
    : Stage(tics, variance)
    , _texture      (texture)
    , maskTexture   (maskTexture)
    , blendMode     (blendMode)
    , shininess     (shininess)
    , minColor      (minColor)
    , maskDimensions(maskDimensions)
{}

MaterialShineLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : Stage(other)
    , _texture      (other._texture)
    , maskTexture   (other.maskTexture)
    , blendMode     (other.blendMode)
    , shininess     (other.shininess)
    , minColor      (other.minColor)
    , maskDimensions(other.maskDimensions)
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
    return _texture;
}

String MaterialShineLayer::AnimationStage::description() const
{
    String const path     = (_texture    ? _texture  ->manifest().composeUri().asText() : "(prev)");
    String const maskPath = (maskTexture? maskTexture->manifest().composeUri().asText() : "(none)");

    return String("Texture:\"%1\" MaskTexture:\"%2\" Tics:%3 (~%4)"
                  "\n      Shininess:%5 BlendMode:%6 MaskDimensions:%7"
                  "\n      MinColor:%8")
               .arg(path)
               .arg(maskPath)
               .arg(tics)
               .arg(variance, 0, 'g', 2)
               .arg(shininess, 0, 'g', 2)
               .arg(R_NameForBlendMode(blendMode))
               .arg(maskDimensions.asText())
               .arg(minColor.asText());
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
