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

#include "gloom/world/map.h"
#include "gloom/render/materiallib.h"

#include <de/glbuffer.h>
#include <de/id.h>
#include <de/hash.h>

namespace gloom {

using namespace de;

/**
 * Vertex format with 3D coordinates, normal vector, one set of texture
 * coordinates, and an RGBA color.
 */
struct MapVertex
{
    Vec3f    pos;
    Vec3f    normal;
    Vec3f    tangent;
    Vec4f    texCoord;
    Vec2f    expander;
    uint32_t material[2];
    uint32_t geoPlane;     // Index0 (x)
    uint32_t texPlane[2];  // Index0 (yz)
    uint32_t texOffset[2]; // Index1 (xy)
    uint32_t flags;

    LIBGUI_DECLARE_VERTEX_FORMAT(10)

    enum Flag {
        WorldSpaceXZToTexCoords = 0x1,
        WorldSpaceYToTexCoord   = 0x2,
        FlipTexCoordY           = 0x4,
        AnchorTopPlane          = 0x8,
        TextureOffset           = 0x10,
        LeftEdge                = 0x20,
        RightEdge               = 0x40,
    };
};

class MapBuild
{
public:
    typedef GLBufferT<MapVertex> Buffer;

    enum BufferType {
        OpaqueGeometry,
        TransparentGeometry,

        BufferCount,
    };

    struct Buffers {
        std::shared_ptr<Buffer> geom[2]; // all opaque/transparent surfaces
        struct Transparency {
            geo::Plane plane;
        };
        List<Transparency> transparencies;
        GLBuffer::DrawRanges transparentRanges; // for sorting
    };

public:
    MapBuild(const Map &map, const MaterialLib &materials);

    Buffers build();

    /// Helper for mapping IDs to elements of a data buffer.
    struct Mapper : public Hash<ID, duint32>
    {
        duint32 insert(ID id)
        {
            const auto found = find(id);
            if (found == end())
            {
                const duint32 mapped = duint32(size());
                Hash<ID, duint32>::insert(id, mapped);
                return mapped;
            }
            return found->second;
        }

        Mapper &operator=(const Mapper &other)
        {
            for (const auto &v : other)
            {
                Hash<ID, duint32>::insert(v.first, v.second);
            }
            return *this;
        }
    };

    const Mapper &planeMapper() const;
    const Mapper &texOffsetMapper() const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPBUILD_H
