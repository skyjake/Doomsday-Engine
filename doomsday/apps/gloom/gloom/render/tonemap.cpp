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

#include <de/Animation>
#include <de/GLFramebuffer>

using namespace de;

namespace gloom {

DE_PIMPL(Tonemap)
{
    ScreenQuad    quad;
    GLUniform     uFramebuf{"uFramebuf", GLUniform::Sampler2D};
    GLUniform     uExposure{"uExposure", GLUniform::Float};
    GLFramebuffer brightnessFramebuf[2];
    GLTexture     brightnessSamples[2];
    int           brightnessSampleIndex = 0;
    ScreenQuad    brightnessQuad;
    double        brightnessTime = 0;
    Animation     exposure{.25f, Animation::Linear};

    Impl(Public *i) : Base(i)
    {}
};

Tonemap::Tonemap()
    : d(new Impl(this))
{}

void Tonemap::glInit(Context &context)
{
    Render::glInit(context);

    d->quad.glInit(context);
    context.shaders->build(d->quad.program(), "gloom.tonemap.exposure")
            << d->uFramebuf
            << context.bloom->uBloomFramebuf()
            << d->uExposure
            << context.uDebugMode
            << context.uDebugTex;

    // Brightness analysis.
    {
        for (int i = 0; i < 2; ++i)
        {
            auto &bs = d->brightnessSamples[i];

            bs.setAutoGenMips(false);
            bs.setFilter(gfx::Nearest, gfx::Nearest, gfx::MipNone);
            bs.setUndefinedContent(Vec2ui(4, 4), GLPixelFormat(GL_RGB16F, GL_RGB, GL_FLOAT));

            d->brightnessFramebuf[i].configure(GLFramebuffer::Color0, bs);
        }
        d->brightnessQuad.glInit(context);
        context.shaders->build(d->brightnessQuad.program(), "gloom.tonemap.sample")
            << d->uFramebuf;
    }
}

void Tonemap::glDeinit()
{
    d->quad.glDeinit();
    d->brightnessQuad.glDeinit();
    for (auto &bf : d->brightnessFramebuf) bf.configure();

    Render::glDeinit();
}

void Tonemap::render()
{
    d->uFramebuf = context().framebuf->attachedTexture(GLFramebuffer::Color0);
    d->uExposure = d->exposure;

    // Brightness sampling.
    {
        auto &idx = d->brightnessSampleIndex;
        idx = (idx + 1) % 2;
        GLState::push()
                .setTarget(d->brightnessFramebuf[idx])
                .setViewport(Rectangleui::fromSize(d->brightnessSamples[0].size()));
        d->brightnessQuad.state().setTarget(d->brightnessFramebuf[idx]);
        d->brightnessQuad.render();
        GLState::pop();
    }

    // Perform the tone mapping with exposure adjustment.
    {
        d->quad.render();
    }
}

void Tonemap::advanceTime(TimeSpan elapsed)
{
    d->brightnessTime += elapsed;
    if (d->brightnessTime < 0.25) return;
    d->brightnessTime = 0;

    const auto &bs = d->brightnessSamples[d->brightnessSampleIndex];
    List<Vec3f> sample(bs.size().area());
    if (sample.isEmpty()) return;

    // Read the previous frame's brightness sample.
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,
                             d->brightnessFramebuf[d->brightnessSampleIndex].glName());
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, bs.size().x, bs.size().y, GL_RGB, GL_FLOAT, &sample[0]);
        GLState::current().target().glBind();
    }

    // TODO: try using the median brightness instead of max

    //const Vec3f grayscale{0.2126f, 0.7152f, 0.0722f};
    const Vec3f grayscale{.333f, .333f, .333f};

    float brightest = 0;
    for (const auto &s : sample)
    {
        brightest = de::max(float(s.dot(grayscale)), brightest);
//        brightest = de::max(s.max(), brightest);
    }

    // The adjustment is kept below 1.0 to avoid overbrightening dark scenes.
    d->exposure.setValue(de::min(1.0f, 1.75f / brightest), 1.0);
//    d->exposure.setValue(de::min(1.0f, 1.0f / brightest), 1.0);
}

GLTexture &Tonemap::brightnessSample(int index) const
{
    return d->brightnessSamples[index];
}

GLUniform &Tonemap::uExposure() const
{
    return d->uExposure;
}

} // namespace gloom
