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

#include <de/GLState>

using namespace de;

namespace gloom {

static const dsize SAMPLE_COUNT = 64;

DENG2_PIMPL(SSAO)
{
    ScreenQuad        quad;
    GLUniform         uSamples{"uSamples", GLUniform::Vec3Array, SAMPLE_COUNT};
    DataBuffer<Vec3f> noise{"uNoise", Image::RGB_16f};
    GLFramebuffer     ssaoFrameBuf;
    GLTexture         ssaoBuf;

    Impl(Public *i) : Base(i)
    {}

    void updateBuffer()
    {
        const auto bufSize = self().context().gbuffer->size();
        if (ssaoBuf.size() != bufSize)
        {
            ssaoBuf.setUndefinedImage(bufSize, Image::R_8);

        }
    }
};

SSAO::SSAO()
    : d(new Impl(this))
{}

void SSAO::glInit(const Context &context)
{
    Render::glInit(context);
    d->quad.glInit(context);
    context.shaders->build(d->quad.program(), "gloom.ssao")
            << context.gbuffer->uGBufferAlbedo()
            << context.gbuffer->uGBufferNormal()
            << context.gbuffer->uGBufferDepth()
            << context.view.uInverseProjMatrix
            << context.view.uProjMatrix;

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
        d->uSamples.set(samples, SAMPLE_COUNT);
        d->quad.program() << d->uSamples;
    }

    // Noise.
    {
        d->noise.init(64);
        for (int i = 0; i < d->noise.elementCount; ++i)
        {
            d->noise.setData(i, Vec3f{Rangef(0, 2).random() - 1,
                                         Rangef(0, 2).random() - 1,
                                         0});
        }
        d->noise.update();
        d->quad.program() << d->noise.var;
    }
}

void SSAO::glDeinit()
{
    d->quad.glDeinit();
    Render::glDeinit();
}

void SSAO::render()
{
    // Make sure the destination buffer is the correct size.
    d->updateBuffer();

    d->quad.state().setTarget(d->ssaoFrameBuf); // target is the ssaoBuf
    d->quad.render();
}

} // namespace gloom
