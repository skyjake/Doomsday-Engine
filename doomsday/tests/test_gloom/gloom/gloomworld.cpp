/** @file gloomworld.cpp
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

#include "gloom/gloomworld.h"
#include "gloom/render/light.h"
#include "gloom/audio/audiosystem.h"
#include "gloom/render/context.h"
#include "gloom/render/gbuffer.h"
#include "gloom/render/maprender.h"
#include "gloom/render/lightrender.h"
#include "gloom/render/skybox.h"
#include "gloom/render/ssao.h"
#include "gloom/render/tonemap.h"
#include "gloom/world/entitymap.h"
#include "gloom/world/environment.h"
#include "gloom/world/map.h"
#include "gloom/world/user.h"
#include "../src/gloomapp.h" // access to shader bank

#include <de/Drawable>
#include <de/File>
#include <de/Folder>
#include <de/PackageLoader>
#include <de/GLState>
#include <de/ModelDrawable>
#include <de/TextureBank>

using namespace de;

namespace gloom {

DENG2_PIMPL(GloomWorld), public Asset
, DENG2_OBSERVES(User, Warp)
{
    User *            localUser = nullptr;
    Context           renderContext;
    Environment       environ;
    GLTextureFramebuffer framebuf{Image::RGB_16f};
    GBuffer           gbuffer;
    SkyBox            sky;
    Map               map;
    QHash<ID, double> initialPlaneY;
    MapRender         mapRender;
    SSAO              ssao;
    Tonemap           tonemap;

    float  visibleDistance;
    double currentTime = 0.0;

    std::unique_ptr<AtlasTexture> atlas;

    Impl(Public *i)
        : Base(i)
        , visibleDistance(1.4f * 512 /*500*/) // 500+ meters in all directions
    {
        atlas.reset(AtlasTexture::newWithKdTreeAllocator(
                        Atlas::BackingStore | Atlas::WrapBordersInBackingStore,
                        Atlas::Size(4096 + 64, /*8192*/ 4096 + 64)));
        atlas->setMarginSize(0);
#if 1
        atlas->setMaxLevel(4);
        atlas->setBorderSize(16); // room for 4 miplevels
        atlas->setAutoGenMips(true);
        atlas->setFilter(gl::Linear, gl::Linear, gl::MipNearest);
#endif

        renderContext.images   = &GloomApp::images(); // TODO: remove dependency on App
        renderContext.shaders  = &GloomApp::shaders();
        renderContext.atlas    = atlas.get();
        renderContext.uAtlas   = renderContext.atlas;
        renderContext.ssao     = &ssao;
        renderContext.gbuffer  = &gbuffer;
        renderContext.framebuf = &framebuf;
        renderContext.lights   = &mapRender.lights();
        renderContext.map      = &map;

        environ.setWorld(thisPublic);
    }

    bool glInit()
    {
        if (isReady()) return false;

        qDebug() << "[GloomWorld] glInit";

        DENG2_ASSERT(localUser);

        // Cube maps are used for 360-degree env maps, so prefer seamless edge filtering.
        LIBGUI_GL.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        sky.setSize(visibleDistance);

        framebuf .glInit();
        gbuffer  .glInit(renderContext);
        sky      .glInit(renderContext);
        mapRender.glInit(renderContext);
        ssao     .glInit(renderContext);
        tonemap  .glInit(renderContext);

//        Vec3f const fogColor{.83f, .89f, 1.f};
//        uFog = Vec4f(fogColor, visibleDistance);

        setState(Ready);
        return true;
    }

    void glDeinit()
    {
        setState(NotReady);

        tonemap  .glDeinit();
        ssao     .glDeinit();
        mapRender.glDeinit();
        sky      .glDeinit();
        gbuffer  .glDeinit();
        framebuf .glDeinit();

        atlas->clear();

        localUser->audienceForWarp -= this;
    }

    void rebuildMap()
    {
        mapRender.rebuild();

        // Remember the initial plane heights.
        {
            for (auto i = map.planes().constBegin(), end = map.planes().constEnd(); i != end; ++i)
            {
                initialPlaneY.insert(i.key(), i.value().point.y);
            }
        }
    }

    Vec3f lightColor() const
    {
        return Vec3f(1, 1, 1);
    }

    Vec3f lightDirection() const
    {
        return Vec3f(-.45f, .5f, -.89f).normalize();
    }

    void userWarped(const User &)
    {}

    void update(const TimeSpan &elapsed)
    {
        currentTime += elapsed;

        renderContext.uCurrentTime = float(currentTime);

        for (auto i = map.planes().begin(), end = map.planes().end(); i != end; ++i)
        {
            const float planeY = float(initialPlaneY[i.key()]) +
                                 std::sin(i.key() + float(currentTime) * .1f);
            i.value().point.y = planeY;
        }

        updateEntities(elapsed);
    }

    void updateEntities(const TimeSpan &)
    {
        for (auto i = map.entities().begin(), end = map.entities().end(); i != end; ++i)
        {
            auto &ent = *i.value();
            Vec3d pos = ent.position();
            pos.y = self().groundSurfaceHeight(pos);
            ent.setPosition(pos);
        }
    }

