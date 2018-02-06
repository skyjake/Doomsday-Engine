#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <de/Time>

namespace gloom {

class World;

class Environment
{
public:
    Environment();

    void setWorld(World *world);

    void enable(bool enabled = true);
    void disable() { enable(false); }
    void update(de::TimeSpan const &elapsed);

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // ENVIRONMENT_H
