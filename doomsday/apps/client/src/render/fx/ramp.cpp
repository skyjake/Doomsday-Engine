/** @file fx/ramp.cpp  Gamma correction, contrast, and brightness.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/fx/ramp.h"
#include "gl/gl_main.h"
#include "clientapp.h"

#include <de/glprogram.h>
#include <de/gluniform.h>
#include <de/drawable.h>

namespace fx {

using namespace de;

DE_PIMPL(Ramp)
{
    GLUniform uGamma{"uGamma", GLUniform::Float};
    GLUniform uContrast{"uContrast", GLUniform::Float};
    GLUniform uBrightness{"uBrightness", GLUniform::Float};
    GLUniform uTex{"uTex", GLUniform::Sampler2D};
    Drawable ramp;

    Impl(Public *i) : Base(i)
    {}

    void init()
    {
        using VBuf = GLBufferT<Vertex2Tex>;
        auto *buf = new VBuf;
        buf->setVertices(gfx::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(-1, -1, 2, 2),
                                                  Rectanglef(0, 0, 1, 1)),
                         gfx::Static);
        ramp.addBuffer(buf);

        ClientApp::shaders().build(ramp.program(), "fx.ramp")
            << uGamma << uContrast << uBrightness << uTex;
    }

    void deinit()
    {
        ramp.clear();
    }
};

Ramp::Ramp(int console)
    : ConsoleEffect(console)
    , d(new Impl(this))
{}

void Ramp::glInit()
{
    ConsoleEffect::glInit();
    d->init();
}

void Ramp::glDeinit()
{
    d->deinit();
    ConsoleEffect::glDeinit();
}

void Ramp::draw()
{
    GLFramebuffer &target = GLState::current().target();
    GLTexture *colorTex = target.attachedTexture(GLFramebuffer::Color0);

    // Must have access to the color texture containing the frame buffer contents.
    if (!colorTex) return;

    if (!fequal(vid_gamma, 1.0f) || !fequal(vid_contrast, 1.0f) || !fequal(vid_bright, 0.0f))
    {
        d->uTex        = *colorTex;
        d->uGamma      = de::max(0.1f, vid_gamma);
        d->uContrast   = de::max(0.1f, vid_contrast);
        d->uBrightness = de::clamp(-0.8f, vid_bright, 0.8f);

        if (GLInfo::extensions().NV_texture_barrier)
        {
            gl::glTextureBarrierNV();
        }

        d->ramp.draw();
    }
}

} // namespace fx
