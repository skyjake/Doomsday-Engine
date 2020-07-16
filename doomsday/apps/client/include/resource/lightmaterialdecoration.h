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
#  error "LightMaterialDecoration only exists in the Client"
#endif

#include <de/string.h>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/dedtypes.h>
#include "misc/r_util.h"  // LightRange
#include "resource/clienttexture.h"
#include "resource/clientmaterial.h"

/**
 * @ingroup resource
 */
class LightMaterialDecoration : public ClientMaterial::Decoration
{
public:
    /**
     * Stages describe light change animations.
     */
    class AnimationStage : public Stage
    {
    public:
        de::Vec2f origin;     ///< Position in material space.
        float elevation;         ///< Distance from the surface.
        de::Vec3f color;      ///< Light color.
        float radius;            ///< Dynamic light radius (-1 = no light).
        float haloRadius;        ///< Halo radius (zero = no halo).
        LightRange lightLevels;  ///< Fade by sector lightlevel.

        ClientTexture *tex;
        ClientTexture *floorTex;
        ClientTexture *ceilTex;

        ClientTexture *flareTex;
        int sysFlareIdx;         ///< @todo Remove me

    public:
        AnimationStage(int tics, float variance, const de::Vec2f &origin, float elevation,
                       const de::Vec3f &color, float radius, float haloRadius,
                       const LightRange &lightLevels, ClientTexture *ceilingTexture,
                       ClientTexture *floorTexture, ClientTexture *texture,
                       ClientTexture *flareTexture, int sysFlareIdx = -1);
        AnimationStage(const AnimationStage &other);
        virtual ~AnimationStage() {}

        /**
         * Construct a new AnimationStage from the given @a stageDef.
         */
        static AnimationStage *fromDef(const de::Record &stageDef);

        de::String description() const;
    };

public:
    LightMaterialDecoration(const de::Vec2i &patternSkip   = de::Vec2i(),
                            const de::Vec2i &patternOffset = de::Vec2i(),
                            bool useInterpolation             = true);
    virtual ~LightMaterialDecoration();

    /**
     * Construct a new material decoration from the specified definition.
     */
    static LightMaterialDecoration *fromDef(const de::Record &decorationDef);

    de::String describe() const;

    /**
     * Add a new animation stage to the material light decoration.
     *
     * @param stage  New stage to add (a copy is made).
     *
     * @return  Index of the newly added stage (0 based).
     */
    int addStage(const AnimationStage &stage);

    /**
     * Lookup an AnimationStage by it's unique @a index.
     *
     * @param index  Index of the AnimationStage to lookup. Will be cycled into valid range.
     */
    AnimationStage &stage(int index) const;

    /**
     * Returns @c true if interpolation should be used with this decoration.
     */
    bool useInterpolation() const;

private:
    bool _useInterpolation;
};

#endif  // CLIENT_RESOURCE_MATERIALLIGHTDECORATION_H
