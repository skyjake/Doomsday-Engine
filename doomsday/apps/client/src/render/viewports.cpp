/** @file viewports.cpp  Player viewports and related low-level rendering.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "render/viewports.h"

#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>
#include <de/legacy/vector1.h>
#include <de/bitarray.h>
#include <de/glinfo.h>
#include <de/glstate.h>

#include "clientapp.h"
#include "api_console.h"
#include "dd_main.h"
#include "dd_loop.h"
//#include "ui/editors/edit_bias.h"

#include "gl/gl_main.h"

#include "api_render.h"
#include "render/angleclipper.h"
#include "render/cameralensfx.h"
#include "render/classicworldrenderer.h"
#include "render/fx/bloom.h"
#include "render/playerweaponanimator.h"
#include "render/r_draw.h"
#include "render/r_main.h"
#include "render/rendersystem.h"
#include "render/rendpoly.h"
#include "render/skydrawable.h"
#include "render/vissprite.h"
#include "render/vr.h"

#include "network/net_demo.h"

#include "world/p_object.h"
#include "world/p_players.h"
#include "world/convexsubspace.h"
#include "world/surface.h"
#include "world/contact.h"
#include "world/subsector.h"
#include "world/sky.h"

#include "ui/ui_main.h"
#include "ui/clientwindow.h"
//#include "ui/widgets/gameuiwidget.h"

#include <doomsday/world/bspleaf.h>
#include <doomsday/world/linesighttest.h>
#include <doomsday/world/mobjthinker.h>
#include <doomsday/world/thinkers.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/tab_tables.h>

using namespace de;
using world::World;

dd_bool firstFrameAfterLoad;

static dint loadInStartupMode;
static dint rendCameraSmooth = true;  ///< Smoothed by default.
static dbyte showFrameTimePos;
static dbyte showViewAngleDeltas;
static dbyte showViewPosDeltas;

dint rendInfoTris;

static viewport_t *currentViewport;

struct FrameLuminous
{
    coord_t distance;
    duint isClipped;
};
static List<FrameLuminous> frameLuminous;

static BitArray subspacesVisible;

static BitArray generatorsVisible(Map::MAX_GENERATORS);

static dint frameCount;

static dint gridCols, gridRows;
static viewport_t viewportOfLocalPlayer[DDMAXPLAYERS];

static dint resetNextViewer = true;

dint R_FrameCount()
{
    return frameCount;
}

void R_ResetFrameCount()
{
    frameCount = 0;
}

#undef R_SetViewOrigin
DE_EXTERN_C void R_SetViewOrigin(dint consoleNum, coord_t const origin[3])
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    DD_Player(consoleNum)->viewport().latest.origin = Vec3d(origin);
}

#undef R_SetViewAngle
DE_EXTERN_C void R_SetViewAngle(dint consoleNum, angle_t angle)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    DD_Player(consoleNum)->viewport().latest.setAngle(angle);
}

#undef R_SetViewPitch
DE_EXTERN_C void R_SetViewPitch(dint consoleNum, dfloat pitch)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    DD_Player(consoleNum)->viewport().latest.pitch = pitch;
}

void R_SetupDefaultViewWindow(dint consoleNum)
{
    viewdata_t *vd = &DD_Player(consoleNum)->viewport();
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;

    vd->window =
        vd->windowOld =
            vd->windowTarget = Rectanglei::fromSize(Vec2i(0, 0), Vec2ui(DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT));
    vd->windowInter = 1;
}

void R_ViewWindowTicker(dint consoleNum, timespan_t ticLength)
{
    viewdata_t *vd = &DD_Player(consoleNum)->viewport();
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS)
    {
        return;
    }

    vd->windowInter += dfloat(.4 * ticLength * TICRATE);
    if(vd->windowInter >= 1)
    {
        vd->window = vd->windowTarget;
    }
    else
    {
        vd->window.moveTopLeft(
            Vec2i(de::roundf(de::lerp<dfloat>(
                      vd->windowOld.topLeft.x, vd->windowTarget.topLeft.x, vd->windowInter)),
                  de::roundf(de::lerp<dfloat>(
                      vd->windowOld.topLeft.y, vd->windowTarget.topLeft.y, vd->windowInter))));
        vd->window.setSize(
            Vec2ui(de::roundf(de::lerp<dfloat>(
                       vd->windowOld.width(), vd->windowTarget.width(), vd->windowInter)),
                   de::roundf(de::lerp<dfloat>(
                       vd->windowOld.height(), vd->windowTarget.height(), vd->windowInter))));
    }
}

#undef R_ViewWindowGeometry
DE_EXTERN_C dint R_ViewWindowGeometry(dint player, RectRaw *geometry)
{
    if(!geometry) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    const viewdata_t &vd = DD_Player(player)->viewport();
    geometry->origin.x    = vd.window.topLeft.x;
    geometry->origin.y    = vd.window.topLeft.y;
    geometry->size.width  = vd.window.width();
    geometry->size.height = vd.window.height();
    return true;
}

#undef R_ViewWindowOrigin
DE_EXTERN_C dint R_ViewWindowOrigin(dint player, Point2Raw *origin)
{
    if(!origin) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    const viewdata_t &vd = DD_Player(player)->viewport();
    origin->x = vd.window.topLeft.x;
    origin->y = vd.window.topLeft.y;
    return true;
}

#undef R_ViewWindowSize
DE_EXTERN_C dint R_ViewWindowSize(dint player, Size2Raw *size)
{
    if(!size) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    const viewdata_t &vd = DD_Player(player)->viewport();
    size->width  = vd.window.width();
    size->height = vd.window.height();
    return true;
}

/**
 * @note Do not change values used during refresh here because we might be
 * partway through rendering a frame. Changes should take effect on next
 * refresh only.
 */
