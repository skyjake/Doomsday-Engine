/** @file materiallightdecoration.h  Logical material, light decoration.
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

#ifndef CLIENT_RESOURCE_MATERIALLIGHTDECORATION_H
#define CLIENT_RESOURCE_MATERIALLIGHTDECORATION_H

#ifndef __CLIENT__
#  error "MaterialLightDecoration only exists in the Client"
#endif

#include <de/String>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/dedtypes.h>
#include "Material"
#include "r_util.h"  // LightRange

/**
 * @ingroup resource
 */
class MaterialLightDecoration : public Material::Decoration
{
public:
    /**
     * Stages describe light change animations.
     */
    class AnimationStage : public Stage
    {
    public:
        de::Vector2f origin;     ///< Position in material space.
        float elevation;         ///< Distance from the surface.
        de::Vector3f color;      ///< Light color.
        float radius;            ///< Dynamic light radius (-1 = no light).
        float haloRadius;        ///< Halo radius (zero = no halo).
        LightRange lightLevels;  ///< Fade by sector lightlevel.

        de::Texture *tex;
        de::Texture *floorTex;
        de::Texture *ceilTex;

        de::Texture *flareTex;
        int sysFlareIdx;         ///< @todo Remove me

    public:
        AnimationStage(int tics, float variance, de::Vector2f const &origin, float elevation,
                       de::Vector3f const &color, float radius, float haloRadius,
                       LightRange const &lightLevels, de::Texture *ceilingTexture,
                       de::Texture *floorTexture, de::Texture *texture,
                       de::Texture *flareTexture, int sysFlareIdx = -1);
        AnimationStage(AnimationStage const &other);
        virtual ~AnimationStage() {}

        /**
         * Construct a new AnimationStage from the given @a stageDef.
         */
        static AnimationStage *fromDef(de::Record const &stageDef);

        de::String description() const;
    };

public:
    MaterialLightDecoration(de::Vector2i const &patternSkip   = de::Vector2i(),
                            de::Vector2i const &patternOffset = de::Vector2i());
    virtual ~MaterialLightDecoration();

    /**
     * Construct a new material decoration from the specified definition.
     */
    static MaterialLightDecoration *fromDef(de::Record const &decorationDef);

    de::String describe() const;

    /**
     * Add a new animation stage to the material light decoration.
     *
     * @param stage  New stage to add (a copy is made).
     *
     * @return  Index of the newly added stage (0 based).
     */
    int addStage(AnimationStage const &stage);

    /**
     * Lookup an AnimationStage by it's unique @a index.
     *
     * @param index  Index of the AnimationStage to lookup. Will be cycled into valid range.
     */
    AnimationStage &stage(int index) const;
};

#endif  // CLIENT_RESOURCE_MATERIALLIGHTDECORATION_H
