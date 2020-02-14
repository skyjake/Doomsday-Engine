/** @file world.cpp
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

#include "gloom/world.h"
#include "gloom/render/light.h"
#include "gloom/audio/audiosystem.h"
#include "gloom/render/context.h"
#include "gloom/render/gbuffer.h"
#include "gloom/render/maprender.h"
#include "gloom/render/lightrender.h"
#include "gloom/render/skybox.h"
#include "gloom/render/ssao.h"
#include "gloom/render/tonemap.h"
#include "gloom/render/bloom.h"
#include "gloom/render/screenquad.h"
#include "gloom/world/entitymap.h"
#include "gloom/world/environment.h"
#include "gloom/world/map.h"
#include "gloom/world/user.h"

#include <de/Drawable>
#include <de/File>
#include <de/Folder>
#include <de/GLState>
#include <de/GLTimer>
#include <de/GLWindow>
#include <de/Hash>
#include <de/ModelDrawable>
#include <de/PackageLoader>
#include <de/TextureBank>

#undef environ

using namespace de;

namespace gloom {

struct GLScopedTimer {
    Id _id;

    GLScopedTimer(const Id &id)
        : _id(id)
    {
        GLWindow::getMain().timer().beginTimer(_id);
    }
    ~GLScopedTimer() { GLWindow::getMain().timer().endTimer(_id); }
};

enum {
    MapRenderTimer,
    SkyTimer,
    SSAOTimer,
    MapRenderLightsTimer,
    BloomTimer,
    TonemapTimer,

    PerfTimerCount
};

DE_PIMPL(World), public Asset
, DE_OBSERVES(User, Warp)
{
    GLShaderBank &       shaders;
    ImageBank &          images;
    User *               localUser = nullptr;
    Context              renderContext;
    Environment          environ;
    GLTextureFramebuffer framebuf{Image::RGB_16f};
    GBuffer              gbuffer;
    SkyBox               sky;
    Map                  map;
    Hash<ID, double>     initialPlaneY;
    MapRender            mapRender;
    SSAO                 ssao;
    Bloom                bloom;
    Tonemap              tonemap;
    ScreenQuad           debugQuad;

    Id   timerId[PerfTimerCount];
    int  frameCount = 0;
    Time frameCountStartedAt;

    float  visibleDistance;
    double currentTime = 0.0;

    AtlasTexture *textureAtlas[4];

    Impl(Public *i, GLShaderBank &shaders, ImageBank &images)
        : Base(i)
        , shaders(shaders)
        , images(images)
        , visibleDistance(400) // 500+ meters in all directions
    {
        for (auto &atlas : textureAtlas)
        {
            atlas = AtlasTexture::newWithKdTreeAllocator(
                            Atlas::BackingStore | Atlas::WrapBordersInBackingStore,
                            Atlas::Size(4096 + 64, /*8192*/ 2048 + 64));
            atlas->setMarginSize(0);
#if 1
            atlas->setMaxLevel(4);
            atlas->setBorderSize(16); // room for 4 miplevels
            atlas->setAutoGenMips(true);
            atlas->setFilter(gfx::Linear, gfx::Linear, gfx::MipNearest);
