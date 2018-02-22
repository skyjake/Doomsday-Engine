/** @file light.h
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

#ifndef GLOOM_RENDER_LIGHT_H
#define GLOOM_RENDER_LIGHT_H

#include <de/GLFramebuffer>
#include <de/GLTexture>
#include <de/Matrix>
#include <de/Vector>

#include "gloom/world/entity.h"

namespace gloom {

/**
 * Light source.
 */
class Light
{
public:
    enum Type { Omni, Linear, Spot };

    Light();

    void setEntity(const Entity *entity);
    void setType(Type type);

    const Entity *entity() const;

    Type      type() const;
    de::Vec3f origin() const; // from entity
    de::Vec3f direction() const;
    de::Vec3f intensity() const;
    float     fovY() const;
    float     aspectRatio() const;

    de::GLTexture &    shadowMap();
    de::GLFramebuffer &framebuf();
    de::Mat4f          lightMatrix() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_RENDER_LIGHT_H
