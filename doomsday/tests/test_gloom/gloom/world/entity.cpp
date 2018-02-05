#include "entity.h"

using namespace de;

DENG2_PIMPL_NOREF(Entity)
{
    Type type;
    Vector3f pos;
    float angle;
    Vector3f scale;

    Instance()
        : type(Tree1)
        , angle(0)
    {}
};

Entity::Entity() : d(new Instance)
{}

void Entity::setType(Entity::Type t)
{
    d->type = t;
}

void Entity::setPosition(Vector3f const &pos)
{
    d->pos = pos;
}

void Entity::setScale(float scale)
{
    d->scale = Vector3f(scale, scale, scale);
}

void Entity::setScale(Vector3f const &scale)
{
    d->scale = scale;
}

void Entity::setAngle(float yawDegrees)
{
    d->angle = yawDegrees;
}

Entity::Type Entity::type() const
{
    return d->type;
}

Vector3f Entity::position() const
{
    return d->pos;
}

Vector3f Entity::scale() const
{
    return d->scale;
}

float Entity::angle() const
{
    return d->angle;
}