//    void positionOnGround(Entity &ent, Vec2f const &surfacePos)
//    {
//        ent.setPosition(Vec3f(surfacePos.x,
//                                 height.heightAtPosition(surfacePos) + .05f,
//                                 surfacePos.y));
//    }

//    bool isFlatSurface(Vec2f const &pos) const
//    {
//        return (height.normalAtPosition(pos).y < -.9 &&
//                height.normalAtPosition(pos + Vec2f(-1, -1)).y < -.9 &&
//                height.normalAtPosition(pos + Vec2f(1, -1)).y < -.9 &&
//                height.normalAtPosition(pos + Vec2f(-1, 1)).y < -.9 &&
//                height.normalAtPosition(pos + Vec2f(1, 1)).y < -.9);
//    }

};

GloomWorld::GloomWorld() : d(new Impl(this))
{}

void GloomWorld::glInit()
{
    if (d->glInit())
    {
        DENG2_FOR_AUDIENCE(Ready, i)
        {
            i->worldReady(*this);
        }
    }
}

void GloomWorld::glDeinit()
{
    d->glDeinit();
}

void GloomWorld::update(TimeSpan const &elapsed)
{
    d->update(elapsed);
    d->environ.advanceTime(elapsed);
    d->mapRender.advanceTime(elapsed);
    d->tonemap.advanceTime(elapsed);
}

void GloomWorld::render(ICamera const &camera)
{
    if (!d->isReady()) return;

    const auto frameSize = GLState::current().target().size();

    d->framebuf.resize(frameSize);
    d->framebuf.attachedTexture(GLFramebuffer::Color0)
        ->setFilter(gl::Nearest, gl::Nearest, gl::MipNearest);
    d->framebuf.clear(GLFramebuffer::Color0);

    d->gbuffer.resize(frameSize);
    d->gbuffer.clear();

    d->renderContext.view.setCamera(camera);

    // Render the G-buffer contents: albedo, normals, depth.
    GLState::push()
            .setTarget(d->gbuffer.framebuf())
            .setCull(gl::Back)
            .setDepthTest(true);

    d->mapRender.render();
    d->ssao.render();
    d->sky.render();

    GLState::pop();

    // Render the frame: deferred shading using the G-buffer.
    GLState::push().setTarget(d->framebuf);
    d->mapRender.lights().renderLighting();
    GLState::pop();

    // Framebuffer contents are mipmapped for brightness analysis.
    d->framebuf.attachedTexture(GLFramebuffer::Color0)->generateMipmap();

    d->tonemap.render();
}

User *GloomWorld::localUser() const
{
    return d->localUser;
}

World::POI GloomWorld::initialViewPosition() const
{
    return POI(Vec3f(0, 0, 0), 90);
}

QList<World::POI> GloomWorld::pointsOfInterest() const
{
    return QList<POI>({ POI(initialViewPosition()) });
}

float GloomWorld::groundSurfaceHeight(Vec3f const &pos) const
{
    const auto sec_vol = d->map.findSectorAndVolumeAt(pos);
    if (sec_vol.first)
    {
        const Volume &vol = d->map.volume(sec_vol.second);
        return float(d->map.plane(vol.planes[0]).projectPoint(pos.xz()).y);
    }
    return 0;
}

float GloomWorld::ceilingHeight(Vec3f const &) const
{
    //return -d->heightRange * 2;
    return 1000;
}

void GloomWorld::setLocalUser(User *user)
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

void GloomWorld::setMap(const Map &map)
{
    d->map = map;
    d->rebuildMap();
}

void GloomWorld::setDebugMode(int debugMode)
{
    d->renderContext.uDebugMode = debugMode;
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
    }
}

} // namespace gloom
