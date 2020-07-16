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

#ifndef GLOOM_ENTITY_H
#define GLOOM_ENTITY_H

#include <de/vector.h>
#include "gloom/identity.h"

namespace gloom {

class LIBGLOOM_PUBLIC Entity
{
public:
    enum Type {
        None = 0,

        // Special entity types:
        Light     = 1,
        Spotlight = 2,

        // World entities:
        Tree1 = 1000,
        Tree2,
        Tree3,
        TestSphere,
        Buggy,
    };

public:
    Entity();

    void setId(ID id);
    void setType(Type t);
    void setPosition(const de::Vec3d &pos);
    void setScale(float scale);
    void setScale(const de::Vec3f &scale);
    void setAngle(float yawDegrees);

    ID        id() const;
    Type      type() const;
    de::Vec3d position() const;
    de::Vec3f scale() const;
    float     angle() const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_ENTITY_H