#endif
        }

        renderContext.images    = &images;
        renderContext.shaders   = &shaders;
        renderContext.atlas     = textureAtlas;
        renderContext.ssao      = &ssao;
        renderContext.gbuffer   = &gbuffer;
        renderContext.framebuf  = &framebuf;
        renderContext.bloom     = &bloom;
        renderContext.mapRender = &mapRender;
        renderContext.lights    = &mapRender.lights();
        renderContext.map       = &map;
        renderContext.tonemap   = &tonemap;

        environ.setWorld(thisPublic);
    }

    ~Impl() override
    {
        for (auto &a : textureAtlas) delete a;
    }

    bool glInit()
    {
        if (isReady()) return false;

        debug("[World] glInit");

//        DE_ASSERT(localUser);

        // Cube maps are used for 360-degree env maps, so prefer seamless edge filtering.
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        sky.setSize(visibleDistance);

        renderContext.uCurrentFrameRate = 60.f; // Will be updated when frames are rendered.

        framebuf .glInit();
        gbuffer  .glInit(renderContext);
        sky      .glInit(renderContext);
        mapRender.glInit(renderContext);
        ssao     .glInit(renderContext);
        bloom    .glInit(renderContext);
        tonemap  .glInit(renderContext);
        debugQuad.glInit(renderContext);

        // Debug view.
        {
            renderContext.shaders->build(debugQuad.program(), "gloom.debug")
                    << renderContext.uDebugMode
                    << renderContext.uDebugTex
                    << renderContext.lights->uViewSpaceLightDir()
                    << renderContext.lights->uShadowMap()
                    << renderContext.view.uInverseProjMatrix
                    << ssao.uSSAOBuf()
                    << bloom.uBloomFramebuf()
                    << tonemap.uBrightnessSamples();
            renderContext.bindGBuffer(debugQuad.program());
        }

//        Vec3f const fogColor{.83f, .89f, 1.f};
//        uFog = Vec4f(fogColor, visibleDistance);

        setState(Ready);
        return true;
    }

    void glDeinit()
    {
        setState(NotReady);

        debugQuad.glDeinit();
        tonemap  .glDeinit();
        bloom    .glDeinit();
        ssao     .glDeinit();
        mapRender.glDeinit();
        sky      .glDeinit();
        gbuffer  .glDeinit();
        framebuf .glDeinit();

        for (auto *atl : textureAtlas) atl->clear();

        if (localUser)
        {
            localUser->audienceForWarp -= this;
        }
    }

    void rebuildMap()
    {
        mapRender.rebuild();

        // Remember the initial plane heights.
        {
            for (const auto &i : map.planes())
            {
                initialPlaneY.insert(i.first, i.second.point.y);
            }
        }
    }

    Vec3f lightColor() const
    {
        return Vec3f(1);
    }

    Vec3f lightDirection() const
    {
        return Vec3f(-.45f, .5f, -.89f).normalize();
    }

    void userWarped(const User &) override
    {}

    void update(TimeSpan elapsed)
    {
        currentTime += elapsed;
        renderContext.uCurrentTime = float(currentTime);

        updateEntities(elapsed);
    }

    void updateEntities(TimeSpan)
    {
        for (const auto &i : map.entities())
        {
            auto &ent = *i.second;
            Vec3d pos = ent.position();
            pos.y = self().groundSurfaceHeight(pos);
            ent.setPosition(pos);
        }
    }
};

World::World(GLShaderBank &shaders, ImageBank &images)
    : d(new Impl(this, shaders, images))
{}

void World::loadMap(const String &mapId)
{
    Map loadedMap;
    {
        const auto &asset = App::asset("map." + mapId);
        loadedMap.deserialize(FS::locate<const File>(asset.absolutePath("path")));
    }
    setMap(loadedMap);
}

const Map &World::map() const
{
    return d->map;
}

MapRender &World::mapRender()
{
    return d->mapRender;
}

void World::glInit()
{
    if (d->glInit())
    {
        DE_NOTIFY_VAR(Ready, i)
        {
            i->worldReady(*this);
        }
    }
}

void World::glDeinit()
{
    d->glDeinit();
}

void World::update(TimeSpan elapsed)
{
    d->update(elapsed);
    d->environ.advanceTime(elapsed);
    d->mapRender.advanceTime(elapsed);
    d->tonemap.advanceTime(elapsed);
}

