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
    de::Vector2f texCoord;
    uint32_t texture;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

class MapBuild
{
public:
    typedef de::GLBufferT<MapVertex> Buffer;

public:
    typedef QHash<de::String, uint32_t> TextureIds;

    MapBuild(const Map &map, const TextureIds &textures);

    Buffer *build();

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPBUILD_H
