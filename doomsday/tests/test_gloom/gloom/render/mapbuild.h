#ifndef GLOOM_MAPBUILD_H
#define GLOOM_MAPBUILD_H

#include "../world/map.h"

#include <de/GLBuffer>

namespace gloom {

class MapBuild
{
public:
    typedef de::GLBufferT<de::Vertex3NormalTexRgba> Buffer;

public:
    MapBuild(const Map &map);

    Buffer *build();

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPBUILD_H
