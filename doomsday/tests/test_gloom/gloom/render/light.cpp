/** @file light.cpp
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

#include "gloom/render/light.h"

#include <de/GLFramebuffer>

using namespace de;

namespace gloom {

DENG2_PIMPL(Light)
{
    const Entity *entity = nullptr;
    Type          type   = Omni;
    Vec3d         origin;
    Vec3f         dir{-.41f, -.51f, -.75f};
    Vec3f         intensity{5, 5, 5};

    struct Shadow {
        GLTexture map;
        GLFramebuffer framebuf;
    };
    std::unique_ptr<Shadow> shadow;

    Impl(Public *i) : Base(i)
    {
        origin = -dir * 50;
    }

    void setCastShadows(bool yes)
    {
        if (yes && !shadow)
        {
            // TODO: Shadow maps should be owned by LightRender and assigned dynamically
            // to visible lights as needed.

            shadow.reset(new Shadow);
            shadow->map.setAutoGenMips(false);
            shadow->map.setFilter(gl::Linear, gl::Linear, gl::MipNone);
            shadow->map.setComparisonMode(gl::CompareRefToTexture, gl::LessOrEqual);

            if (type == Directional)
            {
                shadow->map.setWrap(gl::ClampToBorder, gl::ClampToBorder);
                shadow->map.setBorderColor(Vec4f(1, 1, 1, 1));
                shadow->map.setUndefinedContent(
                    GLTexture::Size(2048, 2048),
                    GLPixelFormat(GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_FLOAT));
            }
            else if (type == Omni)
            {
                shadow->map.setWrap(gl::ClampToEdge, gl::ClampToEdge);
                for (int i = 0; i < 6; ++i)
                {
                    shadow->map.setUndefinedContent(gl::CubeFace(i), GLTexture::Size(512, 512),
                                                    GLPixelFormat(GL_DEPTH_COMPONENT16,
                                                                  GL_DEPTH_COMPONENT, GL_FLOAT));
                }
            }

            shadow->framebuf.configure(GLFramebuffer::Depth, shadow->map);
        }
        else if (!yes && shadow)
        {
            shadow.reset();
        }
    }
};

Light::Light()
    : d(new Impl(this))
{}

void Light::setEntity(const Entity *entity)
{
    d->entity = entity;
    d->origin = entity->position();
}

void Light::setType(Type type)
{
    d->type = type;
}

void Light::setCastShadows(bool castShadows)
{
    d->setCastShadows(castShadows);
}

const Entity *Light::entity() const
{
    return d->entity;
}

bool Light::castShadows() const
{
    return bool(d->shadow);
}

Light::Type Light::type() const
{
    return d->type;
}

Vec3f Light::origin() const
{
    if (d->entity)
    {
        const auto p = d->entity->position();
        return p + Vec3f(0, 2, 0); // <---TESTING---
    }
    return d->origin;
}

Vec3f Light::direction() const
{
    if (d->type == Omni) return Vec3f(); // emits in all directions
    return d->dir.normalize();
}

Vec3f Light::intensity() const
{
    return d->intensity;
}

float Light::falloffDistance() const
{
    float maxInt = d->intensity.max();
    return maxInt;
}

GLTexture &Light::shadowMap() const
{
    DENG2_ASSERT(d->shadow);
    return d->shadow->map;
}

GLFramebuffer &Light::framebuf()
{
    DENG2_ASSERT(d->shadow);
    return d->shadow->framebuf;
}

Mat4f Light::lightMatrix() const
{
    return Mat4f::ortho(-25, 20, -10, 10, 15, 80) *
           Mat4f::lookAt(d->origin + d->dir, d->origin, Vec3f(0, 1, 0));
}

Vec3f Light::cameraPosition() const
{
    return d->origin;
}

Vec3f Light::cameraFront() const
{
    return Vec3f(0, 0, 1);
}

Vec3f Light::cameraUp() const
{
    return Vec3f(0, 1, 0);
}

Mat4f Light::cameraProjection() const
{
    return Mat4f::perspective(90.0f, 1.0f, .1f, falloffDistance());
}

Mat4f Light::cameraModelView() const
{
    return Mat4f::lookAt(cameraPosition() + cameraFront(), cameraPosition(), cameraUp());
}

Mat4f Light::lightMatrix(gl::CubeFace face) const
{
    const Mat4f proj = Mat4f::perspective(90.0f, 1.0f, .05f, falloffDistance());
    const auto pos = origin();

    switch (face)
    {
    case gl::PositiveX:
        return proj * Mat4f::lookAt(pos + Vec3f(-1, 0, 0), pos, Vec3f(0, 1, 0));

    case gl::NegativeX:
        return proj * Mat4f::lookAt(pos + Vec3f(1, 0, 0), pos, Vec3f(0, 1, 0));

    case gl::PositiveY:
        return proj * Mat4f::lookAt(pos + Vec3f(0, -1, 0), pos, Vec3f(0, 0, -1));

    case gl::NegativeY:
        return proj * Mat4f::lookAt(pos + Vec3f(0, 1, 0), pos, Vec3f(0, 0, 1));

    case gl::PositiveZ:
        return proj * Mat4f::lookAt(pos + Vec3f(0, 0, -1),  pos, Vec3f(0, 1, 0));

    case gl::NegativeZ:
        return proj * Mat4f::lookAt(pos + Vec3f(0, 0, 1), pos, Vec3f(0, 1, 0));
    }
}

} // namespace gloom
