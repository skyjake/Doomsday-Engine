/** @file shard.h  3D map geometry fragment.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_RENDER_SHARD_H
#define DENG_CLIENT_RENDER_SHARD_H

#include <QList>
#include <de/GLBuffer>
#include <de/Vector>
#include "rendersystem.h" // WorldVBuf
#include "DrawList"
#include "gl/gltextureunit.h"

/**
 * 3D map geometry fragment.
 *
 * Shards are produced by SectorClusters when the map geometry is split into
 * drawable geometry fragments.
 */
class Shard
{
public:
    /// Logical shard types (primarily for identification/grouping):
    enum Type {
        General,  ///< Regular geometry (walls, flats).
        Light,    ///< Dynamic lights (mobjs, plane glows).
        Shadow,   ///< Dynamic shadows (including FakeRadio).
        Shine,    ///< Surface shine geometry.
        SkyMask   ///< Sky-masked geometry.
    };

public:
    Type type;
    DrawListSpec listSpec;
    blendmode_t blendmode;
    GLuint modTex;
    de::Vector3f modColor;
    bool hasDynlights;
    typedef WorldVBuf::Indices Indices;
    Indices indices;
    struct Primitive
    {
        de::gl::Primitive type;
        WorldVBuf const *vbuffer;
        WorldVBuf::Index vertCount;
        WorldVBuf::Index *indices;
        struct Texunit {
            bool useOffset;
            bool useScale;
            de::Vector2f offset;
            de::Vector2f scale;
        } texunits[2];

        de::Vector2f texOffset(int unit) const;
        de::Vector2f texScale (int unit) const;

        Primitive &setTexOffset(int unit, de::Vector2f const &newOffset);
        Primitive &setTexScale (int unit, de::Vector2f const &newScale);

        inline de::Vector2f tex0Offset() const { return texOffset(0); }
        inline de::Vector2f tex0Scale () const { return texScale (0); }
        inline de::Vector2f tex1Offset() const { return texOffset(1); }
        inline de::Vector2f tex1Scale () const { return texScale (1); }

        inline Primitive &setTex0Offset(de::Vector2f const &newOffset) { return setTexOffset(0, newOffset); }
        inline Primitive &setTex0Scale (de::Vector2f const &newScale)  { return setTexScale (0, newScale);  }
        inline Primitive &setTex1Offset(de::Vector2f const &newOffset) { return setTexOffset(1, newOffset); }
        inline Primitive &setTex1Scale (de::Vector2f const &newScale)  { return setTexScale (1, newScale);  }
    };
    typedef QList<Primitive> Primitives;
    Primitives primitives;

public:
    /**
     * Construct a new Shard of 3D map geometry.
     */
    Shard(Type type                    = General,
          blendmode_t blendmode        = BM_NORMAL,
          GLuint modTex                = 0,
          de::Vector3f const &modColor = de::Vector3f(),
          bool hasDynlights            = false);

    /**
     * Configure all GL texture units in order, according to the given @a gltumap.
     * @note If no entry exists for a given unit then it will remain @em unchanged.
     *
     * @see setTextureUnit()
     */
    Shard &setAllTextureUnits(de::GLTextureUnit const *gltumap[NUM_TEXTURE_UNITS]);

    /**
     * Configure a single GL texture @a unit according to @a gltu.
     *
     * @param unit  Unique identifier of the texture unit to change.
     * @param gltu  GL texture unit configuration to apply.
     *
     * @see setAllTextureUnits()
     */
    Shard &setTextureUnit(texunitid_t unit, de::GLTextureUnit const &gltu);

    Primitive &newPrimitive(de::gl::Primitive type, WorldVBuf::Index vertCount,
        WorldVBuf &vbuf, WorldVBuf::Index indicesOffset = 0);
};

typedef Shard::Primitive ShardPrimitive;

#endif // DENG_CLIENT_RENDER_SHARD_H
