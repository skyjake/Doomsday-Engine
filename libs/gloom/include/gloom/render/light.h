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

#include <de/glframebuffer.h>
#include <de/gltexture.h>
#include <de/matrix.h>
#include <de/vector.h>

#include "gloom/world/entity.h"
#include "gloom/render/icamera.h"

namespace gloom {

using namespace de;

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
    void setOrigin(const Vec3d &);
    void setDirection(const Vec3f &);
    void setType(Type type);
    void setIntensity(const Vec3f &intensity);
    void setCastShadows(bool castShadows);

    const Entity *entity() const;

    bool  castShadows() const;
    Type  type() const;
    Vec3d origin() const; // from entity
    Vec3f direction() const;
    Vec3f intensity() const;
    float fovY() const;
    float aspectRatio() const;
    float falloffDistance() const;

    Mat4f lightMatrix() const;
    Mat4f lightMatrix(gfx::CubeFace) const;

    Vec3f cameraPosition() const;
    Vec3f cameraFront() const;
    Vec3f cameraUp() const;
    Mat4f cameraProjection() const;
    Mat4f cameraModelView() const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_RENDER_LIGHT_H
