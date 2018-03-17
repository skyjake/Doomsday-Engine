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
#include "gloom/render/icamera.h"

namespace gloom {

class Shadow;

/**
 * Light source.
 */
class Light : public ICamera
{
public:
    enum Type { Omni, Directional, Spot };

    Light();

    void setEntity(const Entity *entity);
    void setType(Type type);
    void setIntensity(const de::Vec3f &intensity);
    void setCastShadows(bool castShadows);

    const Entity *entity() const;

    bool      castShadows() const;
    Type      type() const;
    de::Vec3f origin() const; // from entity
    de::Vec3f direction() const;
    de::Vec3f intensity() const;
    float     fovY() const;
    float     aspectRatio() const;
    float     falloffDistance() const;

    de::Mat4f          lightMatrix() const;
    de::Mat4f          lightMatrix(de::gl::CubeFace) const;

    de::Vec3f cameraPosition() const;
    de::Vec3f cameraFront() const;
    de::Vec3f cameraUp() const;
    de::Mat4f cameraProjection() const;
    de::Mat4f cameraModelView() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_RENDER_LIGHT_H