#undef R_SetViewWindowGeometry
DE_EXTERN_C void R_SetViewWindowGeometry(dint player, const RectRaw *geometry, dd_bool interpolate)
{
    dint p = P_ConsoleToLocal(player);
    if(p < 0) return;

    const viewport_t *vp = &viewportOfLocalPlayer[p];
    viewdata_t *vd = &DD_Player(player)->viewport();

    Rectanglei newGeom = Rectanglei::fromSize(Vec2i(de::clamp<dint>(0, geometry->origin.x, vp->geometry.width()),
                                                       de::clamp<dint>(0, geometry->origin.y, vp->geometry.height())),
                                              Vec2ui(de::abs(geometry->size.width),
                                                        de::abs(geometry->size.height)));

    if((unsigned) newGeom.bottomRight.x > vp->geometry.width())
    {
        newGeom.setWidth(vp->geometry.width() - newGeom.topLeft.x);
    }
    if((unsigned) newGeom.bottomRight.y > vp->geometry.height())
    {
        newGeom.setHeight(vp->geometry.height() - newGeom.topLeft.y);
    }

    // Already at this target?
    if(vd->window == newGeom)
    {
        return;
    }

    // Record the new target.
    vd->windowTarget = newGeom;

    // Restart or advance the interpolation timer?
    // If dimensions have not yet been set - do not interpolate.
    if(interpolate && vd->window.size() != Vec2ui(0, 0))
    {
        vd->windowOld   = vd->window;
        vd->windowInter = 0;
    }
    else
    {
        vd->windowOld   = vd->windowTarget;
        vd->windowInter = 1;  // Update on next frame.
    }
}

#undef R_ViewPortGeometry
DE_EXTERN_C dint R_ViewPortGeometry(dint player, RectRaw *geometry)
{
    if(!geometry) return false;

    dint p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    const viewport_t &vp = viewportOfLocalPlayer[p];
    geometry->origin.x    = vp.geometry.topLeft.x;
    geometry->origin.y    = vp.geometry.topLeft.y;
    geometry->size.width  = vp.geometry.width();
    geometry->size.height = vp.geometry.height();
    return true;
}

#undef R_ViewPortOrigin
DE_EXTERN_C dint R_ViewPortOrigin(dint player, Point2Raw *origin)
{
    if(!origin) return false;

    dint p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    const viewport_t &vp = viewportOfLocalPlayer[p];
    origin->x = vp.geometry.topLeft.x;
    origin->y = vp.geometry.topLeft.y;
    return true;
}

#undef R_ViewPortSize
DE_EXTERN_C dint R_ViewPortSize(dint player, Size2Raw *size)
{
    if(!size) return false;

    dint p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    const viewport_t &vp = viewportOfLocalPlayer[p];
    size->width  = vp.geometry.width();
    size->height = vp.geometry.height();
    return true;
}

#undef R_SetViewPortPlayer
DE_EXTERN_C void R_SetViewPortPlayer(dint consoleNum, dint viewPlayer)
{
    dint p = P_ConsoleToLocal(consoleNum);
    if(p != -1)
    {
        viewportOfLocalPlayer[p].console = viewPlayer;
    }
}

/**
 * Calculate the placement and dimensions of a specific viewport.
 * Assumes that the grid has already been configured.
 */
void R_UpdateViewPortGeometry(viewport_t *port, dint col, dint row)
{
    DE_ASSERT(port);

    Rectanglei newGeom = Rectanglei(Vec2i(DE_GAMEVIEW_X + col * DE_GAMEVIEW_WIDTH  / gridCols,
                                             DE_GAMEVIEW_Y + row * DE_GAMEVIEW_HEIGHT / gridRows),
                                    Vec2i(DE_GAMEVIEW_X + (col+1) * DE_GAMEVIEW_WIDTH  / gridCols,
                                             DE_GAMEVIEW_Y + (row+1) * DE_GAMEVIEW_HEIGHT / gridRows));
    ddhook_viewport_reshape_t p;

    if(port->geometry == newGeom) return;

    bool doReshape = false;
    if(port->console != -1 && Plug_CheckForHook(HOOK_VIEWPORT_RESHAPE))
    {
        p.oldGeometry.origin.x    = port->geometry.topLeft.x;
        p.oldGeometry.origin.y    = port->geometry.topLeft.y;
        p.oldGeometry.size.width  = port->geometry.width();
        p.oldGeometry.size.height = port->geometry.height();
        doReshape = true;
    }

    port->geometry = newGeom;

    if(doReshape)
    {
        p.geometry.origin.x    = port->geometry.topLeft.x;
        p.geometry.origin.y    = port->geometry.topLeft.y;
        p.geometry.size.width  = port->geometry.width();
        p.geometry.size.height = port->geometry.height();

        DoomsdayApp::plugins().callAllHooks(HOOK_VIEWPORT_RESHAPE, port->console, (void *)&p);
    }
}

bool R_SetViewGrid(dint numCols, dint numRows)
{
    if(numCols > 0 && numRows > 0)
    {
        if(numCols * numRows > DDMAXPLAYERS)
        {
            return false;
        }

        if(numCols != gridCols || numRows != gridRows)
        {
            // The number of consoles has changes; LensFx needs to reallocate resources
            // only for the consoles in use.
            /// @todo This could be done smarter, only for the affected viewports. -jk
            LensFx_GLRelease();
        }

        if(numCols > DDMAXPLAYERS)
            numCols = DDMAXPLAYERS;
        if(numRows > DDMAXPLAYERS)
            numRows = DDMAXPLAYERS;

        gridCols = numCols;
        gridRows = numRows;
    }

    dint p = 0;
    for(dint y = 0; y < gridRows; ++y)
    for(dint x = 0; x < gridCols; ++x)
    {
        // The console number is -1 if the viewport belongs to no one.
        viewport_t *vp = &viewportOfLocalPlayer[p];

        const dint console = P_LocalToConsole(p);
        if(console != -1)
        {
            vp->console = DD_Player(console)->viewConsole;
        }
        else
        {
            vp->console = -1;
        }

        R_UpdateViewPortGeometry(vp, x, y);
        ++p;
    }

    return true;
}

