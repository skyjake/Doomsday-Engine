#ifndef GLOOM_MAPGEOM_H
#define GLOOM_MAPGEOM_H

#include "mapbuild.h"

namespace gloom {

class ICamera;

class MapGeom
{
public:
    MapGeom();

    void setMap(const Map &map);

    void glInit();
    void glDeinit();
    void render(const ICamera &camera);

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPGEOM_H
