#ifndef ENTITYMAP_H
#define ENTITYMAP_H

#include <functional>
#include <de/Vector>
#include "entity.h"

class EntityMap
{
public:
    EntityMap();

    void setSize(de::Vector2f const &size);

    /**
     * @brief insert
     * @param entity  Ownership taken.
     */
    void insert(Entity *entity);

    QList<Entity const *> listRegionBackToFront(de::Vector3f const &pos, float radius) const;

    typedef std::function<void (Entity const &)> Callback;

    void iterateRegion(de::Vector3f const &pos, float radius, Callback const &callback) const;

private:
    DENG2_PRIVATE(d)
};

#endif // ENTITYMAP_H