void R_ResetViewer()
{
    resetNextViewer = 1;
}

dint R_IsViewerResetPending()
{
    return resetNextViewer;
}

/**
 * The components whose difference is too large for interpolation will be
 * snapped to the sharp values.
 */
void R_CheckViewerLimits(viewer_t *src, viewer_t *dst)
{
    const dint MAXMOVE = 32;

    /// @todo Remove this snapping. The game should determine this and disable the
    ///       the interpolation as required.
    if(fabs(dst->origin.x - src->origin.x) > MAXMOVE ||
       fabs(dst->origin.y - src->origin.y) > MAXMOVE)
    {
        src->origin = dst->origin;
    }

    /*
    if(abs(dint(dst->angle) - dint(src->angle)) >= ANGLE_45)
    {
        LOG_DEBUG("R_CheckViewerLimits: Snap camera angle to %08x.") << dst->angle;
        src->angle = dst->angle;
    }
    */
}

/**
 * Retrieve the current sharp camera position.
 */
viewer_t R_SharpViewer(ClientPlayer &player)
{
    DE_ASSERT(player.publicData().mo);

    const ddplayer_t &ddpl = player.publicData();

    viewer_t view(player.viewport().latest);

    if((ddpl.flags & DDPF_CHASECAM) && !(ddpl.flags & DDPF_CAMERA))
    {
        // STUB
        // This needs to be fleshed out with a proper third person
        // camera control setup. Currently we simply project the viewer's
        // position a set distance behind the ddpl.
        const dfloat distance = 90;

        duint angle = view.angle() >> ANGLETOFINESHIFT;
        duint pitch = angle_t(LOOKDIR2DEG(view.pitch) / 360 * ANGLE_MAX) >> ANGLETOFINESHIFT;

        view.origin -= Vec3d(FIX2FLT(finecosine[angle]),
                                FIX2FLT(finesine[angle]),
                                FIX2FLT(finesine[pitch])) * distance;
    }

    // Check that the viewZ doesn't go too high or low.
    // Cameras are not restricted.
    if(!(ddpl.flags & DDPF_CAMERA))
    {
        if(view.origin.z > ddpl.mo->ceilingZ - 4)
        {
            view.origin.z = ddpl.mo->ceilingZ - 4;
        }

        if(view.origin.z < ddpl.mo->floorZ + 4)
        {
            view.origin.z = ddpl.mo->floorZ + 4;
        }
    }

    return view;
}

void R_NewSharpWorld()
{
    if(resetNextViewer)
    {
        resetNextViewer = 2;
    }

    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr  = DD_Player(i);
        viewdata_t *vd = &plr->viewport();

        if(!plr->isInGame())
        {
            continue;
        }

        viewer_t sharpView = R_SharpViewer(*plr);

        // The game tic has changed, which means we have an updated sharp
        // camera position.  However, the position is at the beginning of
        // the tic and we are most likely not at a sharp tic boundary, in
        // time.  We will move the viewer positions one step back in the
        // buffer.  The effect of this is that [0] is the previous sharp
        // position and [1] is the current one.

        vd->lastSharp[0] = vd->lastSharp[1];
        vd->lastSharp[1] = sharpView;

        R_CheckViewerLimits(vd->lastSharp, &sharpView);
    }

    if (auto *map = maybeAs<Map>(ClientApp::world().mapPtr()))
    {
        map->updateTrackedPlanes();
        map->updateScrollingSurfaces();
    }
}

