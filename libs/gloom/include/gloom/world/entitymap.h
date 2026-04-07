/** @file entitymap.h
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

#ifndef GLOOM_ENTITYMAP_H
#define GLOOM_ENTITYMAP_H

#include <de/rectangle.h>
#include <de/vector.h>

#include "gloom/world/entity.h"

namespace gloom {

class EntityMap
{
public:
    using EntityList = de::List<const Entity *>;

    EntityMap();

    void clear();
    void setBounds(const de::Rectangled &bounds);
    void insert(const Entity &entity);

    EntityList listRegionBackToFront(const de::Vec3f &pos, float radius) const;

    void iterateRegion(const de::Vec3f &pos,
                       float            radius,
                       const std::function<void(const Entity &)>& callback) const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_ENTITYMAP_H
