#ifndef WORLD_H
#define WORLD_H

#include <de/AtlasTexture>
#include <de/Matrix>
#include "../icamera.h"

#include <QList>

namespace gloom {

class User;

class World
{
public:
    World();
    virtual ~World();

    virtual void setLocalUser(User *) = 0;

    virtual void glInit();
    virtual void glDeinit();
    virtual void update(de::TimeSpan const &elapsed);
    virtual void render(ICamera const &camera);

    struct POI {
        de::Vector3f position;
        float        yaw;

        POI(de::Vector3f const &pos, float yawAngle = 0)
            : position(pos)
            , yaw(yawAngle)
        {}
    };

    virtual User *     localUser() const = 0;
    virtual POI        initialViewPosition() const;
    virtual QList<POI> pointsOfInterest() const;

    virtual float groundSurfaceHeight(de::Vector3f const &pos) const;
    virtual float ceilingHeight(de::Vector3f const &pos) const;

public:
    DENG2_DEFINE_AUDIENCE(Ready, void worldReady(World &))
};

} // namespace gloom

#endif // WORLD_H