void R_UpdateViewer(dint consoleNum)
{
    DE_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);

    const dint VIEWPOS_MAX_SMOOTHDISTANCE = 172;

    player_t *player = DD_Player(consoleNum);
    viewdata_t *vd   = &player->viewport();

    if(!player->isInGame()) return;

    viewer_t sharpView = R_SharpViewer(*player);

    if(resetNextViewer ||
       (sharpView.origin - vd->current.origin).length() > VIEWPOS_MAX_SMOOTHDISTANCE)
    {
        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
        {
            resetNextViewer = 0;
        }

        // Just view from the sharp position.
        vd->current = sharpView;

        vd->lastSharp[0] = vd->lastSharp[1] = sharpView;
    }
    // While the game is paused there is no need to calculate any
    // time offsets or interpolated camera positions.
    else  //if(!clientPaused)
    {
        // Calculate the smoothed camera position, which is somewhere between
        // the previous and current sharp positions. This introduces a slight
        // delay (max. 1/35 sec) to the movement of the smoothed camera.
        viewer_t smoothView = vd->lastSharp[0].lerp(vd->lastSharp[1], frameTimePos);

        // Use the latest view angles known to us if the interpolation flags
        // are not set. The interpolation flags are used when the view angles
        // are updated during the sharp tics and need to be smoothed out here.
        // For example, view locking (dead or camera setlock).
        /*if(!(player->shared.flags & DDPF_INTERYAW))
            smoothView.angle = sharpView.angle;*/
        /*if(!(player->shared.flags & DDPF_INTERPITCH))
            smoothView.pitch = sharpView.pitch;*/

        vd->current = smoothView;

        // Monitor smoothness of yaw/pitch changes.
        if(showViewAngleDeltas)
        {
            struct OldAngle {
                ddouble time;
                dfloat yaw;
                dfloat pitch;
            };

            static OldAngle oldAngle[DDMAXPLAYERS];
            OldAngle *old = &oldAngle[DoomsdayApp::players().indexOf(viewPlayer)];
            dfloat yaw    = (ddouble)smoothView.angle() / ANGLE_MAX * 360;

            LOGDEV_MSG("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f "
                       "Rdx=%-10.3f Rdy=%-10.3f")
                    << SECONDS_TO_TICKS(gameTime)
                    << frameTimePos
                    << sysTime - old->time
                    << yaw - old->yaw
                    << smoothView.pitch - old->pitch
                    << (yaw - old->yaw) / (sysTime - old->time)
                    << (smoothView.pitch - old->pitch) / (sysTime - old->time);

            old->yaw   = yaw;
            old->pitch = smoothView.pitch;
            old->time  = sysTime;
        }

        // The Rdx and Rdy should stay constant when moving.
        if(showViewPosDeltas)
        {
            struct OldPos {
                ddouble time;
                Vec3f pos;
            };

            static OldPos oldPos[DDMAXPLAYERS];
            OldPos *old = &oldPos[DoomsdayApp::players().indexOf(viewPlayer)];

            LOGDEV_MSG("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f dz=%-10.3f dx/dt=%-10.3f dy/dt=%-10.3f")
                    << SECONDS_TO_TICKS(gameTime)
                    << frameTimePos
                    << sysTime - old->time
                    << smoothView.origin.x - old->pos.x
                    << smoothView.origin.y - old->pos.y
                    << smoothView.origin.z - old->pos.z
                    << (smoothView.origin.x - old->pos.x) / (sysTime - old->time)
                    << (smoothView.origin.y - old->pos.y) / (sysTime - old->time);

            old->pos  = smoothView.origin;
            old->time = sysTime;
        }
    }

    // Update viewer.
    const angle_t viewYaw = vd->current.angle();

    const duint an = viewYaw >> ANGLETOFINESHIFT;
    vd->viewSin = FIX2FLT(finesine[an]);
    vd->viewCos = FIX2FLT(finecosine[an]);

    // Calculate the front, up and side unit vectors.
    const dfloat yawRad   = ((viewYaw / (dfloat) ANGLE_MAX) *2) * PI;
    const dfloat pitchRad = vd->current.pitch * 85 / 110.f / 180 * PI;

    // The front vector.
    vd->frontVec.x = cos(yawRad) * cos(pitchRad);
    vd->frontVec.z = sin(yawRad) * cos(pitchRad);
    vd->frontVec.y = sin(pitchRad);

    // The up vector.
    vd->upVec.x = -cos(yawRad) * sin(pitchRad);
    vd->upVec.z = -sin(yawRad) * sin(pitchRad);
    vd->upVec.y = cos(pitchRad);

    // The side vector is the cross product of the front and up vectors.
    vd->sideVec = vd->frontVec.cross(vd->upVec);
}

/**
 * Prepare rendering the view of the given player.
 */
void R_SetupFrame(player_t *player)
{
#define MINEXTRALIGHTFRAMES         2

    // This is now the current view player.
    viewPlayer = player;

    // Reset the GL triangle counter.
    //polyCounter = 0;

    if(showFrameTimePos)
    {
        LOGDEV_VERBOSE("frametime = %f") << frameTimePos;
    }

    // Handle extralight (used to light up the world momentarily (used for
    // e.g. gun flashes). We want to avoid flickering, so when ever it is
    // enabled; make it last for a few frames.
    if(player->targetExtraLight != player->publicData().extraLight)
    {
        player->targetExtraLight = player->publicData().extraLight;
        player->extraLightCounter = MINEXTRALIGHTFRAMES;
    }

    if(player->extraLightCounter > 0)
    {
        player->extraLightCounter--;
        if(player->extraLightCounter == 0)
            player->extraLight = player->targetExtraLight;
    }

    World::validCount++;

    extraLight      = player->extraLight;
    extraLightDelta = extraLight / 16.0f;

    if(!freezeRLs)
    {
        R_ClearVisSprites();
    }

#undef MINEXTRALIGHTFRAMES
}

void R_RenderPlayerViewBorder()
{
    R_DrawViewBorder();
}

void R_UseViewPort(const viewport_t *vp)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    if (!vp)
    {
        currentViewport = nullptr;
        /*ClientWindow::main().game().glApplyViewport(
                Rectanglei::fromSize(Vec2i(DE_GAMEVIEW_X, DE_GAMEVIEW_Y),
                                     Vec2ui(DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT)));*/
    }
    else
    {
        currentViewport = const_cast<viewport_t *>(vp);
        //ClientWindow::main().game().glApplyViewport(vp->geometry);
    }
}

void R_UseViewPort(int consoleNum)
{
    int local = P_ConsoleToLocal(consoleNum);
    if (local >= 0)
    {
        R_UseViewPort(&viewportOfLocalPlayer[local]);
    }
    else
    {
        R_UseViewPort(nullptr);
    }
}

Rectanglei R_ConsoleRect(int console)
{
    int local = P_ConsoleToLocal(console);
    if (local < 0) return Rectanglei();

    const auto &port = viewportOfLocalPlayer[local];

    return Rectanglei(port.geometry.topLeft.x,
                      port.geometry.topLeft.y,
                      port.geometry.width(),
                      port.geometry.height());
}

Rectanglei R_Console3DViewRect(int console)
{
    Rectanglei rect = R_ConsoleRect(console);

    const auto &pv = DD_Player(console)->viewport();
    return Rectanglei(rect.left() + pv.window.topLeft.x,
                      rect.top()  + pv.window.topLeft.y,
                      de::min(rect.width(),  pv.window.width()),
                      de::min(rect.height(), pv.window.height()));
}

const viewport_t *R_CurrentViewPort()
{
    return currentViewport;
}

void R_RenderBlankView()
{
    UI_DrawDDBackground(Point2Raw{0, 0}, Size2Raw{{{320, 200}}}, 1);
}

