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
#include "gloom/render/gbuffer.h"
#include "gloom/render/bloom.h"

#include <de/animation.h>
#include <de/glframebuffer.h>

using namespace de;

namespace gloom {

DE_PIMPL(Tonemap)
{
    ScreenQuad    tonemapQuad;
    GLUniform     uFramebuf{"uFramebuf", GLUniform::Sampler2D}; // read framebuffer contents
    GLFramebuffer brightnessFramebuf;                           // blending sample values
    GLTexture     brightnessSamples;                            // store averaged sample values
    GLUniform     uBrightnessSamples{"uBrightnessSamples", GLUniform::Sampler2D};
    ScreenQuad    brightnessQuad;

    Impl(Public *i) : Base(i)
    {}
};

Tonemap::Tonemap()
    : d(new Impl(this))
{}

void Tonemap::glInit(Context &context)
{
    Render::glInit(context);

    // Brightness analysis.
    {
        // Small buffer for storing averaged brightness values.
        {
            auto &bs = d->brightnessSamples;
            bs.setAutoGenMips(false);
            bs.setFilter(gfx::Nearest, gfx::Nearest, gfx::MipNone);
            bs.setUndefinedContent(Vec2ui(4, 4), GLPixelFormat(GL_RGB16F, GL_RGB, GL_FLOAT));
            d->brightnessFramebuf.configure(GLFramebuffer::Color0, bs);
            d->brightnessFramebuf.clear(GLFramebuffer::Color0);
            d->uBrightnessSamples = bs;
        }
        d->brightnessQuad.glInit(context);
        context.shaders->build(d->brightnessQuad.program(), "gloom.tonemap.sample")
            << d->uFramebuf
            << context.uCurrentFrameRate;

        // Samples are blended for smooth exposure changes.
        d->brightnessQuad.state()
            .setBlend(true)
            .setBlendFunc(gfx::SrcAlpha, gfx::OneMinusSrcAlpha);
    }

    d->tonemapQuad.glInit(context);
    context.shaders->build(d->tonemapQuad.program(), "gloom.tonemap.exposure")
            << d->uFramebuf
            << context.bloom->uBloomFramebuf()
            << d->uBrightnessSamples
            << context.uDebugMode
            << context.uDebugTex;
}

void Tonemap::glDeinit()
{
    d->tonemapQuad.glDeinit();
    d->brightnessQuad.glDeinit();
    d->brightnessFramebuf.configure();
    d->brightnessSamples.clear();

    Render::glDeinit();
}

void Tonemap::render()
{
    d->uFramebuf = context().framebuf->attachedTexture(GLFramebuffer::Color0);

    // Brightness sampling.
    {
        GLState::push()
                .setTarget(d->brightnessFramebuf)
                .setViewport(Rectangleui::fromSize(d->brightnessSamples.size()));
        d->brightnessQuad.state().setTarget(d->brightnessFramebuf);
        d->brightnessQuad.render();
        GLState::pop();
    }

    // Tone map the frame (with exposure adjustment based on sampled values).
    {
        d->tonemapQuad.state().setTarget(GLState::current().target());
        d->tonemapQuad.render();
    }
}

GLUniform &Tonemap::uBrightnessSamples() const
{
    return d->uBrightnessSamples;
}

} // namespace gloom
