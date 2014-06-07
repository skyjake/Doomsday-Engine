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

        Primitive &setTexOffset(int unit, de::Vector2f const &newOffset);
        Primitive &setTexScale (int unit, de::Vector2f const &newScale);

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
    Shard(GeomGroup geomGroup          = UnlitGeom,
          blendmode_t blendmode        = BM_NORMAL,
          GLuint modTex                = 0,
          de::Vector3f const &modColor = de::Vector3f(),
          bool hasDynlights            = false);

    DrawListSpec const &drawListSpec() const;

    Shard &setTextureUnit(texunitid_t unit, de::GLTextureUnit const &gltu);

    Primitive &newPrimitive(de::gl::Primitive type, WorldVBuf::Index vertCount,
        WorldVBuf &vbuf, WorldVBuf::Index indicesOffset = 0);
};

typedef Shard::Primitive ShardPrimitive;

#endif // DENG_CLIENT_RENDER_SHARD_H
