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

#include <de/Hash>
#include <de/Set>

using namespace de;

namespace gloom {

struct LightData {
    Vec3f lightOrigin;
    Vec3f lightIntensity;
    Vec3f lightDir;
    float radius;
    int   shadowMapIndex;
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

static constexpr int MAX_OMNI_LIGHTS = 6;
static constexpr int MAX_OMNI_SHADOWS = 6;

DE_PIMPL(LightRender)
{
    using VBuf   = GLBufferT<Vertex3>;
    using Lights = Hash<ID, std::shared_ptr<Light>>;

    std::unique_ptr<Light> skyLight;
    Lights                 lights;
    Set<Light *>           activeLights;
    Set<Light *>           shadowCasters; // up to MAX_OMNI_SHADOWS
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
    GLUniform uShadowSize          {"uShadowSize",           GLUniform::Vec2};
    GLUniform uShadowMap           {"uShadowMap",            GLUniform::Sampler2D}; // <----TESTING-----

    GLUniform uShadowMaps[MAX_OMNI_SHADOWS] {
        {"uShadowMaps[0]", GLUniform::SamplerCube},
        {"uShadowMaps[1]", GLUniform::SamplerCube},
        {"uShadowMaps[2]", GLUniform::SamplerCube},
        {"uShadowMaps[3]", GLUniform::SamplerCube},
        {"uShadowMaps[4]", GLUniform::SamplerCube},
        {"uShadowMaps[5]", GLUniform::SamplerCube}
    };
    GLUniform uOmniLightCount{"uOmniLightCount", GLUniform::Int};
    struct OmniLight {
        GLUniform origin;
        GLUniform intensity;
        GLUniform falloffRadius;
        GLUniform shadowIndex;
    };
    OmniLight uOmniLights[MAX_OMNI_LIGHTS] { // TODO: Shader storage buffer would be nice (GLSL 4)
#define OMNI_LIGHT_MEMBERS(idx) { \
            {"uOmniLights["#idx"].origin",        GLUniform::Vec3}, \
            {"uOmniLights["#idx"].intensity",     GLUniform::Vec3}, \
            {"uOmniLights["#idx"].falloffRadius", GLUniform::Float}, \
            {"uOmniLights["#idx"].shadowIndex",   GLUniform::Int} }
        OMNI_LIGHT_MEMBERS(0),
        OMNI_LIGHT_MEMBERS(1),
        OMNI_LIGHT_MEMBERS(2),
        OMNI_LIGHT_MEMBERS(3),
        OMNI_LIGHT_MEMBERS(4),
        OMNI_LIGHT_MEMBERS(5)
#undef OMNI_LIGHT_MEMBERS
    };
    std::unique_ptr<Shadow> dirShadow;
    std::unique_ptr<Shadow> omniShadows[MAX_OMNI_SHADOWS];
    Hash<const Light *, const Shadow *> activeShadows;

    Impl(Public *i) : Base(i)
    {}

    void glInit()
    {
        shadowState
            .setBlend(false)
            .setDepthTest(true)
            .setDepthWrite(true)
            .setColorMask(gfx::WriteNone)
            .setCull(gfx::None);

        stencilPassState
            .setColorMask(gfx::WriteNone)
            .setBlend(false)
            .setDepthTest(true)
            .setDepthWrite(false)
            .setCull(gfx::None)
            .setStencilTest(true)
            .setStencilFunc(gfx::Always, 0, 0)
            .setStencilOp(gfx::StencilOp::Keep, gfx::StencilOp::IncrementWrap, gfx::StencilOp::Keep, gfx::Front)
            .setStencilOp(gfx::StencilOp::Keep, gfx::StencilOp::DecrementWrap, gfx::StencilOp::Keep, gfx::Back);

        shadingState
            .setBlend(true)
            .setBlendFunc(gfx::One, gfx::One)
            .setDepthTest(false)
            .setDepthWrite(false)
            .setCull(gfx::Front)
            .setStencilTest(true)
            .setStencilFunc(gfx::NotEqual, 0, 0xff);

#if 0
        skyLight.reset(new Light);
        skyLight->setType(Light::Directional);
        skyLight->setCastShadows(true);
#endif

        // Create shadow maps. These will be assigned to lights as needed.
        {
            dirShadow.reset(new Shadow(Light::Directional));
            for (int i = 0; i < MAX_OMNI_SHADOWS; ++i)
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
        ctx.shaders->build(giQuad.program(), "gloom.light.global");
        ctx.bindCamera(giQuad.program());
        ctx.bindGBuffer(giQuad.program());
        bindLighting(giQuad.program());

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

            sphere.setVertices(verts, gfx::Static);
            sphere.setIndices(gfx::Triangles, inds, gfx::Static);
        }
    }

    void glDeinit()
    {
        skyLight.reset();
        giQuad.glDeinit();
    }

    void bindLighting(GLProgram &program)
    {
        auto &ctx = self().context();

        program
            // Ambient:
                << ctx.uEnvMap
                << ctx.uEnvIntensity
                << ctx.ssao->uSSAOBuf()
            // Directional:
                << uShadowMap
                << uViewSpaceLightOrigin
                << uViewSpaceLightDir
                << uLightIntensity
                << uViewToLightMatrix
                << ctx.uLightMatrix
            // Omni:
                << uOmniLightCount;

        for (auto &u : uShadowMaps)
        {
            program << u;
        }

        for (int i = 0; i < MAX_OMNI_LIGHTS; ++i)
        {
            program << uOmniLights[i].origin
                    << uOmniLights[i].intensity
                    << uOmniLights[i].falloffRadius
                    << uOmniLights[i].shadowIndex;
        }
    }

    void selectShadowCasters()
    {
        shadowCasters.clear();

        auto &ctx = self().context();
        if (ctx.view.camera)
        {
            using ProxEntry = std::pair<double, Light *>;
            const Vec3d camPos = ctx.view.camera->cameraPosition(); // TODO: Get complete frustum.

            if (skyLight)
            {
                shadowCasters << skyLight.get();
            }

            List<ProxEntry> proxLights;
            proxLights.reserve(activeLights.size());

            // The remaining shadows will be assigned based on proximity.
            for (Light *light : activeLights)
            {
                if (light->castShadows())
                {
                    proxLights << std::make_pair((camPos - light->origin()).length(), light);
                }
            }

            std::sort(proxLights.begin(),
                      proxLights.end(),
                      [](const ProxEntry &a, const ProxEntry &b) { return a.first < b.first; });

            for (const ProxEntry &proxEntry : proxLights)
            {
                // TODO: Is the light falloff volume fully or partially inside the view frustum?
                // (Check a sphere against the 5 frustum planes.)

                shadowCasters << proxEntry.second;

                if (shadowCasters.size() == MAX_OMNI_SHADOWS + 1)
                {
                    break; // skyLight has a separate shadow map
                }
            }
        }
    }

    int assignOmniLights(const std::function<bool (const Light *, int)> &assignLight)
    {
        int totalOmnis = 0;
        int counter = 0;
        for (const auto *light : activeLights)
        {
            if (light->type() == Light::Omni)
            {
                totalOmnis++;

                // Assign shadow maps.
                int shadowIndex = -1;
                if (activeShadows.contains(light) && counter < MAX_OMNI_SHADOWS)
                {
                    shadowIndex = counter++;
                    uShadowMaps[shadowIndex] = activeShadows[light]->shadowMap();
                }

                if (!assignLight(light, shadowIndex)) break;
            }
        }
        return totalOmnis;
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
    for (auto *light : d->shadowCasters)
    {
        DE_ASSERT(light->castShadows());
        {
            Shadow *shadow = nullptr;
            if (light->type() == Light::Directional)
            {
                shadow = d->dirShadow.get();
            }
            else
            {
                if (shadowIndex == MAX_OMNI_SHADOWS) continue;
                shadow = d->omniShadows[shadowIndex++].get();
            }

            d->activeShadows.insert(light, shadow);

            shadow->framebuf().clear(GLFramebuffer::Depth | GLFramebuffer::FullClear);

            d->uLightDir             = light->direction();
            context().uLightOrigin   = light->origin().toVec3f();
            context().uLightFarPlane = light->falloffDistance();

            if (light->type() == Light::Omni)
            {
                d->shadowState.setCull(gfx::Front);
                for (int i = 0; i < 6; ++i)
                {
                    context().uLightCubeMatrices.set(i, light->lightMatrix(gfx::CubeFace(i)));
                }
            }
            else
            {
                d->shadowState.setCull(gfx::None);
                context().uLightMatrix        = light->lightMatrix();
                context().uInverseLightMatrix = light->lightMatrix().inverse();

                d->uShadowSize = d->dirShadow->shadowMap().size();
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

void LightRender::advanceTime(TimeSpan elapsed)
{
    // Pick the shadow casters.
    d->selectShadowCasters();

#if 0
    // Testing.
    {
        Vec3d rotPos = Mat4d::rotate(4.0 * elapsed, Vec3d(0, 1, 0)) * d->skyLight->origin();
        d->skyLight->setOrigin(rotPos);
        d->skyLight->setDirection(-rotPos);
    }
#endif
}

void LightRender::bindLighting(GLProgram &program)
{
    d->bindLighting(program);
}

void LightRender::renderLighting()
{
    auto &     ctx    = context();
    auto &     target = GLState::current().target();
    const auto vp     = GLState::current().viewport();

    // Directional lights.
    if (d->skyLight)
    {
        const auto &lightMatrix = d->skyLight->lightMatrix();

        ctx.uLightMatrix      = lightMatrix;
        d->uLightIntensity    = d->skyLight->intensity();
        d->uViewSpaceLightDir = ctx.view.uWorldToViewRotate.toMat3f() * d->skyLight->direction();
        d->uViewSpaceLightOrigin = ctx.view.camera->cameraModelView() * d->skyLight->origin().toVec3f();
        d->uViewToLightMatrix    = lightMatrix * ctx.view.camera->cameraModelView().inverse();

        if (d->activeShadows.contains(d->skyLight.get()))
        {
            d->uShadowMap = d->activeShadows[d->skyLight.get()]->shadowMap();
        }
    }
    else
    {
        d->uLightIntensity = Vec3f();
    }

    // Select omni lights for global pass.
    Set<const Light *> promoted;
    {
        d->assignOmniLights([this, &ctx, &promoted](const Light *light, int shadowIndex)
        {
            // Only promote lights if the camera is well within the falloff volume.
            const Vec3d camPos = ctx.view.camera->cameraPosition();
            if ((light->origin() - camPos).length() < double(light->falloffDistance()))
            {
                if (promoted.size() == MAX_OMNI_LIGHTS) return false;

                const int promIdx  = promoted.size();
                auto &    omni     = d->uOmniLights[promIdx];
                omni.origin        = ctx.view.camera->cameraModelView() * light->origin().toVec3f();
                omni.intensity     = light->intensity();
                omni.falloffRadius = light->falloffDistance();
                omni.shadowIndex   = shadowIndex;

                promoted.insert(light);
            }
            return true;
        });
        d->uOmniLightCount = promoted.size();
    }

    // Global illumination.
    d->giQuad.state()
            .setBlend(false)
            .setDepthWrite(false)
            .setDepthTest(false)
            .setTarget(target);
    d->giQuad.render();

    typedef GLBufferT<LightData> LightBuf;

    // Individual unlimited light sources.
    LightBuf::Vertices lightData;

    d->assignOmniLights([&lightData, &promoted](const Light *light, int shadowIndex)
    {
        if (!promoted.contains(light))
        {
            LightData instance{light->origin(),
                               light->intensity(),
                               light->direction(),
                               light->falloffDistance(),
                               shadowIndex};
            lightData << instance;
        }
        // Keep going through all the lights.
        return true;
    });

    // The G-buffer depths are used as-is.
    context().gbuffer->framebuf().blit(target, GLFramebuffer::Depth);

    if (!lightData.isEmpty())
    {
        LightBuf ibuf;
        ibuf.setVertices(lightData, gfx::Stream);

        // Stencil pass: find out where light volumes intersect surfaces.
        {
            glClearStencil(0);
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
    d->callback = std::move(callback);
}

void LightRender::createLights()
{
    d->lights.clear();
    d->activeLights.clear();
    if (d->skyLight)
    {
        d->activeLights.insert(d->skyLight.get());
    }

    const auto &map = *context().map;
    for (const auto &i : map.entities())
    {
        const Entity *ent = i.second.get();
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

GLUniform &LightRender::uShadowMap()
{
    return d->uShadowMap;
}

GLUniform &LightRender::uShadowSize()
{
    return d->uShadowSize;
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
    debug("%i", d->lights.begin()->second.get()->entity()->id());
    return d->lights.begin()->second.get();
}

} // namespace gloom
