#include "mapgeom.h"

#include <de/Drawable>
#include <de/GLUniform>

using namespace de;

namespace gloom {

DENG2_PIMPL(MapGeom)
{
    const Map *map = nullptr;

    Drawable  mapDrawable;
    GLUniform uMvpMatrix{"uMvpMatrix", GLUniform::Mat4};

    Impl(Public *i) : Base(i)
    {}

    void clear()
    {
        mapDrawable.clear();
    }
};

MapGeom::MapGeom()
    : d(new Impl(this))
{}

void MapGeom::setMap(const Map &map)
{
    d->clear();
    d->map = &map;
}

void MapGeom::glInit()
{

}

void MapGeom::glDeinit()
{}

void MapGeom::render(const ICamera &camera)
{}

} // namespace gloom
