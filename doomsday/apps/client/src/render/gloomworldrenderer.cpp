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

#include <gloom/world.h>
#include <gloom/world/map.h>
#include <gloom/render/icamera.h>
#include <de/ImageBank>

using namespace de;

DE_PIMPL(GloomWorldRenderer)
{
    /**
     * Gloom camera for rendering a specific player's view.
     */
    struct PlayerCamera : public gloom::ICamera
    {
        Vec3f pos, up, front;
        Mat4f mvMat, projMat;

        void update(int console, const Vec3f &metersPerUnit)
        {
            player_t *player = DD_Player(console);
            viewdata_t *vd   = &player->viewport();

            pos     = vd->current.origin.xzy() * metersPerUnit;
            up      = vd->upVec;
            front   = vd->frontVec;
            mvMat   = Mat4f::scale(metersPerUnit) * Rend_GetModelViewMatrix(console) *
                      Mat4f::scale(Vec3f(1.0f) / metersPerUnit) *
                      Mat4f::scale(Vec3f(1, 1, -1));
            projMat = Rend_GetProjectionMatrix(0.0f, metersPerUnit.x /* clip planes in meters */);
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
    ImageBank                     images;

    Impl(Public *i) : Base(i)
    {
        images.add("sky.morning", "/home/sky-morning.jpg");
    }
};

GloomWorldRenderer::GloomWorldRenderer()
    : d(new Impl(this))
{}

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
}

void GloomWorldRenderer::advanceTime(TimeSpan elapsed)
{
    if (d->glWorld)
    {
        d->glWorld->update(elapsed);
    }
}

void GloomWorldRenderer::renderPlayerView(int num)
{
    // Gloom assumes counterclockwise front faces.
    glFrontFace(GL_CCW);

    d->playerCamera.update(num, d->glWorld->map().metersPerUnit().toVec3f());
    d->glWorld->render(d->playerCamera);

    // Classic renderer assumes clockwise.
    glFrontFace(GL_CW);
}
