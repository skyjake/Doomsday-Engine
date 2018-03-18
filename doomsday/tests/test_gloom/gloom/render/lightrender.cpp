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
#include "gloom/render/maprender.h"
#include "gloom/render/gbuffer.h"
#include "gloom/render/ssao.h"
#include "gloom/render/screenquad.h"
#include "gloom/render/shadow.h"
#include "gloom/world/map.h"

using namespace de;

namespace gloom {

struct LightData {
    Vec3f lightOrigin;
    Vec3f lightIntensity;
    Vec3f lightDir;
    float radius;
    int shadowMapIndex;
    LIBGUI_DECLARE_VERTEX_FORMAT(5)
};

internal::AttribSpec const LightData::_spec[5] = {
    { internal::AttribSpec::Origin,    3, GL_FLOAT, false, sizeof(LightData), 0 * 4 },
    { internal::AttribSpec::Intensity, 3, GL_FLOAT, false, sizeof(LightData), 3 * 4 },
    { internal::AttribSpec::Direction, 3, GL_FLOAT, false, sizeof(LightData), 6 * 4 },
    { internal::AttribSpec::TexCoord,  1, GL_FLOAT, false, sizeof(LightData), 9 * 4 },
    { internal::AttribSpec::Index,     1, GL_FLOAT, false, sizeof(LightData), 10 * 4 },
};
LIBGUI_VERTEX_FORMAT_SPEC(LightData, 11 * 4)

static constexpr int MAX_SHADOWS = 6;

DENG2_PIMPL(LightRender)
{
    using VBuf   = GLBufferT<Vertex3>;
    using Lights = QHash<ID, std::shared_ptr<Light>>;

    std::unique_ptr<Light> skyLight;
    Lights                 lights;
    QSet<Light *>          activeLights;
    RenderFunc             callback;
    GLState                shadowState;
    GLProgram              shadingProgram;
    GLState                shadingState;
    GLProgram              stencilPassProgram;
    GLState                stencilPassState;
    VBuf                   sphere;
    ScreenQuad             giQuad;

    GLUniform uLightDir            {"uLightDir",             GLUniform::Vec3};
    GLUniform uLightIntensity      {"uLightIntensity",       GLUniform::Vec3};
    GLUniform uViewSpaceLightOrigin{"uViewSpaceLightOrigin", GLUniform::Vec3};
    GLUniform uViewSpaceLightDir   {"uViewSpaceLightDir",    GLUniform::Vec3};
    GLUniform uViewToLightMatrix   {"uViewToLightMatrix",    GLUniform::Mat4};
    GLUniform uShadowMap           {"uShadowMap",            GLUniform::Sampler2D}; // <----TESTING-----

    GLUniform uShadowMaps[MAX_SHADOWS] {
        {"uShadowMaps[0]", GLUniform::SamplerCube},
        {"uShadowMaps[1]", GLUniform::SamplerCube},
        {"uShadowMaps[2]", GLUniform::SamplerCube},
        {"uShadowMaps[3]", GLUniform::SamplerCube},
        {"uShadowMaps[4]", GLUniform::SamplerCube},
        {"uShadowMaps[5]", GLUniform::SamplerCube}
    };
    std::unique_ptr<Shadow> dirShadow;
    std::unique_ptr<Shadow> omniShadows[MAX_SHADOWS];
    QHash<const Light *, const Shadow *> activeShadows;

    Impl(Public *i) : Base(i)
    {}

    void glInit()
    {
        shadowState
            .setBlend(false)
            .setDepthTest(true)
            .setDepthWrite(true)
            .setColorMask(gl::WriteNone)
            .setCull(gl::None);

        stencilPassState
            .setColorMask(gl::WriteNone)
            .setBlend(false)
            .setDepthTest(true)
            .setDepthWrite(false)
            .setCull(gl::None)
            .setStencilTest(true)
            .setStencilFunc(gl::Always, 0, 0)
            .setStencilOp(gl::StencilOp::Keep, gl::StencilOp::IncrementWrap, gl::StencilOp::Keep, gl::Front)
            .setStencilOp(gl::StencilOp::Keep, gl::StencilOp::DecrementWrap, gl::StencilOp::Keep, gl::Back);

        shadingState
            .setBlend(true)
            .setBlendFunc(gl::One, gl::One)
            .setDepthTest(false)
            .setDepthWrite(false)
            .setCull(gl::Front)
            .setStencilTest(true)
            .setStencilFunc(gl::NotEqual, 0, 0xff);

        skyLight.reset(new Light);
        skyLight->setType(Light::Directional);
        skyLight->setCastShadows(true);

        // Create shadow maps. These will be assigned to lights as needed.
        {
            dirShadow.reset(new Shadow(Light::Directional));
            for (int i = 0; i < MAX_SHADOWS; ++i)
            {
                omniShadows[i].reset(new Shadow(Light::Omni));
            }
        }

        auto &ctx = self().context();

        ctx.shaders->build(stencilPassProgram, "gloom.light.stencil")
                << ctx.view.uCameraMvpMatrix
                << ctx.view.uModelViewMatrix
                << ctx.view.uWorldToViewRotate;

        ctx.shaders->build(shadingProgram, "gloom.light.sources")
                << ctx.view.uCameraMvpMatrix
                << ctx.view.uModelViewMatrix
                << ctx.view.uWorldToViewRotate
                << ctx.view.uInverseProjMatrix
                << ctx.uEnvMap
                << uShadowMaps[0]
                << uShadowMaps[1]
                << uShadowMaps[2]
                << uShadowMaps[3]
                << uShadowMaps[4]
                << uShadowMaps[5]
                << ctx.view.uViewToWorldRotate;
        ctx.bindGBuffer(shadingProgram);

        giQuad.glInit(self().context());
        ctx.shaders->build(giQuad.program(), "gloom.light.global")
                << ctx.view.uInverseProjMatrix
                << ctx.view.uViewToWorldRotate
                << ctx.uEnvMap
                << ctx.uEnvIntensity
                << ctx.ssao->uSSAOBuf()
                << uShadowMap
                << uViewSpaceLightOrigin
                << uViewSpaceLightDir
                << uLightIntensity
                << uViewToLightMatrix
                << ctx.uLightMatrix;
        ctx.bindGBuffer(giQuad.program());

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
                    else
                    {
                        inds << 2 + (j - 1)*hFaces + i
                             << 2 + (j - 1)*hFaces + (i + 1)%hFaces
                             << 2 + (j + 0)*hFaces + i

                             << 2 + (j - 1)*hFaces + (i + 1)%hFaces
                             << 2 + (j + 0)*hFaces + (i + 1)%hFaces
                             << 2 + (j + 0)*hFaces + i;
                    }
                    if (j == vFaces - 2) // bottom row
                    {
                        inds << 1
                             << 2 + j*hFaces + i
                             << 2 + j*hFaces + (i + 1)%hFaces;
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
    int shadowIndex = 0;
    d->activeShadows.clear();

    // Update shadow maps.
    for (auto *light : d->activeLights)
    {
        if (light->castShadows())
        {
            Shadow *shadow = nullptr;
            if (light->type() == Light::Directional)
            {
                shadow = d->dirShadow.get();
            }
            else
            {
                if (shadowIndex == MAX_SHADOWS) continue;
                shadow = d->omniShadows[shadowIndex++].get();
            }

            d->activeShadows.insert(light, shadow);

            shadow->framebuf().clear(GLFramebuffer::Depth | GLFramebuffer::FullClear);

            d->uLightDir             = light->direction();
            context().uLightOrigin   = light->origin();
            context().uLightFarPlane = light->falloffDistance();

            if (light->type() == Light::Omni)
            {
                d->shadowState.setCull(gl::Front);
                for (int i = 0; i < 6; ++i)
                {
                    context().uLightCubeMatrices.set(i, light->lightMatrix(gl::CubeFace(i)));
                }
            }
            else
            {
                d->shadowState.setCull(gl::None);
                context().uLightMatrix = light->lightMatrix();
            }
            d->uViewSpaceLightDir =
                context().view.uWorldToViewRotate.toMat3f() * light->direction();

            d->shadowState
                    .setTarget(shadow->framebuf())
                    .setViewport(Rectangleui::fromSize(shadow->framebuf().size()));

            d->callback(*light);
        }
    }
}

void LightRender::renderLighting()
{
    auto &     ctx    = context();
    auto &     target = GLState::current().target();
    const auto vp     = GLState::current().viewport();

    // Directional lights.
    {
        const auto &lightMatrix = d->skyLight->lightMatrix();

        ctx.uLightMatrix      = lightMatrix;
        d->uLightIntensity    = d->skyLight->intensity();
        d->uViewSpaceLightDir = ctx.view.uWorldToViewRotate.toMat3f() * d->skyLight->direction();
        d->uViewSpaceLightOrigin = ctx.view.camera->cameraModelView() * d->skyLight->origin();
        d->uViewToLightMatrix    = lightMatrix * ctx.view.camera->cameraModelView().inverse();
        d->uShadowMap            = d->activeShadows[d->skyLight.get()]->shadowMap();
    }

    // Global illumination.
    d->giQuad.state()
            .setBlend(false)
            .setDepthWrite(false)
            .setDepthTest(false)
            .setTarget(target);
    d->giQuad.render();

    typedef GLBufferT<LightData> LightBuf;

    // Individual light sources.
    LightBuf::Vertices lightData;
    int counter = 0;

    for (const auto *light : d->activeLights)
    {
        if (light->type() == Light::Directional)
        {
            // Already shaded during GI pass.
            continue;
        }

        // Assign shadow maps.
        int shadowIndex = -1;
        //if (light->type() == Light::Omni && light->castShadows())
        if (d->activeShadows.contains(light))
        {
            shadowIndex = counter++;
            d->uShadowMaps[shadowIndex] = d->activeShadows[light]->shadowMap();
        }

        LightData instance{light->origin(),
                           light->intensity(),
                           light->direction(),
                           light->falloffDistance(),
                           shadowIndex};
        lightData << instance;
    }

    // The G-buffer depths are used as-is.
    context().gbuffer->framebuf().blit(target, GLFramebuffer::Depth);

    if (!lightData.isEmpty())
    {
        LightBuf ibuf;
        ibuf.setVertices(lightData, gl::Stream);

        // Stencil pass: find out where light volumes intersect surfaces.
        {
            LIBGUI_GL.glClearStencil(0);
            target.clear(GLFramebuffer::Stencil);
            d->stencilPassState.setTarget(target).setViewport(vp).apply();
            d->stencilPassProgram.beginUse();
            d->sphere.drawInstanced(ibuf);
            d->stencilPassProgram.endUse();
        }

        // Shading pass: shade fragments within the light volumes.
        {
            d->shadingState.setTarget(target).setViewport(vp).apply();
            d->shadingProgram.beginUse();
            d->sphere.drawInstanced(ibuf);
            d->shadingProgram.endUse();
        }

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
            light->setType(Light::Omni);
            light->setCastShadows(true);
            light->setIntensity(Vec3f(15, 15, 15));
            d->lights.insert(ent->id(), light);
            d->activeLights.insert(light.get());
        }
    }
}

//Vec3f LightRender::direction() const
//{
//    return d->skyLight->direction();
//}

GLState &LightRender::shadowState()
{
    return d->shadowState;
}

GLUniform &LightRender::uLightDir()
{
    return d->uLightDir;
}

GLUniform &LightRender::uViewSpaceLightDir()
{
    return d->uViewSpaceLightDir;
}

const ICamera *LightRender::testCamera() const
{
    if (d->lights.isEmpty()) return nullptr;
    qDebug() << d->lights.begin().value()->entity()->id();
    return d->lights.begin().value().get();
}

} // namespace gloom
