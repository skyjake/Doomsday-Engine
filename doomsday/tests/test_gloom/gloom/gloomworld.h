#ifndef GLOOMWORLD_H
#define GLOOMWORLD_H

#include "../gloom/icamera.h"
#include "../gloom/world/user.h"
#include "../gloom/world/world.h"

namespace gloom {

class GloomWorld : public World
{
public:
    GloomWorld();

    void setLocalUser(User *user);

    void glInit();
    void glDeinit();
    void update(de::TimeSpan const &elapsed);
    void render(ICamera const &camera);

    User *     localUser() const;
    POI        initialViewPosition() const;
    QList<POI> pointsOfInterest() const;

    /**
     * Determines the height of the ground at a given world coordinates.
     */
    float groundSurfaceHeight(de::Vector3f const &pos) const;

    float groundSurfaceHeight(de::Vector2f const &worldMapPos) const;

    float ceilingHeight(de::Vector3f const &pos) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOMWORLD_H
