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

#include "gloomworld.h"
#include "ilight.h"
#include "audio/audiosystem.h"
#include "render/skybox.h"
#include "render/maprender.h"
#include "world/entitymap.h"
#include "world/environment.h"
#include "world/user.h"
#include "world/map.h"
#include "../src/gloomapp.h"

#include <de/Drawable>
#include <de/File>
#include <de/Folder>
#include <de/PackageLoader>
#include <de/GLState>
#include <de/ModelDrawable>
#include <de/TextureBank>

using namespace de;

namespace gloom {

DENG2_PIMPL(GloomWorld), public ILight
, DENG2_OBSERVES(User, Warp)
{
    User *localUser = nullptr;
//    HeightMap height;
//    HeightField land;
    SkyBox sky;
    Environment environ;
    Map map;
    MapRender mapRender;

    float visibleDistance;
//    Vector2f mapSize;
//    float heightRange;

//    typedef GLBufferT<Vertex3Tex2BoundsRgba> SkyVBuf;

    std::unique_ptr<AtlasTexture> atlas;
    GLUniform uModelProj { "uViewProjMatrix",   GLUniform::Mat4 };
    GLUniform uViewPos   { "uViewPos",          GLUniform::Vec3 };
    GLUniform uFog       { "uFog",              GLUniform::Vec4 };
    GLUniform uLightDir  { "uLightDir",         GLUniform::Vec3 };
//    ModelDrawable trees[NUM_MODELS];
    GLUniform uTex       { "uTex",              GLUniform::Sampler2D };
//    Id heightMap;
//    Id normalMap;

    Impl(Public *i)
        : Base(i)
        , visibleDistance(1.4f * 512 /*500*/) // 500+ meters in all directions
//        , mapSize(1.4 * 2048, 1.4 * 2048)  // 2km x 2km map (1 pixel == 1 meter)
//        , heightRange(100)     // 100m height differences
    {
        atlas.reset(AtlasTexture::newWithKdTreeAllocator(
                        Atlas::BackingStore | Atlas::WrapBordersInBackingStore,
                        Atlas::Size(4096 + 64, 8192 /*4096 + 64*/)));
        atlas->setMarginSize(0);
#if 1
        atlas->setMaxLevel(4);
        atlas->setBorderSize(16); // room for 4 miplevels
        atlas->setAutoGenMips(true);
        atlas->setFilter(gl::Linear, gl::Linear, gl::MipNearest);
#endif
        uTex = *atlas;

        environ.setWorld(thisPublic);
    }

    //void loadTextures()
    //{
//        const ImageBank &images = GloomApp::images();

//        height.loadGrayscale(images.image("world.heightmap"));
//        height.setMapSize(mapSize, heightRange);
//        heightMap = atlas->alloc(height.toImage());
//        normalMap = atlas->alloc(height.makeNormalMap());

//        Id stone = atlas->alloc(images.image("world.stone"));
//        Id grass = atlas->alloc(images.image("world.grass"));
//        Id dirt  = atlas->alloc(images.image("world.dirt"));
//        land.setMaterial(HeightField::Stone, atlas->imageRectf(stone));
//        land.setMaterial(HeightField::Grass, atlas->imageRectf(grass));
//        land.setMaterial(HeightField::Dirt,  atlas->imageRectf(dirt));
//    }

    void glInit()
    {
        DENG2_ASSERT(localUser);

        //loadTextures();
        //loadModels();

        sky.setAtlas(*atlas);
        sky.setSize(visibleDistance);
        sky.glInit();

        mapRender.setMap(map);
        mapRender.setAtlas(*atlas);
        mapRender.glInit();

        Vector3f const fogColor{.83f, .89f, 1.f};
        uFog = Vector4f(fogColor, visibleDistance);

        // Entities.
        //ents.setSize(mapSize);
//        generateEntities();
    }

    void glDeinit()
    {
//        for(int i = 0; i < NUM_MODELS; ++i)
//        {
//            trees[i].glDeinit();
//        }
//        land.glDeinit();
        sky.glDeinit();
        mapRender.glDeinit();

        atlas->clear();

        localUser->audienceForWarp -= this;
    }

    void rebuildMap()
    {
        mapRender.rebuild();
    }

    Vector3f lightColor() const
    {
        return Vector3f(1, 1, 1);
    }

    Vector3f lightDirection() const
    {
        return Vector3f(-.45f, .5f, -.89f).normalize();
    }

    void userWarped(User const &)
    {
        //land.skipMeshBlending();
    }

//    void positionOnGround(Entity &ent, Vector2f const &surfacePos)
//    {
//        ent.setPosition(Vector3f(surfacePos.x,
//                                 height.heightAtPosition(surfacePos) + .05f,
//                                 surfacePos.y));
//    }

//    bool isFlatSurface(Vector2f const &pos) const
//    {
//        return (height.normalAtPosition(pos).y < -.9 &&
//                height.normalAtPosition(pos + Vector2f(-1, -1)).y < -.9 &&
//                height.normalAtPosition(pos + Vector2f(1, -1)).y < -.9 &&
//                height.normalAtPosition(pos + Vector2f(-1, 1)).y < -.9 &&
//                height.normalAtPosition(pos + Vector2f(1, 1)).y < -.9);
//    }

//    void generateEntities()
//    {
//        for(int i = 0; i < 10000; ++i)
//        {
//            Vector2f pos;
//            do
//            {
//                pos = -mapSize/2 + Vector2f(frand() * mapSize.x, frand() * mapSize.y);
//            }
//            while(!isFlatSurface(pos));

//            Entity *ent = new Entity;
//            ent->setType(frand() < .333? Entity::Tree1 : frand() < .5? Entity::Tree2 : Entity::Tree3);
//            ent->setAngle(frand() * 360);
//            ent->setScale(.05f + frand() * .28);
//            positionOnGround(*ent, pos);
//            ents.insert(ent);
//        }
//    }

//    typedef GLBufferT<InstanceData> InstanceBuf;

//    void drawEntities(ICamera const &camera)
//    {
//        uModelProj = camera.cameraModelViewProjection();

//        float fullDist = 500;
//        QList<Entity const *> entities = ents.listRegionBackToFront(camera.cameraPosition(), fullDist);

//        InstanceBuf ibuf;

//        // Draw all model types.
//        for(int i = 0; i < NUM_MODELS; ++i)
//        {
//            InstanceBuf::Builder data;

//            ModelDrawable const *model = &trees[i];

//            // Set up the instance buffer.
//            foreach(Entity const *e, entities)
//            {
//                if(e->type() != Entity::Tree1 + i) continue;

//                float dims = model->dimensions().z * e->scale().y;

//                float maxDist = min(fullDist, dims * 10);
//                float fadeItv = .333f * maxDist;
//                float distance = (e->position() - camera.cameraPosition()).length();

//                if(distance < maxDist)
//                {
//                    InstanceBuf::Type inst;
//                    inst.matrix =
//                            Matrix4f::translate(e->position()) *
//                            Matrix4f::rotate(e->angle(), Vector3f(0, -1, 0)) *
//                            Matrix4f::rotate(90, Vector3f(1, 0, 0)) *
//                            Matrix4f::scale(e->scale());

//                    // Fade in the distance.
//                    inst.color = Vector4f(1, 1, 1, clamp(0.f, 1.f - (distance - maxDist + fadeItv)/fadeItv, 1.f));

//                    data << inst;
//                }
//            }

//            if(!data.isEmpty())
//            {
//                ibuf.setVertices(data, gl::Dynamic);
//                model->drawInstanced(ibuf);
//            }
//        }
//    }
};

GloomWorld::GloomWorld() : d(new Impl(this))
{}

void GloomWorld::glInit()
{
    d->glInit();
    DENG2_FOR_AUDIENCE(Ready, i)
    {
        i->worldReady(*this);
    }
}

void GloomWorld::glDeinit()
{
    d->glDeinit();
}

void GloomWorld::update(TimeSpan const &elapsed)
{
    d->environ.advanceTime(elapsed);
    d->mapRender.advanceTime(elapsed);
}

void GloomWorld::render(ICamera const &camera)
{
    //DENG2_ASSERT(d->modelProgram.isReady());

    GLState::push()
            .setCull(gl::Back)
            .setDepthTest(true);

    const Matrix4f mvp = camera.cameraModelViewProjection();

    d->uViewPos  = camera.cameraPosition();
    d->uLightDir = d->lightDirection();

    d->mapRender.render(camera);
    d->sky.render(mvp * Matrix4f::translate(camera.cameraPosition()));

//    d->land.draw(mvp);
//    d->drawEntities(camera);

    GLState::pop();
}

User *GloomWorld::localUser() const
{
    return d->localUser;
}

World::POI GloomWorld::initialViewPosition() const
{
    return POI(Vector3f(0, 0, 0), 90);
}

QList<World::POI> GloomWorld::pointsOfInterest() const
{
    return QList<POI>({ POI(initialViewPosition()) });
}

float GloomWorld::groundSurfaceHeight(Vector3f const &pos) const
{
    return groundSurfaceHeight(Vector2f(pos.x, pos.z));
}

float GloomWorld::groundSurfaceHeight(Vector2f const &worldMapPos) const
{
    DENG2_UNUSED(worldMapPos);
    //return d->height.heightAtPosition(worldMapPos);
    return 0;
}

float GloomWorld::ceilingHeight(Vector3f const &) const
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

} // namespace gloom
