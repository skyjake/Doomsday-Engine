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

static de::Uri findTextureForDetailStage(ded_detail_stage_t const &def)
{
    try
    {
        if(def.texture)
        {
            return App_ResourceSystem().textureScheme("Details")
                       .findByResourceUri(*def.texture)
                           .composeUri();
        }
    }
    catch(TextureScheme::NotFoundError const &)
    {} // Ignore this error.
    return de::Uri();
}

MaterialDetailLayer::AnimationStage::AnimationStage(de::Uri const &texture, int tics,
    float variance, float scale, float strength, float maxDistance)
    : MaterialTextureLayer::AnimationStage(texture, tics, variance)
{
    set("scale", scale);
    set("strength", strength);
    set("maxDistance", maxDistance);
}

MaterialDetailLayer::AnimationStage::AnimationStage(AnimationStage const &other)
    : MaterialTextureLayer::AnimationStage(other)
{}

MaterialDetailLayer::AnimationStage::~AnimationStage()
{}

void MaterialDetailLayer::AnimationStage::resetToDefaults()
{
    MaterialTextureLayer::AnimationStage::resetToDefaults();
    addNumber("scale", 1);
    addNumber("strength", 1);
    addNumber("maxDistance", 0);
}

MaterialDetailLayer::AnimationStage *
MaterialDetailLayer::AnimationStage::fromDef(ded_detail_stage_t const &def)
{
    de::Uri const texture = findTextureForDetailStage(def);
    return new AnimationStage(texture, def.tics, def.variance,
                              def.scale, def.strength, def.maxDistance);
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

String MaterialDetailLayer::describe() const
{
    return "Detail layer";
}
