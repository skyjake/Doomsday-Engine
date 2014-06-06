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

/**
 * 3D map geometry fragment.
 *
 * Shards are produced by SectorClusters when the map geometry is split into
 * drawable geometry fragments.
 */
class Shard
{
public:
    DrawList::Spec listSpec;
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
        de::Vector2f texScale;
        de::Vector2f texOffset;
        de::Vector2f detailTexScale;
        de::Vector2f detailTexOffset;
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

};

#endif // DENG_CLIENT_RENDER_SHARD_H
