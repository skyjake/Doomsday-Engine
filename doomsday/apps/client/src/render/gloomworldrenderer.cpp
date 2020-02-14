/** @file gloomworldrenderer.cpp
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/gloomworldrenderer.h"
#include "render/rendersystem.h"
#include "render/rend_main.h"
#include "world/p_players.h"
#include "clientapp.h"

#include <doomsday/world/map.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/world.h>
#include <gloom/world.h>
#include <gloom/world/map.h>
#include <gloom/render/icamera.h>
#include <gloom/render/maprender.h>
#include <de/ImageBank>
#include <de/FS>
#include <de/Value>
#include <de/ScriptSystem>
#include <de/data/json.h>
#include <de/legacy/timer.h>

using namespace de;

Value *Function_Render_SetDebugMode(Context &, const Function::ArgumentValues &args)
{
    auto &wr = static_cast<GloomWorldRenderer &>(ClientApp::render().world());
    wr.glWorld().setDebugMode(args.at(0)->asInt());
    return nullptr;
}

DE_PIMPL(GloomWorldRenderer)
, DE_OBSERVES(world::World, PlaneMovement)
{
    Binder binder;

    /**
     * Gloom camera for rendering a specific player's view.
     */
    struct PlayerCamera : public gloom::ICamera
    {
        Vec3f pos, up, front;
        Mat4f mvMat, projMat;

        void update(int console, const Vec3f &metersPerUnit)
        {
            const Vec3f worldMirror{1.0f, 1.0f, -1.0f};

            player_t *player = DD_Player(console);
            viewdata_t *vd   = &player->viewport();

            pos     = vd->current.origin.xzy() * metersPerUnit * worldMirror;
            up      = vd->upVec * worldMirror;
            front   = vd->frontVec * worldMirror;

            // These axis flips are a bit silly, but they are here because the view matrices
            // come from Doomsday's old renderer. They also assume clockwise triangle winding.
            // Gloom uses counterclockwise (OpenGL default). We need to invert the coordinate
            // axes accordingly.

            mvMat   = Mat4f::scale(metersPerUnit) *
                      Mat4f::rotate(180, Vec3f(0, 1, 0)) *
                      Rend_GetModelViewMatrix(console) *
                      Mat4f::scale(Vec3f(1.0f) / metersPerUnit) *
                      Mat4f::scale(worldMirror);

            projMat = Rend_GetProjectionMatrix(0.0f, metersPerUnit.x /* clip planes in meters */) *
                      Mat4f::scale(Vec3f(-1, 1, -1));
        }

        Vec3f cameraPosition() const
        {
            return pos;
        }

        Vec3f cameraFront() const
        {
            return front;
        }

        Vec3f cameraUp() const
        {
            return up;
        }

        Mat4f cameraProjection() const
        {
            return projMat;
        }

        Mat4f cameraModelView() const
        {
            return mvMat;
        }
    };

    PlayerCamera                  playerCamera;
    std::unique_ptr<gloom::World> glWorld;
    Set<const world::Plane *>     planesToUpdate;
    List<gloom::ID>               sectorLut; // sector number => gloom ID
    ImageBank                     images;

    Impl(Public *i) : Base(i)
    {
        binder.init(ScriptSystem::get().nativeModule("Render"))
            << DE_FUNC(Render_SetDebugMode, "setDebugMode", "mode");

        images.add("sky.morning", "/home/sky-morning.jpg");
    }

    void planeMovementBegan(const world::Plane &plane) override
    {
        planesToUpdate.insert(&plane);
    }
};

GloomWorldRenderer::GloomWorldRenderer()
    : d(new Impl(this))
{
    world::World::get().audienceForPlaneMovement() += d;
}

void GloomWorldRenderer::glInit()
{
    d->glWorld.reset(new gloom::World(ClientApp::shaders(), d->images));
    d->glWorld->glInit();
}

void GloomWorldRenderer::glDeinit()
{
    d->glWorld->glDeinit();
    d->glWorld.reset();
}

void GloomWorldRenderer::setCamera()
{}

void GloomWorldRenderer::loadMap(const String &mapId)
{
    d->glWorld->loadMap(mapId);

    // Read the lookup tables.
    {
        const auto & asset = App::asset("map." + mapId);
        const Record lut =
            parseJSON(String::fromUtf8(FS::locate<const File>(asset.absolutePath("lookupPath"))));

        const auto &sectorIds = lut["sectorIds"].array();
        d->sectorLut.resize(sectorIds.size());
        auto i = d->sectorLut.begin();
        for (const auto *value : sectorIds.elements())
        {
            *i++ = value->asUInt();
        }
    }
}

void GloomWorldRenderer::unloadMap()
{
    d->sectorLut.clear();
}

void GloomWorldRenderer::advanceTime(TimeSpan elapsed)
{
    if (d->glWorld)
    {
        // Update changed plane heights.
        if (!d->sectorLut.isEmpty())
        {
            const auto &glMap = d->glWorld->map();
            for (const auto &plane : d->planesToUpdate)
            {
                const gloom::ID sectorId  = d->sectorLut[plane->sector().indexInArchive()];
                const gloom::ID planeId   = plane->isSectorFloor() ? glMap.floorPlaneId(sectorId)
                                                                   : glMap.ceilingPlaneId(sectorId);

                d->glWorld->mapRender().setPlaneY(planeId,
                                                  plane->heightTarget(),
                                                  plane->initialHeightOfMovement(),
                                                  plane->movementBeganAt(),
                                                  plane->speed() * TICSPERSEC);

                // TODO: Pass the target height and speed to glWorld and let the shaders update
                // the heights on the GPU. Only update the plane heights buffer when a move begins.
            }
        }
        d->glWorld->update(elapsed);
        d->glWorld->setCurrentTime(world::World::get().time());
        // TODO: time sync should happen once, and after that `elapsed` should be scaled
        // according to tic length.
        d->planesToUpdate.clear();
    }
}

void GloomWorldRenderer::renderPlayerView(int num)
{
    // Gloom assumes counterclockwise front faces (default for OpenGL).
    glFrontFace(GL_CCW);

    d->playerCamera.update(num, d->glWorld->map().metersPerUnit().toVec3f());
    d->glWorld->render(d->playerCamera);

    // Classic renderer assumes clockwise.
    glFrontFace(GL_CW);
}

gloom::World &GloomWorldRenderer::glWorld()
{
    DE_ASSERT(d->glWorld);
    return *d->glWorld;
}