static void setupPlayerSprites()
{
    DE_ASSERT(viewPlayer);

    // There are no 3D psprites.
    ::psp3d = false;

    ddplayer_t *ddpl = &viewPlayer->publicData();

    // Cameramen have no psprites.
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    if(!ddpl->mo) return;
    mobj_t *mob = ddpl->mo;

    if(!Mobj_HasSubsector(*mob)) return;
    auto &subsec = Mobj_Subsector(*mob);

    // Determine if we should be drawing all the psprites full bright?
    bool fullBright = CPP_BOOL(::levelFullBright);
    if(!fullBright)
    {
        for(const ddpsprite_t &psp : ddpl->pSprites)
        {
            if(!psp.statePtr) continue;

            // If one of the psprites is fullbright, both are.
            if(psp.statePtr->flags & STF_FULLBRIGHT)
            {
                fullBright = true;
            }
        }
    }

    const viewdata_t *viewData = &viewPlayer->viewport();

    for(dint i = 0; i < DDMAXPSPRITES; ++i)
    {
        vispsprite_t *spr = &visPSprites[i];

        spr->type    = VPSPR_SPRITE;
        spr->psp     = &ddpl->pSprites[i];
        spr->origin  = viewData->current.origin;
        spr->bspLeaf = &Mobj_BspLeafAtOrigin(*mob);
        spr->alpha   = spr->psp->alpha;

        if(!spr->psp->statePtr) continue;

        spr->light.isFullBright = (spr->psp->flags & DDPSPF_FULLBRIGHT) != 0;

        // First, determine whether this is a model or a sprite.
        FrameModelDef *mf = nullptr, *nextmf = nullptr;
        dfloat inter = 0;
        if(useModels)
        {
            if(viewPlayer->playerWeaponAnimator().hasModel())
            {
                viewPlayer->playerWeaponAnimator().setupVisPSprite(*spr);

                // There are 3D psprites.
                ::psp3d = true;
                continue;
            }
            else
            {
                // Is there a model for this frame?
                MobjThinker dummy;

                // Setup a dummy for the call to R_CheckModelFor.
                dummy->state = spr->psp->statePtr;
                dummy->tics  = spr->psp->tics;

                mf = Mobj_ModelDef(dummy, &nextmf, &inter);
            }
        }

        // Use a 3D model?
        if(mf)
        {
            // There are 3D psprites.
            ::psp3d = true;

            spr->type = VPSPR_MODEL;

            spr->data.model.flags       = 0;
            // 32 is the raised weapon height.
            spr->data.model.topZ        = viewData->current.origin.z;
            if (auto *vsub = maybeAs<Subsector>(subsec))
            {
                spr->data.model.secFloor = vsub->visFloor().heightSmoothed();
                spr->data.model.secCeil  = vsub->visCeiling().heightSmoothed();
            }
            else
            {
                spr->data.model.secFloor = subsec.sector().floor().height();
                spr->data.model.secCeil  = subsec.sector().ceiling().height();
            }
            spr->data.model.pClass      = 0;
            spr->data.model.floorClip   = 0;

            spr->data.model.mf          = mf;
            spr->data.model.nextMF      = nextmf;
            spr->data.model.inter       = inter;
            spr->data.model.viewAligned = true;

            // Offsets to rotation angles.
            spr->data.model.yawAngleOffset   = spr->psp->pos[0] * weaponOffsetScale - 90;
            spr->data.model.pitchAngleOffset =
                (32 - spr->psp->pos[1]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
            // Is the FOV shift in effect?
            if (weaponFOVShift > 0 && weaponFixedFOV > 90)
            {
                spr->data.model.pitchAngleOffset -= weaponFOVShift * (weaponFixedFOV - 90) / 90;
            }
            // Real rotation angles.
            spr->data.model.yaw =
                viewData->current.angle() / (dfloat) ANGLE_MAX *-360 + spr->data.model.yawAngleOffset + 90;
            spr->data.model.pitch = viewData->current.pitch * 85 / 110 + spr->data.model.yawAngleOffset;
            std::memset(spr->data.model.visOff, 0, sizeof(spr->data.model.visOff));
        }
        else
        {
            // No, draw a 2D sprite (in Rend_DrawPlayerSprites).
            spr->type = VPSPR_SPRITE;
        }
    }
}

static Mat4f frameViewerMatrix;

static void setupViewMatrix()
{
    auto &rend = ClientApp::render();

    // These will be the matrices for the current frame.
    rend.uProjectionMatrix() = Rend_GetProjectionMatrix();
    rend.uViewMatrix()       = Rend_GetModelViewMatrix(DoomsdayApp::players().indexOf(viewPlayer));

    frameViewerMatrix        = rend.uProjectionMatrix().toMat4f() * rend.uViewMatrix().toMat4f();
}

const Mat4f &Viewer_Matrix()
{
    return frameViewerMatrix;
}

enum ViewState { Default2D, PlayerView3D, PlayerSprite2D };

static void changeViewState(ViewState viewState) //, const viewport_t *port, const viewdata_t *viewData)
{
    //DE_ASSERT(port && viewData);

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    switch (viewState)
    {
    case PlayerView3D:
        DGL_CullFace(DGL_BACK);
        DGL_Enable(DGL_DEPTH_TEST);
        GL_ProjectionMatrix(); // The 3D projection matrix.
        break;

    case PlayerSprite2D:
    {
        const auto conRect  = R_ConsoleRect(displayPlayer);
        const auto viewRect = R_Console3DViewRect(displayPlayer);

        const dint height = dint(SCREENHEIGHT
                * ( float(conRect.width()) * float(viewRect.height())
                                           / float(viewRect.width()) )
                / float(conRect.height()));

        scalemode_t sm = R_ChooseScaleMode(SCREENWIDTH, SCREENHEIGHT,
                                           conRect.width(), conRect.height(),
                                           scalemode_t(weaponScaleMode));

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_LoadIdentity();

        if(sm == SCALEMODE_STRETCH)
        {
            DGL_Ortho(0, 0, SCREENWIDTH, height, -1, 1);
        }
        else
        {
            // Use an orthographic projection in native screenspace. Then
            // translate and scale the projection to produce an aspect
            // corrected coordinate space at 4:3, aligned vertically to
            // the bottom and centered horizontally in the window.
            DGL_Ortho(0, 0, conRect.width(), conRect.height(), -1, 1);
            DGL_Translatef(conRect.width()/2, conRect.height(), 0);

            if(conRect.width() >= conRect.height())
            {
                DGL_Scalef(dfloat( conRect.height() ) / SCREENHEIGHT,
                                   dfloat( conRect.height() ) / SCREENHEIGHT, 1);
            }
            else
            {
                DGL_Scalef(dfloat( conRect.width() ) / SCREENWIDTH,
                                   dfloat( conRect.width() ) / SCREENWIDTH, 1);
            }

            // Special case: viewport height is greater than width.
            // Apply an additional scaling factor to prevent player sprites
            // looking too small.
            if(conRect.height() > conRect.width())
            {
                dfloat extraScale = (dfloat(conRect.height() * 2) / conRect.width()) / 2;
                DGL_Scalef(extraScale, extraScale, 1);
            }

            DGL_Translatef(-(SCREENWIDTH / 2), -SCREENHEIGHT, 0);
            DGL_Scalef(1, dfloat( SCREENHEIGHT ) / height, 1);
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_LoadIdentity();

        // Depth testing must be disabled so that psprite 1 will be drawn
        // on top of psprite 0 (Doom plasma rifle fire).
        DGL_Disable(DGL_DEPTH_TEST);

        break;
    }

    case Default2D:
        DGL_CullFace(DGL_NONE);
        DGL_Disable(DGL_DEPTH_TEST);
        break;
    }


    //std::memcpy(&currentView, port, sizeof(currentView));

    //viewpx = port->geometry.topLeft.x + viewData->window.topLeft.x;
    //viewpy = port->geometry.topLeft.y + viewData->window.topLeft.y;

    /*const auto viewRect = R_Console3DViewRect(displayPlayer);
    viewpx = 0;
    viewpy = 0;
    viewpw = int(viewRect.width());
    viewph = int(viewRect.height());*/

    //viewpw = de::min(port->geometry.width(), viewData->window.width());
    //viewph = de::min(port->geometry.height(), viewData->window.height());

    /*ClientWindow::main().game().glApplyViewport(Rectanglei::fromSize(Vec2i(viewpx, viewpy),
                                                                     Vec2ui(viewpw, viewph)));*/

}

void ClassicWorldRenderer::glInit()
{

}

void ClassicWorldRenderer::glDeinit()
{

}

void ClassicWorldRenderer::setCamera()
{

}

void ClassicWorldRenderer::advanceTime(TimeSpan /*elapsed*/)
{

}

void ClassicWorldRenderer::renderPlayerView(int num)
{
    player_t *player = DD_Player(num);

    // Setup for rendering the frame.
    R_SetupFrame(player);

    vrCfg().setEyeHeightInMapUnits(Con_GetInteger("player-eyeheight"));

    setupViewMatrix();
    setupPlayerSprites();

    if (ClientApp::vr().mode() == VRConfig::OculusRift &&
        World::get().map().as<Map>().isPointInVoid(Rend_EyeOrigin().xzy()))
    {
        // Putting one's head in the wall will cause a blank screen.
        GLState::current().target().clear(GLFramebuffer::Color0);
        return;
    }

    // Hide the viewPlayer's mobj?
    dint oldFlags = 0;
    if (!(player->publicData().flags & DDPF_CHASECAM))
    {
        oldFlags = player->publicData().mo->ddFlags;
        player->publicData().mo->ddFlags |= DDMF_DONTDRAW;
    }

    // Go to wireframe mode?
#if defined (DE_OPENGL)
    if (renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
#endif

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    // GL is in 3D transformation state only during the frame.
    //switchTo3DState(true); //, currentViewport, vd);
    changeViewState(PlayerView3D);

    Rend_RenderMap(World::get().map().as<Map>());

    // Orthogonal projection to the view window.
    //restore2DState(1); //, currentViewport, vd);
    changeViewState(PlayerSprite2D);

    // Don't render in wireframe mode with 2D psprites.
#if defined (DE_OPENGL)
    if (renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    Rend_Draw2DPlayerSprites();  // If the 2D versions are needed.

#if defined (DE_OPENGL)
    if (renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
#endif

    // Do we need to render any 3D psprites?
    if (psp3d)
    {
        //switchTo3DState(false); //, currentViewport, vd);
        changeViewState(PlayerView3D);
        Rend_Draw3DPlayerSprites();
    }

    // Restore fullscreen viewport, original matrices and state: back to normal 2D.
    //restore2DState(2); //, currentViewport, vd);
    changeViewState(Default2D);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_Flush();

#if defined (DE_OPENGL)
    // Back from wireframe mode?
    if (renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    // Now we can show the viewPlayer's mobj again.
    if (!(player->publicData().flags & DDPF_CHASECAM))
    {
        player->publicData().mo->ddFlags = oldFlags;
    }

    R_PrintRendPoolInfo();
}

#undef R_RenderPlayerView
DE_EXTERN_C void R_RenderPlayerView(dint num)
{
    if (num < 0 || num >= DDMAXPLAYERS) return; // Huh?
    player_t *player = DD_Player(num);

    if (!player->publicData().inGame) return;
    if (!player->publicData().mo) return;
    if (!ClientApp::world().hasMap()) return;

    if (firstFrameAfterLoad)
    {
        // Don't let the clock run yet.  There may be some texture
        // loading still left to do that we have been unable to
        // predetermine.
        firstFrameAfterLoad = false;
        DD_ResetTimer();
    }

    // Too early? Game has not configured the view window?
    viewdata_t *vd = &player->viewport();
    if (vd->window.isNull()) return;

    ClientApp::render().world().renderPlayerView(num);
}

/**
 * Should be called when returning from a game-side drawing method to ensure
 * that our assumptions of the GL state are valid. This is necessary because
 * DGL affords the user the posibility of modifiying the GL state.
 *
 * @todo: A cleaner approach would be a DGL state stack which could simply pop.
 */
static void restoreDefaultGLState()
{
    // Here we use the DGL methods as this ensures it's state is kept in sync.
    DGL_Disable(DGL_FOG);
    DGL_Disable(DGL_SCISSOR_TEST);
    DGL_Disable(DGL_TEXTURE_2D);
    DGL_Enable(DGL_LINE_SMOOTH);
    DGL_Enable(DGL_POINT_SMOOTH);
}

void R_RenderViewPort(int playerNum)
{
    int localNum = P_ConsoleToLocal(playerNum);
    if (localNum < 0) return;

    const viewport_t *vp = &viewportOfLocalPlayer[localNum];

    DE_ASSERT(vp->console == playerNum);

    const dint oldDisplay = displayPlayer;
    displayPlayer = vp->console;
    //R_UseViewPort(vp);
    //currentViewport = vp;

    if(displayPlayer < 0 || (DD_Player(displayPlayer)->publicData().flags & DDPF_UNDEFINED_ORIGIN))
    {
        R_RenderBlankView();
        displayPlayer = oldDisplay;
        return;
    }

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();

    Rectanglei viewRect = R_Console3DViewRect(playerNum);

    // Use an orthographic projection in real pixel dimensions.
    DGL_Ortho(0, 0, viewRect.width(), viewRect.height(), -1, 1);

    const viewdata_t *vd = &DD_Player(vp->console)->viewport();
    RectRaw vpGeometry = {{vp->geometry.topLeft.x, vp->geometry.topLeft.y},
                          {int(vp->geometry.width()), int(vp->geometry.height())}};

    RectRaw vdWindow = {{vd->window.topLeft.x, vd->window.topLeft.y},
                        {int(vd->window.width()), int(vd->window.height())}};

    R_UpdateViewer(vp->console);

    gx.DrawViewPort(localNum, &vpGeometry, &vdWindow, displayPlayer, /* layer: */ 0);

    // Apply camera lens effects on the rendered view.
    LensFx_Draw(vp->console);

    restoreDefaultGLState();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    // Increment the internal frame count. This does not
    // affect the window's FPS counter.
    frameCount++;

    // Keep reseting until a new sharp world has arrived.
    if(resetNextViewer > 1) resetNextViewer = 0;

    // Restore things back to normal.
    displayPlayer = oldDisplay;
}

void R_ClearViewData()
{
    frameLuminous.clear();
}

/**
 * Viewer specific override controlling whether a given sky layer is enabled.
 *
 * @todo The override should be applied at SkyDrawable level. We have Raven to
 * thank for this nonsense (Hexen's sector special 200)... -ds
 */
#undef R_SkyParams
DE_EXTERN_C void R_SkyParams(dint layerIndex, dint param, void * /*data*/)
{
    LOG_AS("R_SkyParams");
    if (!ClientApp::world().hasMap())
    {
        LOG_GL_WARNING("No map currently loaded, ignoring");
        return;
    }
    auto &sky = World::get().map().sky();
    if (layerIndex >= 0 && layerIndex < sky.layerCount())
    {
        auto &layer = sky.layer(layerIndex);
        switch (param)
        {
        case DD_ENABLE:  layer.enable();  break;
        case DD_DISABLE: layer.disable(); break;

        default: // Log but otherwise ignore this error.
            LOG_GL_WARNING("Failed configuring layer #%i: bad parameter %i")
                << layerIndex << param;
        }
        return;
    }
    LOG_GL_WARNING("Invalid layer #%i") << + layerIndex;
}

bool R_ViewerSubspaceIsVisible(const world::ConvexSubspace &subspace)
{
    DE_ASSERT(subspace.indexInMap() != world::MapElement::NoIndex);
    return subspacesVisible.testBit(subspace.indexInMap());
}

void R_ViewerSubspaceMarkVisible(const world::ConvexSubspace &subspace, bool yes)
{
    DE_ASSERT(subspace.indexInMap() != world::MapElement::NoIndex);
    subspacesVisible.setBit(subspace.indexInMap(), yes);
}

bool R_ViewerGeneratorIsVisible(const Generator &generator)
{
    return generatorsVisible.testBit(generator.id() - 1 /* id is 1-based index */);
}

void R_ViewerGeneratorMarkVisible(const Generator &generator, bool yes)
{
    generatorsVisible.setBit(generator.id() - 1 /* id is 1-based index */, yes);
}

ddouble R_ViewerLumobjDistance(dint idx)
{
    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < World::get().map().as<Map>().lumobjCount())
    {
        return frameLuminous.at(idx).distance;
    }
    return 0;
}

bool R_ViewerLumobjIsClipped(dint idx)
{
    if (idx >= 0 && idx < frameLuminous.sizei())
    {
        return CPP_BOOL(frameLuminous.at(idx).isClipped);
    }
    return false;
}

bool R_ViewerLumobjIsHidden(dint idx)
{
    if (idx >= 0 && idx < frameLuminous.sizei())
    {
        return frameLuminous.at(idx).isClipped == 2;
    }
    return false;
}

static void markLumobjClipped(const Lumobj &lob, bool yes = true)
{
    const dint index = lob.indexInMap();
    DE_ASSERT(index >= 0 && index < lob.map().as<Map>().lumobjCount());
    DE_ASSERT(index < frameLuminous.sizei());
    frameLuminous[index].isClipped = yes? 1 : 0;
}

void R_BeginFrame()
{
    static List<int> frameLuminousOrder;

    auto &map = World::get().map().as<Map>();

    subspacesVisible.resize(map.subspaceCount());
    subspacesVisible.fill(false);

    // Clear all generator visibility flags.
    generatorsVisible.fill(false);

    int numLuminous = map.lumobjCount();
    if (!numLuminous) return;

    // Resize the associated buffers used for per-frame stuff.
    //int maxLuminous = numLuminous;
    if (frameLuminous.sizei() < numLuminous)
    {
        frameLuminous.resize(numLuminous);
        frameLuminousOrder.resize(numLuminous);
    }

    // Update viewer => lumobj distances ready for linking and sorting.
    const viewdata_t *viewData = &viewPlayer->viewport();
    map.forAllLumobjs([&viewData] (Lumobj &lob)
    {
        // Approximate the distance in 3D.
        Vec3d delta = lob.origin() - viewData->current.origin;
        frameLuminous[lob.indexInMap()].distance = M_ApproxDistance3(delta.x, delta.y, delta.z * 1.2 /*correct aspect*/);
        return LoopContinue;
    });

    if (rendMaxLumobjs > 0 && numLuminous > rendMaxLumobjs)
    {
        // Sort lumobjs by distance from the viewer. Then clip all lumobjs
        // so that only the closest are visible (max loMaxLumobjs).

        // Init the lumobj indices, sort array.
        // Mark all as hidden.
        for (int i = 0; i < numLuminous; ++i)
        {
            frameLuminousOrder[i] = i;
            frameLuminous[i].isClipped = 2;
        }
        std::sort(
            frameLuminousOrder.begin(), frameLuminousOrder.begin() + numLuminous, [](int a, int b) {
            return frameLuminous[a].distance < frameLuminous[b].distance;
        });

        for (int i = 0, n = 0; i < numLuminous; ++i)
        {
            if (n++ > rendMaxLumobjs)
                break;

            // Unhide this lumobj.
            frameLuminous[frameLuminousOrder[i]].isClipped = 1;
        }
    }
    else
    {
        // Mark all as clipped.
        for (auto &lum : frameLuminous)
        {
            lum.isClipped = 1;
        }
    }
}

void R_ViewerClipLumobj(Lumobj *lum)
{
    if (!lum) return;

    // Has this already been occluded?
    dint lumIdx = lum->indexInMap();
    if (frameLuminous.at(lumIdx).isClipped > 1) return;

    markLumobjClipped(*lum, false);

    /// @todo Determine the exact centerpoint of the light in addLuminous!
    Vec3d const origin(lum->x(), lum->y(), lum->z() + lum->zOffset());

    if (!P_IsInVoid(DD_Player(displayPlayer)) && !devNoCulling)
    {
        if (!ClientApp::render().angleClipper().isPointVisible(origin))
        {
            markLumobjClipped(*lum); // Won't have a halo.
        }
    }
    else
    {
        markLumobjClipped(*lum);

        const Vec3d eye = Rend_EyeOrigin().xzy();
        if (world::LineSightTest(eye, origin, -1, 1, LS_PASSLEFT | LS_PASSOVER | LS_PASSUNDER)
                .trace(lum->map().bspTree()))
        {
            markLumobjClipped(*lum, false); // Will have a halo.
        }
    }
}

void R_ViewerClipLumobjBySight(Lumobj *lob, world::ConvexSubspace *subspace)
{
    if(!lob || !subspace) return;

    // Already clipped?
    if (frameLuminous.at(lob->indexInMap()).isClipped)
        return;

    // We need to figure out if any of the polyobj's segments lies
    // between the viewpoint and the lumobj.
    const Vec3d eye = Rend_EyeOrigin().xzy();

    subspace->forAllPolyobjs([&lob, &eye] (Polyobj &pob)
    {
        for(auto *hedge : pob.mesh().hedges())
        {
            // Is this on the back of a one-sided line?
            if(!hedge->hasMapElement())
                continue;

            // Ignore half-edges facing the wrong way.
            if(hedge->mapElementAs<LineSideSegment>().isFrontFacing())
            {
                coord_t eyeV1[2]       = { eye.x, eye.y };
                coord_t lumOriginV1[2] = { lob->origin().x, lob->origin().y };
                coord_t fromV1[2]      = { hedge->origin().x, hedge->origin().y };
                coord_t toV1[2]        = { hedge->twin().origin().x, hedge->twin().origin().y };
                if(V2d_Intercept2(lumOriginV1, eyeV1, fromV1, toV1, 0, 0, 0))
                {
                    markLumobjClipped(*lob);
                    break;
                }
            }
        }
        return LoopContinue;
    });
}

angle_t viewer_t::angle() const
{
    angle_t a = _angle;
    if(DD_GetInteger(DD_USING_HEAD_TRACKING))
    {
        // Apply the actual, current yaw offset. The game has omitted the "body yaw"
        // portion from the value already.
        a += fixed_t(radianToDegree(vrCfg().oculusRift().headOrientation().z) / 180 * ANGLE_180);
    }
    return a;
}

D_CMD(ViewGrid)
{
    DE_UNUSED(src, argc);
    // Recalculate viewports.
    return R_SetViewGrid(String(argv[1]).toInt(), String(argv[2]).toInt());
}

void Viewports_Register()
{
    C_VAR_INT ("con-show-during-setup",     &loadInStartupMode,     0, 0, 1);

    C_VAR_INT ("rend-camera-smooth",        &rendCameraSmooth,      CVF_HIDE, 0, 1);

    C_VAR_BYTE("rend-info-deltas-angles",   &showViewAngleDeltas,   0, 0, 1);
    C_VAR_BYTE("rend-info-deltas-pos",      &showViewPosDeltas,     0, 0, 1);
    C_VAR_BYTE("rend-info-frametime",       &showFrameTimePos,      0, 0, 1);
    C_VAR_BYTE("rend-info-rendpolys",       &rendInfoRPolys,        CVF_NO_ARCHIVE, 0, 1);
    //C_VAR_INT ("rend-info-tris",            &rendInfoTris,          0, 0, 1); // not implemented atm

    C_CMD("viewgrid", "ii", ViewGrid);
}
