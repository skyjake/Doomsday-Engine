/** @file lightrender.cpp
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

#include "gloom/render/lightrender.h"
#include "gloom/render/light.h"
#include "gloom/render/gbuffer.h"
#include "gloom/render/ssao.h"
#include "gloom/render/screenquad.h"
#include "gloom/world/map.h"

using namespace de;

namespace gloom {

struct LightData {
    Vec3f lightOrigin;
    Vec3f lightIntensity;
    Vec3f lightDir;
    float radius;
    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

internal::AttribSpec const LightData::_spec[4] = {
    { internal::AttribSpec::Origin,    3, GL_FLOAT, false, sizeof(LightData), 0 * 4 },
    { internal::AttribSpec::Intensity, 3, GL_FLOAT, false, sizeof(LightData), 3 * 4 },
    { internal::AttribSpec::Direction, 3, GL_FLOAT, false, sizeof(LightData), 6 * 4 },
    { internal::AttribSpec::TexCoord,  1, GL_FLOAT, false, sizeof(LightData), 9 * 4 },
};
LIBGUI_VERTEX_FORMAT_SPEC(LightData, 10 * 4)

DENG2_PIMPL(LightRender)
{
    using VBuf = GLBufferT<Vertex3>;

    RenderFunc                        callback;
    GLState                           state;
    std::unique_ptr<Light>            skyLight;
    QHash<ID, std::shared_ptr<Light>> lights;
    QSet<Light *>                     activeLights;
    GLProgram                         surfaceProgram;
    GLProgram                         entityProgram;
    GLProgram                         shadingProgram;
    GLState                           shadingState;
    VBuf                              sphere;
    ScreenQuad                        giQuad;

    GLUniform uLightDir         {"uLightDir",          GLUniform::Vec3};
    GLUniform uViewSpaceLightDir{"uViewSpaceLightDir", GLUniform::Vec3};

    Impl(Public *i) : Base(i)
    {}

    void glInit()
    {
        state.setBlend(false);
        state.setDepthTest(true);
        state.setDepthWrite(true);
        state.setColorMask(gl::WriteNone);
        state.setCull(gl::None);

        shadingState.setBlend(true);
        shadingState.setBlendFunc(gl::One, gl::One);
        shadingState.setDepthTest(true);
        shadingState.setDepthWrite(false);
        shadingState.setCull(gl::Back);

        skyLight.reset(new Light);
        skyLight->setCastShadows(true);

        auto &ctx = self().context();
        ctx.shaders->build(surfaceProgram, "gloom.shadow.surface") << ctx.uLightMatrix;
        ctx.shaders->build(entityProgram,  "gloom.shadow.entity")  << ctx.uLightMatrix;
        ctx.shaders->build(shadingProgram, "gloom.lighting.sources")
                << ctx.view.uMvpMatrix
                << ctx.view.uModelViewMatrix
                << ctx.view.uWorldToViewMatrix3
                << ctx.view.uInverseProjMatrix
                // TODO: skylight
                << ctx.gbuffer->uGBufferAlbedo()
                << ctx.gbuffer->uGBufferEmissive()
                << ctx.gbuffer->uGBufferNormal()
                << ctx.gbuffer->uGBufferDepth();

        giQuad.glInit(self().context());
        ctx.shaders->build(giQuad.program(), "gloom.lighting.global")
                << ctx.view.uInverseProjMatrix
                << ctx.ssao->uSSAOBuf()
                << ctx.gbuffer->uGBufferAlbedo()
                << ctx.gbuffer->uGBufferEmissive()
                << ctx.gbuffer->uGBufferNormal()
                << ctx.gbuffer->uGBufferDepth();

        // Generate a sphere for light bounds.
        {
            VBuf::Builder verts;
            VBuf::Indices inds;
            using Vert = VBuf::Type;

            verts << Vert{{0, 1, 0}} << Vert{{0, -1, 0}}; // top and bottom ends

            const int hFaces = 20;
            const int vFaces = hFaces/2;

            for (uint16_t j = 0; j < vFaces - 1; ++j)
            {
                const float vAngle = PIf * (1 + j) / float(vFaces);
                const float y = cos(vAngle);

                for (uint16_t i = 0; i < hFaces; ++i)
                {
                    const float hAngle = 2.f * PIf * i / float(hFaces);
                    const float x = cos(hAngle) * sin(vAngle);
                    const float z = sin(hAngle) * sin(vAngle);

                    verts << Vert{{x, y, z}};

                    if (j == 0) // top row
                    {
                        inds << 0
                             << 2 + i
                             << 2 + (i + hFaces - 1)%hFaces;
                    }
                    else if (j == vFaces - 2) // bottom row
                    {
                        inds << 1
                             << 2 + j*hFaces + i
                             << 2 + j*hFaces + (i + 1)%hFaces;
                    }
                    else
                    {
                        inds << 2 + j*hFaces + i
                             << 2 + j*hFaces + (i + 1)%hFaces
                             << 2 + (j + 1)*hFaces + i
                             << 2 + j*hFaces + (i + 1)%hFaces
                             << 2 + (j + 1)*hFaces + (i + 1)%hFaces
                             << 2 + (j + 1)*hFaces + i;
                    }
                }
            }

            sphere.setVertices(verts, gl::Static);
            sphere.setIndices(gl::Triangles, inds, gl::Static);
        }
    }

    void glDeinit()
    {
        skyLight.reset();
        giQuad.glDeinit();
    }
};

LightRender::LightRender()
    : d(new Impl(this))
{}

void LightRender::glInit(Context &context)
{
    Render::glInit(context);
    d->glInit();
}

void LightRender::glDeinit()
{
    d->glDeinit();
    Render::glDeinit();
}

void LightRender::render()
{
    // Update shadow maps.
    for (auto *light : d->activeLights)
    {
        if (light->castShadows())
        {
            light->framebuf().clear(GLFramebuffer::Depth | GLFramebuffer::FullClear);

            d->uLightDir = light->direction();
            d->uViewSpaceLightDir =
                context().view.uWorldToViewMatrix3.toMat3f() * light->direction();

            d->state.setTarget(light->framebuf())
                    .setViewport(Rectangleui::fromSize(light->framebuf().size()));

            context().uLightMatrix = light->lightMatrix();

            d->callback(*light);
        }
    }
}

void LightRender::renderLighting()
{
    // Global illumination.
    d->giQuad.state()
            .setBlend(false)
            .setDepthWrite(false)
            .setDepthTest(false)
            .setTarget(GLState::current().target());
    d->giQuad.render();

    // Individual light sources.
    typedef GLBufferT<LightData> LightBuf;
    LightBuf::Vertices lightData;
    for (const auto *light : d->activeLights)
    {
        LightData instance{light->origin(),
                           light->intensity(),
                           light->direction(),
                           light->falloffDistance()};
        lightData << instance;
    }
    if (!lightData.isEmpty())
    {
        // The G-buffer depths are used as-is.
        context().gbuffer->framebuf().blit(GLState::current().target(), GLFramebuffer::Depth);

        d->shadingState.setTarget  (GLState::current().target())
                       .setViewport(GLState::current().viewport());
        d->shadingState.apply();

        LightBuf ibuf;
        ibuf.setVertices(lightData, gl::Dynamic);

        d->shadingProgram.beginUse();
        d->sphere.drawInstanced(ibuf);
        d->shadingProgram.endUse();

        GLState::current().apply();
    }
}

void LightRender::setShadowRenderCallback(RenderFunc callback)
{
    d->callback = callback;
}

void LightRender::createLights()
{
    d->lights.clear();
    d->activeLights.clear();

    d->activeLights.insert(d->skyLight.get());

    const auto &map = *context().map;
    for (auto i = map.entities().begin(), end = map.entities().end(); i != end; ++i)
    {
        const Entity *ent = i.value().get();
        if (ent->type() == Entity::Light)
        {
            auto light = std::make_shared<Light>();
            light->setEntity(ent);
            light->setCastShadows(false);
            light->setType(Light::Omni);
            d->lights.insert(ent->id(), light);
            d->activeLights.insert(light.get());
        }
    }
}

GLTexture &LightRender::shadowMap()
{
    return d->skyLight->shadowMap();
}

Vec3f LightRender::direction() const
{
    return d->skyLight->direction();
}

GLProgram &LightRender::surfaceProgram()
{
    return d->surfaceProgram;
}

GLProgram &LightRender::entityProgram()
{
    return d->entityProgram;
}

GLState &LightRender::shadowState()
{
    return d->state;
}

GLUniform &gloom::LightRender::uLightDir()
{
    return d->uLightDir;
}

GLUniform &LightRender::uViewSpaceLightDir()
{
    return d->uViewSpaceLightDir;
}

} // namespace gloom
