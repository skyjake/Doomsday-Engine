/** @file mapbuild.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOM_MAPBUILD_H
#define GLOOM_MAPBUILD_H

#include "../world/map.h"

#include <de/GLBuffer>
#include <de/Id>

namespace gloom {

/**
 * Vertex format with 3D coordinates, normal vector, one set of texture
 * coordinates, and an RGBA color.
 */
struct MapVertex
{
    de::Vector3f pos;
    de::Vector3f normal;
    de::Vector3f texCoord;
    uint32_t texture[2];
    uint32_t geoPlane;
    uint32_t texPlane[2];
    uint32_t texOffset;
    uint32_t flags;

    LIBGUI_DECLARE_VERTEX_FORMAT(10)

    enum Flag {
        WorldSpaceXZToTexCoords = 0x1,
        WorldSpaceYToTexCoord   = 0x2,
        FlipTexCoordY           = 0x4,
        AnchorTopPlane          = 0x8,
    };
};

class MapBuild
{
public:
    typedef de::GLBufferT<MapVertex> Buffer;

public:
    typedef QHash<de::String, uint32_t> TextureIds;
    typedef QHash<ID, uint32_t>         PlaneMapper;

    MapBuild(const Map &map, const TextureIds &textures);
    Buffer *build();

    const PlaneMapper &planeMapper() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPBUILD_H
