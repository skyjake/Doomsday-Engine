#ifndef GLOOM_MAPGEOM_H
#define GLOOM_MAPGEOM_H

#include <de/AtlasTexture>
#include "../world/map.h"

namespace gloom {

class ICamera;

class MapGeom
{
public:
    MapGeom();

    void setAtlas(de::AtlasTexture &atlas);
    void setMap(const Map &map);

    void glInit();
    void glDeinit();
    void render(const ICamera &camera);

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPGEOM_H
