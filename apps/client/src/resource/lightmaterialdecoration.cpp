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

#include "resource/lightmaterialdecoration.h"

#include <doomsday/defs/material.h>
#include <doomsday/res/textures.h>
#include "clientapp.h"

using namespace de;

LightMaterialDecoration::AnimationStage::AnimationStage(int tics, float variance,
    const Vec2f &origin, float elevation, const Vec3f &color, float radius,
    float haloRadius, const LightRange &lightLevels, ClientTexture *ceilingTexture,
    ClientTexture *floorTexture, ClientTexture *texture, ClientTexture *flareTexture, int sysFlareIdx)
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

LightMaterialDecoration::AnimationStage::AnimationStage(const AnimationStage &other)
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

LightMaterialDecoration::AnimationStage *
LightMaterialDecoration::AnimationStage::fromDef(const Record &stageDef)
{
    ClientTexture *lightmapUp   = static_cast<ClientTexture *>(res::Textures::get().tryFindTextureByResourceUri("Lightmaps", res::makeUri(stageDef.gets("lightmapUp"  ))));
    ClientTexture *lightmapDown = static_cast<ClientTexture *>(res::Textures::get().tryFindTextureByResourceUri("Lightmaps", res::makeUri(stageDef.gets("lightmapDown"))));
    ClientTexture *lightmapSide = static_cast<ClientTexture *>(res::Textures::get().tryFindTextureByResourceUri("Lightmaps", res::makeUri(stageDef.gets("lightmapSide"))));

    int haloTextureIndex  = stageDef.geti("haloTextureIndex");
    ClientTexture *haloTexture  = nullptr;
    res::Uri const haloTextureUri(stageDef.gets("haloTexture"), RC_NULL);
    if(!haloTextureUri.isEmpty())
    {
        // Select a system flare by numeric identifier?
        if(haloTextureUri.path().length() == 1 &&
           haloTextureUri.path().toString().first().isNumeric())
        {
            haloTextureIndex = haloTextureUri.path().toString().first().delta('0');
        }
        else
        {
            haloTexture = static_cast<ClientTexture *>(res::Textures::get().tryFindTextureByResourceUri("Flaremaps", haloTextureUri));
        }
    }

    return new AnimationStage(stageDef.geti("tics"), stageDef.getf("variance"),
                              Vec2f(stageDef.geta("origin")), stageDef.getf("elevation"),
                              Vec3f(stageDef.geta("color")), stageDef.getf("radius"),
                              stageDef.getf("haloRadius"),
                              LightRange(Vec2f(stageDef.geta("lightLevels"))),
                              lightmapUp, lightmapDown, lightmapSide,
                              haloTexture, haloTextureIndex);
}

String LightMaterialDecoration::AnimationStage::description() const
{
    return String(_E(l) "Tics: ")      + _E(.) + (tics > 0? Stringf("%i (~%.2f)", tics, variance) : "-1")
                + _E(l) " Origin: "      _E(.) + origin.asText()
                + _E(l) " Elevation: "   _E(.) + String::asText(elevation, 2)
                + _E(l) " LightLevels: " _E(.) + lightLevels.asText()
                + _E(l) "\nColor: "      _E(.) + color.asText()
                + _E(l) " Radius: "      _E(.) + String::asText(radius, 2)
                + _E(l) " HaloRadius: "  _E(.) + String::asText(haloRadius, 2);
}

// ------------------------------------------------------------------------------------

LightMaterialDecoration::LightMaterialDecoration(const Vec2i &patternSkip,
    const Vec2i &patternOffset, bool useInterpolation)
    : Decoration(patternSkip, patternOffset)
    , _useInterpolation(useInterpolation)
{}

LightMaterialDecoration::~LightMaterialDecoration()
{}

LightMaterialDecoration *LightMaterialDecoration::fromDef(const Record &definition)
{
    defn::MaterialDecoration decorDef(definition);

    auto *decor = new LightMaterialDecoration(Vec2i(decorDef.geta("patternSkip")),
                                              Vec2i(decorDef.geta("patternOffset")));
    for(int i = 0; i < decorDef.stageCount(); ++i)
    {
        decor->_stages.append(AnimationStage::fromDef(decorDef.stage(i)));
    }
    return decor;
}

int LightMaterialDecoration::addStage(const AnimationStage &stageToCopy)
{
    _stages.append(new AnimationStage(stageToCopy));
    return _stages.count() - 1;
}

LightMaterialDecoration::AnimationStage &LightMaterialDecoration::stage(int index) const
{
    return static_cast<AnimationStage &>(Decoration::stage(index));
}

String LightMaterialDecoration::describe() const
{
    return "Light decoration";
}

bool LightMaterialDecoration::useInterpolation() const
{
    return _useInterpolation;
}
