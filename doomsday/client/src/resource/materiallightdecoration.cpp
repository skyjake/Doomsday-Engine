/** @file materiallightdecoration.cpp   Logical material, light decoration.
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

#include "resource/materiallightdecoration.h"

#include <doomsday/defs/material.h>
#include "clientapp.h"

using namespace de;

static inline ResourceSystem &resSys()
{
    return ClientApp::resourceSystem();
}

MaterialLightDecoration::AnimationStage::AnimationStage(int tics, float variance,
    Vector2f const &origin, float elevation, Vector3f const &color, float radius,
    float haloRadius, LightRange const &lightLevels, Texture *ceilingTexture,
    Texture *floorTexture, Texture *texture, Texture *flareTexture, int sysFlareIdx)
    : Stage(tics, variance)
    , origin     (origin)
    , elevation  (elevation)
    , color      (color)
    , radius     (radius)
    , haloRadius (haloRadius)
    , lightLevels(lightLevels)
    , tex        (texture)
    , floorTex   (floorTexture)
    , ceilTex    (ceilingTexture)
    , flareTex   (flareTexture)
    , sysFlareIdx(sysFlareIdx)
{}

MaterialLightDecoration::AnimationStage::AnimationStage(AnimationStage const &other)
    : Stage(other)
    , origin     (other.origin)
    , elevation  (other.elevation)
    , color      (other.color)
    , radius     (other.radius)
    , haloRadius (other.haloRadius)
    , lightLevels(other.lightLevels)
    , tex        (other.tex)
    , floorTex   (other.floorTex)
    , ceilTex    (other.ceilTex)
    , flareTex   (other.flareTex)
    , sysFlareIdx(other.sysFlareIdx)
{}

MaterialLightDecoration::AnimationStage *
MaterialLightDecoration::AnimationStage::fromDef(Record const &stageDef)
{
    Texture *lightmapUp   = resSys().texture("Lightmaps", de::Uri(stageDef.gets("lightmapUp"  ), RC_NULL));
    Texture *lightmapDown = resSys().texture("Lightmaps", de::Uri(stageDef.gets("lightmapDown"), RC_NULL));
    Texture *lightmapSide = resSys().texture("Lightmaps", de::Uri(stageDef.gets("lightmapSide"), RC_NULL));

    int haloTextureIndex  = stageDef.geti("haloTextureIndex");
    Texture *haloTexture  = nullptr;
    de::Uri const haloTextureUri(stageDef.gets("haloTexture"), RC_NULL);
    if(!haloTextureUri.isEmpty())
    {
        // Select a system flare by numeric identifier?
        if(haloTextureUri.path().length() == 1 &&
           haloTextureUri.path().toStringRef().first().isDigit())
        {
            haloTextureIndex = haloTextureUri.path().toStringRef().first().digitValue();
        }
        else
        {
            haloTexture = resSys().texture("Flaremaps", haloTextureUri);
        }
    }

    return new AnimationStage(stageDef.geti("tics"), stageDef.getf("variance"),
                              Vector2f(stageDef.geta("origin")), stageDef.getf("elevation"),
                              Vector3f(stageDef.geta("color")), stageDef.getf("radius"),
                              stageDef.getf("haloRadius"),
                              LightRange(Vector2f(stageDef.geta("lightLevels"))),
                              lightmapUp, lightmapDown, lightmapSide,
                              haloTexture, haloTextureIndex);
}

String MaterialLightDecoration::AnimationStage::description() const
{
    return String(_E(l) "Tics: ")      + _E(.) + (tics > 0? String("%1 (~%2)").arg(tics).arg(variance, 0, 'g', 2) : "-1")
                + _E(l) " Origin: "      _E(.) + origin.asText()
                + _E(l) " Elevation: "   _E(.) + String::number(elevation, 'f', 2)
                + _E(l) " LightLevels: " _E(.) + lightLevels.asText()
                + _E(l) "\nColor: "      _E(.) + color.asText()
                + _E(l) " Radius: "      _E(.) + String::number(radius, 'f', 2)
                + _E(l) " HaloRadius: "  _E(.) + String::number(haloRadius, 'f', 2);
}

// ------------------------------------------------------------------------------------

MaterialLightDecoration::MaterialLightDecoration(Vector2i const &patternSkip, Vector2i const &patternOffset)
    : Decoration(patternSkip, patternOffset)
{}

MaterialLightDecoration::~MaterialLightDecoration()
{}

MaterialLightDecoration *MaterialLightDecoration::fromDef(Record const &definition)
{
    defn::MaterialDecoration decorDef(definition);

    auto *decor = new MaterialLightDecoration(Vector2i(decorDef.geta("patternSkip")),
                                              Vector2i(decorDef.geta("patternOffset")));
    for(int i = 0; i < decorDef.stageCount(); ++i)
    {
        decor->_stages.append(AnimationStage::fromDef(decorDef.stage(i)));
    }
    return decor;
}

int MaterialLightDecoration::addStage(AnimationStage const &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

MaterialLightDecoration::AnimationStage &MaterialLightDecoration::stage(int index) const
{
    return static_cast<AnimationStage &>(Decoration::stage(index));
}

String MaterialLightDecoration::describe() const
{
    return "Light decoration";
}
