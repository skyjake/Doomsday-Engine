/** @file bloom.cpp
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

#include "gloom/render/bloom.h"
#include "gloom/render/screenquad.h"
#include "gloom/render/tonemap.h"
#include "gloom/render/defs.h"

#include <de/gluniform.h>

using namespace de;

namespace gloom {

DE_PIMPL(Bloom)
{
    struct WorkBuf {
        GLFramebuffer framebuf;
        GLTexture     texture;
        
        Rectangleui viewport() const { return Rectangleui::fromSize(framebuf.size()); }
    };

    ScreenQuad quad;
    WorkBuf    workBufs[2];
    GLUniform  uInputTex     {"uInputTex",      GLUniform::Sampler2D};
    GLUniform  uInputLevel   {"uInputLevel",    GLUniform::Int};
    GLUniform  uBloomMode    {"uBloomMode",     GLUniform::Int};
    GLUniform  uMinValue     {"uMinValue",      GLUniform::Float};
    GLUniform  uBloomFramebuf{"uBloomFramebuf", GLUniform::Sampler2D}; // output

    Impl(Public *i) : Base(i)
    {}

    void init(Context &context)
    {
        quad.glInit(context);
        context.shaders->build(quad.program(), "gloom.bloom.blur_partial")
                << uInputTex
                << uInputLevel
                << uBloomMode
                << uMinValue
                << context.tonemap->uBrightnessSamples();
        for (auto &buf : workBufs)
        {
            buf.texture.setAutoGenMips(false);
            buf.texture.setFilter(gfx::Linear, gfx::Linear, gfx::MipNone);
            buf.texture.setWrap(gfx::ClampToEdge, gfx::ClampToEdge);
        }
    }

    void deinit()
    {
        for (auto &buf : workBufs)
        {
            buf.texture.clear();
            buf.framebuf.configure();
        }
        quad.glDeinit();
    }

    void resize(const Vec2ui &size)
    {
        for (auto &buf : workBufs)
        {
            if (buf.framebuf.size() != size)
            {
                buf.texture.setUndefinedImage(size, Image::RGB_16f);
                buf.framebuf.configure(GLFramebuffer::Color0, buf.texture);
                //buf.framebuf.clear(GLFramebuffer::Color0);
            }
        }
    }
};

Bloom::Bloom()
    : d(new Impl(this))
{}

void Bloom::glInit(Context &context)
{
    Render::glInit(context);
    d->init(context);
}

void Bloom::glDeinit()
{
    d->deinit();
    Render::glDeinit();
}

void Bloom::render()
{
    d->resize(context().framebuf->size() / 2);

    // Input is the finished frame.
    d->uInputTex   = context().framebuf->attachedTexture(GLFramebuffer::Color0);
    d->uInputLevel = 1;
    d->uMinValue   = 3.0f;

    for (int repeat = 0; repeat < 4; ++repeat)
    {
        if (repeat > 0)
        {
            d->uInputTex   = d->workBufs[1].texture;
            d->uInputLevel = 0;
        }
        d->uBloomMode = BloomHorizontal;

        // First extract the bright pixels and apply blurring along one dimension.
        GLState::push()
                .setTarget   (d->workBufs[0].framebuf)
                .setViewport (d->workBufs[0].viewport())
                .setBlend    (false)
                .setDepthTest(false);

        d->quad.state() = GLState::current();
        d->quad.render();

        GLState::pop();

        // Then apply blurring on the other dimension to finish the blur.
        d->uInputTex   = d->workBufs[0].texture;
        d->uInputLevel = 0;
        d->uBloomMode  = BloomVertical;
        d->uMinValue   = 0.0f;

        GLState::push()
                .setTarget(d->workBufs[1].framebuf)
                .setViewport(d->workBufs[1].viewport())
                .setBlend(false)
                .setDepthTest(false);

        d->quad.state() = GLState::current();
        d->quad.render();

        GLState::pop();
    }

    d->uBloomFramebuf = d->workBufs[1].texture;
}

GLUniform &Bloom::uBloomFramebuf()
{
    return d->uBloomFramebuf;
}

} // namespace gloom
