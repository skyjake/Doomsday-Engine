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

#include <de/GLBuffer>
#include <de/Id>

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
    uint32_t material[2];
    uint32_t geoPlane;
    uint32_t texPlane[2];  // Index0: vec3
    uint32_t texOffset[2]; // Index1: vec2
    uint32_t flags;

    LIBGUI_DECLARE_VERTEX_FORMAT(9)

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
        QVector<Transparency> transparencies;
        GLBuffer::DrawRanges transparentRanges; // for sorting
    };

public:
    MapBuild(const Map &map, const MaterialLib &materials);

    Buffers build();

    /// Helper for mapping IDs to elements of a data buffer.
    struct Mapper : public QHash<ID, uint32_t>
    {
        uint32_t insert(ID id)
        {
            const auto found = constFind(id);
            if (found == constEnd())
            {
                const uint32_t mapped = size();
                QHash<ID, uint32_t>::insert(id, mapped);
                return mapped;
            }
            return found.value();
        }
    };

    const Mapper &planeMapper() const;
    const Mapper &texOffsetMapper() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPBUILD_H
