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
    Vec3d origin    { -100, 100, 30 };
    Vec3f dir       { 100, -100, -30 };
    Vec3f intensity { 10, 10, 10 };
    GLTexture shadowMap;
    GLFramebuffer framebuf;

    Impl(Public *i) : Base(i)
    {
        shadowMap.setAutoGenMips(false);
        shadowMap.setFilter(gl::Linear, gl::Linear, gl::MipNone);
        shadowMap.setUndefinedContent(
            GLTexture::Size(1024, 1024),
            GLPixelFormat(GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT));

        framebuf.configure(GLFramebuffer::Depth, shadowMap);
    }
};

Light::Light()
    : d(new Impl(this))
{}

GLFramebuffer &Light::framebuf()
{
    return d->framebuf;
}

Mat4f Light::lightMatrix() const
{
    return Mat4f::ortho(-50, 50, -50, 50, 1, 200) *
           Mat4f::lookAt(d->origin + d->dir, d->origin, Vec3f(0, 1, 0));
}

} // namespace gloom
