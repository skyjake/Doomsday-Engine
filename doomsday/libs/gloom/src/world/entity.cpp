/** @file entity.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "gloom/world/entity.h"

using namespace de;

namespace gloom {

DE_PIMPL_NOREF(Entity)
{
    ID    id;
    Type  type{Tree1};
    Vec3d pos;
    float angle{0};
    Vec3f scale{1, 1, 1};

    Impl()
        : type(Tree1)
        , angle(0)
    {
        angle = rand() % 360;
    }
};

Entity::Entity() : d(new Impl)
{}

void Entity::setId(ID id)
{
    d->id = id;
}

void Entity::setType(Type t)
{
    d->type = t;
}

void Entity::setPosition(const Vec3d &pos)
{
    d->pos = pos;
}

void Entity::setScale(float scale)
{
    d->scale = Vec3f(scale, scale, scale);
}

void Entity::setScale(const Vec3f &scale)
{
    d->scale = scale;
}

void Entity::setAngle(float yawDegrees)
{
    d->angle = yawDegrees;
}

ID Entity::id() const
{
    return d->id;
}

Entity::Type Entity::type() const
{
    return d->type;
}

Vec3d Entity::position() const
{
    return d->pos;
}

Vec3f Entity::scale() const
{
    return d->scale;
}

float Entity::angle() const
{
    return d->angle;
}

} // namespace gloom
