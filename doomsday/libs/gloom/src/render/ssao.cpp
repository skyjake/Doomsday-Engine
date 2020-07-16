/** @file ssao.cpp
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

#include "gloom/render/ssao.h"
#include "gloom/render/screenquad.h"
#include "gloom/render/gbuffer.h"
#include "gloom/render/databuffer.h"

#include <de/glstate.h>

using namespace de;

namespace gloom {

static const dsize SAMPLE_COUNT = 64;

DE_PIMPL(SSAO)
{
    enum { Noisy, Blurred };

    enum ProgramId {
        SampleRandom   = 0,
        DenoiseFactors = 1,
    };

    ScreenQuad        quad;
    GLUniform         uSamples{"uSamples", GLUniform::Vec3Array, SAMPLE_COUNT};
    DataBuffer<Vec2f> noise{"uNoise", Image::RG_32f, gfx::Static};
    GLFramebuffer     ssaoFrameBuf[2];
    GLTexture         ssaoBuf[2];
    GLUniform         uNoisyFactors{"uNoisyFactors", GLUniform::Sampler2D};
    GLUniform         uSSAOBuf{"uSSAOBuf", GLUniform::Sampler2D};

    Impl(Public *i) : Base(i)
    {}

    void updateBuffer()
    {
        const auto bufSize = self().context().gbuffer->size();
        if (ssaoBuf[Noisy].size() != bufSize)
        {
            ssaoBuf[Noisy]  .setUndefinedImage(bufSize, Image::RG_88);  // factor, depth
            ssaoBuf[Blurred].setUndefinedImage(bufSize, Image::R_8);    // denoised factor
            for (int i = 0; i < 2; ++i)
            {
                ssaoFrameBuf[i].configure(GLFramebuffer::Color0, ssaoBuf[i]);
            }
            ssaoBuf[Noisy].setWrap(gfx::ClampToEdge, gfx::ClampToEdge);
            uNoisyFactors = ssaoBuf[Noisy];
        }
        uSSAOBuf = ssaoBuf[Blurred]; // final result
    }

    void glInit(Context &ctx)
    {
        quad.glInit(ctx);

        ctx.shaders->build(quad.program(), "gloom.ssao.sample")
                << ctx.gbuffer->uViewportSize()
                << ctx.gbuffer->uGBufferNormal()
                << ctx.gbuffer->uGBufferDepth()
                << ctx.view.uInverseProjMatrix
                << ctx.view.uProjMatrix;

        ctx.shaders->build(quad.addProgram(DenoiseFactors), "gloom.ssao.denoise")
                << uNoisyFactors;

        // Generate sample kernel.
        {
            Vec3f samples[SAMPLE_COUNT];
            for (auto &sample : samples)
            {
                // Normal-oriented hemisphere.
                sample = Vec3f{Rangef(0, 2).random() - 1,
                               Rangef(0, 2).random() - 1,
                               Rangef(0, 1).random()};
                sample = sample.normalize();
                const float scale = Rangef(0, 1).random();
                // Bias the samples closer to the center.
                sample *= .1f + .9f * scale * scale;
            }
            uSamples.set(samples, SAMPLE_COUNT);
            quad.program() << uSamples;
        }

        // Noise.
        {
            noise.init(16);
            for (int i = 0; i < noise.size(); ++i)
            {
                noise.setData(i, Vec2f{Rangef(0, 2).random() - 1,
                                       Rangef(0, 2).random() - 1});
            }
            noise.update();
            quad.program() << noise.var;
        }
    }

    void glDeinit()
    {
        for (auto &fb : ssaoFrameBuf) fb.deinit();
        for (auto &b  : ssaoBuf)      b.clear();
        noise.clear();
        quad.glDeinit();
    }
};

SSAO::SSAO()
    : d(new Impl(this))
{}

void SSAO::glInit(Context &context)
{
    Render::glInit(context);

    d->glInit(context);
}

void SSAO::glDeinit()
{
    d->glDeinit();
    Render::glDeinit();
}

void SSAO::render()
{
    // Make sure the destination buffer is the correct size.
    d->updateBuffer();

    d->quad.drawable().setProgram(Impl::SampleRandom);
    d->quad.state().setTarget(d->ssaoFrameBuf[Impl::Noisy]); // target is the ssaoBuf
    d->quad.render();

    d->quad.drawable().setProgram(Impl::DenoiseFactors);
    d->quad.state().setTarget(d->ssaoFrameBuf[Impl::Blurred]); // targeting final SSAO factors
    d->quad.render();
}

const GLTexture &SSAO::occlusionFactors() const
{
    return d->ssaoBuf[Impl::Blurred];
}

GLUniform &SSAO::uSSAOBuf()
{
    return d->uSSAOBuf;
}

} // namespace gloom
