#include "world.h"
#include "../../src/gloomapp.h"
#include <de/Drawable>
#include <de/GLBuffer>
#include <de/BaseGuiApp>
#include <de/Matrix>
#include <de/VRConfig>

using namespace de;

World::World()
{}

World::~World()
{}

void World::glInit()
{}

void World::glDeinit()
{}

void World::update(TimeSpan const &)
{}

void World::render(ICamera const &)
{}

World::POI World::initialViewPosition() const
{
    return Vector3f();
}

QList<World::POI> World::pointsOfInterest() const
{
    return QList<POI>();
}

float World::groundSurfaceHeight(Vector3f const &) const
{
    return 0;
}

float World::ceilingHeight(Vector3f const &) const
{
    return -1000;
}
