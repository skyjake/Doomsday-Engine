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
            shadow.reset(new Shadow);

            shadow->map.setAutoGenMips(false);
            shadow->map.setComparisonMode(gl::CompareRefToTexture, gl::LessOrEqual);
            shadow->map.setFilter(gl::Linear, gl::Linear, gl::MipNone);
            shadow->map.setWrap(gl::ClampToBorder, gl::ClampToBorder);
            shadow->map.setBorderColor(Vec4f(1, 1, 1, 1));
            shadow->map.setUndefinedContent(
                GLTexture::Size(2048, 2048),
                GLPixelFormat(GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_FLOAT));

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
}

void Light::setType(Type type)
{
    d->type = type;
}

void Light::setCastShadows(bool castShadows)
{
    d->setCastShadows(castShadows);
}

bool Light::castShadows() const
{
    return bool(d->shadow);
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

GLTexture &gloom::Light::shadowMap()
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

} // namespace gloom
