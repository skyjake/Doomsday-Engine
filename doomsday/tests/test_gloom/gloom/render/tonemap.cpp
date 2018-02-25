/** @file tonemap.cpp
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

#include "gloom/render/tonemap.h"
#include "gloom/render/screenquad.h"

using namespace de;

namespace gloom {

DENG2_PIMPL(Tonemap)
{
    ScreenQuad quad;
    GLUniform  uFramebuf{"uFramebuf", GLUniform::Sampler2D};
    GLUniform  uExposure{"uExposure", GLUniform::Float};

    Impl(Public *i) : Base(i)
    {
        uExposure = 1.0f;
    }
};

Tonemap::Tonemap()
    : d(new Impl(this))
{}

void Tonemap::glInit(Context &context)
{
    Render::glInit(context);
    d->quad.glInit(context);
    context.shaders->build(d->quad.program(), "gloom.tonemap") << d->uFramebuf << d->uExposure;
}

void Tonemap::glDeinit()
{
    d->quad.glDeinit();
    Render::glDeinit();
}

void Tonemap::render()
{
    d->uFramebuf = context().framebuf->attachedTexture(GLFramebuffer::Color0);
    d->quad.render();
}

} // namespace gloom