void World::render(const ICamera &camera)
{
    if (!d->isReady()) return;

    const auto frameSize = GLState::current().target().size();

    // Estimate current frame rate.
    {
        d->frameCount++;
        const auto elapsed = d->frameCountStartedAt.since();
        if (elapsed > 1.0)
        {
            d->renderContext.uCurrentFrameRate = d->frameCount / elapsed;
            d->frameCount = 0;
            d->frameCountStartedAt = Time();
        }
    }

    d->renderContext.uDiffuseAtlas     = d->textureAtlas[gloom::Diffuse];
    d->renderContext.uEmissiveAtlas    = d->textureAtlas[gloom::Emissive];
    d->renderContext.uSpecGlossAtlas   = d->textureAtlas[gloom::SpecularGloss];
    d->renderContext.uNormalDisplAtlas = d->textureAtlas[gloom::NormalDisplacement];

    d->framebuf.resize(frameSize);
    d->framebuf.attachedTexture(GLFramebuffer::Color0)
        ->setFilter(gfx::Nearest, gfx::Nearest, gfx::MipNearest);
    d->framebuf.clear(GLFramebuffer::Color0);

    d->gbuffer.resize(frameSize);
    d->gbuffer.clear();

    const ICamera *cam = nullptr; //d->mapRender.lights().testCamera();
    if (!cam) cam = &camera;
    d->renderContext.view.setCamera(*cam);

    // Render the G-buffer contents: material, UV, normals, depth.
    GLState::push()
            .setTarget(d->gbuffer.framebuf())
            .setCull(gfx::Back)
            .setDepthTest(true)
            .setBlend(false);

    {
        GLScopedTimer _{d->timerId[MapRenderTimer]};
        d->mapRender.render();
    }
    {
        GLScopedTimer _{d->timerId[SkyTimer]};
        d->sky.render();
    }
    {
        GLScopedTimer _{d->timerId[SSAOTimer]};
        d->ssao.render();
    }

    GLState::pop();

    // Render the frame: deferred shading using the G-buffer.
    GLState::push().setTarget(d->framebuf);
    {
        GLScopedTimer _{d->timerId[MapRenderLightsTimer]};
        d->mapRender.lights().renderLighting();
    }
    GLState::current().setDepthTest(true).setDepthWrite(false);
    GLState::pop();

    // Forward pass: blended effects.
    {

    }

    // Forward pass: refraction + blend in reflections.
    {
        d->mapRender.renderTransparent();
    }

    // Framebuffer contents are mipmapped for bloom and brightness analysis.
    d->framebuf.attachedTexture(GLFramebuffer::Color0)->generateMipmap();

    // Bloom.
    {
        GLScopedTimer _{d->timerId[BloomTimer]};
        d->bloom.render();
    }

    // Tone mapping.
    {
        GLScopedTimer _{d->timerId[TonemapTimer]};
        d->tonemap.render();
    }

    if (d->renderContext.uDebugMode.toInt() != 0)
    {
        d->debugQuad.state().setTarget(GLState::current().target());
        d->debugQuad.render();
    }

#if 0
    {
        auto &perfTimer = GLWindow::getMain().timer();
        for (int i = 0; i < PerfTimerCount; ++i)
        {
            debug("Timer %i: %8llu µs", i, perfTimer.elapsedTime(d->timerId[i]).asMicroSeconds());
        }
    }
#endif
}

User *World::localUser() const
{
    return d->localUser;
}

World::POI World::initialViewPosition() const
{
    return {Vec3f(0.f), 90};
}

List<World::POI> World::pointsOfInterest() const
{
    return {POI(initialViewPosition())};
}

double World::groundSurfaceHeight(const Vec3d &posMeters) const
{
    const auto sec_vol = d->map.findSectorAndVolumeAt(posMeters / d->map.metersPerUnit());
    if (sec_vol.first)
    {
        const Volume &vol = d->map.volume(sec_vol.second);
        return d->map.plane(vol.planes[0]).projectPoint(Point{posMeters.xz()}).y *
               d->map.metersPerUnit().y;
    }
    return 0;
}

double World::ceilingHeight(const Vec3d &) const
{
    return 1000;
}

void World::setLocalUser(User *user)
{
    if (d->localUser)
    {
        d->localUser->audienceForWarp -= d;
    }
    d->localUser = user;
    d->localUser->setWorld(this);
    if (d->localUser)
    {
        d->localUser->audienceForWarp += d;
    }
}

void World::setMap(const Map &map)
{
    d->map = map;
    d->rebuildMap();
}

void World::setDebugMode(int debugMode)
{
    d->renderContext.uDebugMode = debugMode;
    /*
    switch (debugMode)
    {
        case 1:
            d->renderContext.uDebugTex = d->tonemap.brightnessSample(0);
            break;
        case 2:
            d->renderContext.uDebugTex = d->tonemap.brightnessSample(1);
            break;
        case 3:
            d->renderContext.uDebugTex = d->mapRender.lights().shadowMap();
            break;
    }*/
}

void World::setCurrentTime(double time)
{
    d->currentTime = time;
}

} // namespace gloom
