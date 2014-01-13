/** @file viewports.cpp  Player viewports and related low-level rendering.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_resource.h"
#include "de_ui.h"
#include "de_misc.h"

#include "clientapp.h"
#include "edit_bias.h"
#include "api_render.h"
#include "render/vr.h"

#include "network/net_demo.h"
#include "filesys/fs_util.h"

#include "world/linesighttest.h"
#include "world/thinkers.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "Contact"
#include "BspLeaf"
#include "Surface"

#include <de/GLState>
#include <QBitArray>

using namespace de;

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
float devCameraMovementStartTime = 0; // sysTime
float devCameraMovementStartTimeRealSecs = 0;
#endif

D_CMD(ViewGrid);

dd_bool firstFrameAfterLoad;

static int loadInStartupMode = false;
static int rendCameraSmooth = true; // Smoothed by default.
static byte showFrameTimePos = false;
static byte showViewAngleDeltas = false;
static byte showViewPosDeltas = false;

int rendInfoTris = 0;

static viewport_t *currentViewport;

static coord_t *luminousDist;
static byte *luminousClipped;
static uint *luminousOrder;
static QBitArray bspLeafsVisible;

static QBitArray generatorsVisible(Map::MAX_GENERATORS);

static viewdata_t viewDataOfConsole[DDMAXPLAYERS]; // Indexed by console number.

static int frameCount; // Just for profiling purposes.

static int gridCols, gridRows;
static viewport_t viewportOfLocalPlayer[DDMAXPLAYERS];

static int resetNextViewer = true;

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

int R_FrameCount()
{
    return frameCount;
}

void R_ResetFrameCount()
{
    frameCount = 0;
}

#undef R_SetViewOrigin
DENG_EXTERN_C void R_SetViewOrigin(int consoleNum, coord_t const origin[3])
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.origin = Vector3d(origin);
}

#undef R_SetViewAngle
DENG_EXTERN_C void R_SetViewAngle(int consoleNum, angle_t angle)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.setAngle(angle);
}

#undef R_SetViewPitch
DENG_EXTERN_C void R_SetViewPitch(int consoleNum, float pitch)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.pitch = pitch;
}

void R_SetupDefaultViewWindow(int consoleNum)
{
    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;

    vd->window =
        vd->windowOld =
            vd->windowTarget = Rectanglei::fromSize(Vector2i(0, 0), Vector2ui(DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT));
    vd->windowInter = 1;
}

void R_ViewWindowTicker(int consoleNum, timespan_t ticLength)
{
    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS)
    {
        return;
    }

    vd->windowInter += float(.4 * ticLength * TICRATE);
    if(vd->windowInter >= 1)
    {
        vd->window = vd->windowTarget;
    }
    else
    {
        vd->window.moveTopLeft(Vector2i(de::roundf(de::lerp<float>(vd->windowOld.topLeft.x, vd->windowTarget.topLeft.x, vd->windowInter)),
                                        de::roundf(de::lerp<float>(vd->windowOld.topLeft.y, vd->windowTarget.topLeft.y, vd->windowInter))));
        vd->window.setSize(Vector2ui(de::roundf(de::lerp<float>(vd->windowOld.width(),  vd->windowTarget.width(),  vd->windowInter)),
                                     de::roundf(de::lerp<float>(vd->windowOld.height(), vd->windowTarget.height(), vd->windowInter))));
    }
}

#undef R_ViewWindowGeometry
DENG_EXTERN_C int R_ViewWindowGeometry(int player, RectRaw *geometry)
{
    if(!geometry) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const &vd = viewDataOfConsole[player];
    geometry->origin.x    = vd.window.topLeft.x;
    geometry->origin.y    = vd.window.topLeft.y;
    geometry->size.width  = vd.window.width();
    geometry->size.height = vd.window.height();
    return true;
}

#undef R_ViewWindowOrigin
DENG_EXTERN_C int R_ViewWindowOrigin(int player, Point2Raw *origin)
{
    if(!origin) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const &vd = viewDataOfConsole[player];
    origin->x = vd.window.topLeft.x;
    origin->y = vd.window.topLeft.y;
    return true;
}

#undef R_ViewWindowSize
DENG_EXTERN_C int R_ViewWindowSize(int player, Size2Raw *size)
{
    if(!size) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const &vd = viewDataOfConsole[player];
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
DENG_EXTERN_C void R_SetViewWindowGeometry(int player, RectRaw const *geometry, dd_bool interpolate)
{
    int p = P_ConsoleToLocal(player);
    if(p < 0) return;

    viewport_t const *vp = &viewportOfLocalPlayer[p];
    viewdata_t *vd = &viewDataOfConsole[player];

    Rectanglei newGeom = Rectanglei::fromSize(Vector2i(de::clamp<int>(0, geometry->origin.x, vp->geometry.width()),
                                                       de::clamp<int>(0, geometry->origin.y, vp->geometry.height())),
                                              Vector2ui(de::abs(geometry->size.width),
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
    if(interpolate && vd->window.size() != Vector2ui(0, 0))
    {
        vd->windowOld   = vd->window;
        vd->windowInter = 0;
    }
    else
    {
        vd->windowOld   = vd->windowTarget;
        vd->windowInter = 1; // Update on next frame.
    }
}

#undef R_ViewPortGeometry
DENG_EXTERN_C int R_ViewPortGeometry(int player, RectRaw *geometry)
{
    if(!geometry) return false;

    int p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t const &vp = viewportOfLocalPlayer[p];
    geometry->origin.x    = vp.geometry.topLeft.x;
    geometry->origin.y    = vp.geometry.topLeft.y;
    geometry->size.width  = vp.geometry.width();
    geometry->size.height = vp.geometry.height();
    return true;
}

#undef R_ViewPortOrigin
DENG_EXTERN_C int R_ViewPortOrigin(int player, Point2Raw *origin)
{
    if(!origin) return false;

    int p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t const &vp = viewportOfLocalPlayer[p];
    origin->x = vp.geometry.topLeft.x;
    origin->y = vp.geometry.topLeft.y;
    return true;
}

#undef R_ViewPortSize
DENG_EXTERN_C int R_ViewPortSize(int player, Size2Raw *size)
{
    if(!size) return false;

    int p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t const &vp = viewportOfLocalPlayer[p];
    size->width  = vp.geometry.width();
    size->height = vp.geometry.height();
    return true;
}

#undef R_SetViewPortPlayer
DENG_EXTERN_C void R_SetViewPortPlayer(int consoleNum, int viewPlayer)
{
    int p = P_ConsoleToLocal(consoleNum);
    if(p != -1)
    {
        viewportOfLocalPlayer[p].console = viewPlayer;
    }
}

/**
 * Calculate the placement and dimensions of a specific viewport.
 * Assumes that the grid has already been configured.
 */
void R_UpdateViewPortGeometry(viewport_t *port, int col, int row)
{
    DENG2_ASSERT(port != 0);

    Rectanglei newGeom = Rectanglei(Vector2i(DENG_GAMEVIEW_X + col * DENG_GAMEVIEW_WIDTH  / gridCols,
                                             DENG_GAMEVIEW_Y + row * DENG_GAMEVIEW_HEIGHT / gridRows),
                                    Vector2i(DENG_GAMEVIEW_X + (col+1) * DENG_GAMEVIEW_WIDTH  / gridCols,
                                             DENG_GAMEVIEW_Y + (row+1) * DENG_GAMEVIEW_HEIGHT / gridRows));
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

        DD_CallHooks(HOOK_VIEWPORT_RESHAPE, port->console, (void*)&p);
    }
}

bool R_SetViewGrid(int numCols, int numRows)
{
    // LensFx needs to reallocate resources only for the consoles in use.
    LensFx_GLRelease();

    if(numCols > 0 && numRows > 0)
    {
        if(numCols * numRows > DDMAXPLAYERS)
        {
            return false;
        }

        if(numCols > DDMAXPLAYERS)
            numCols = DDMAXPLAYERS;
        if(numRows > DDMAXPLAYERS)
            numRows = DDMAXPLAYERS;

        gridCols = numCols;
        gridRows = numRows;
    }

    int p = 0;
    for(int y = 0; y < gridRows; ++y)
    for(int x = 0; x < gridCols; ++x)
    {
        // The console number is -1 if the viewport belongs to no one.
        viewport_t *vp = viewportOfLocalPlayer + p;

        int const console = P_LocalToConsole(p);
        if(console != -1)
        {
            vp->console = clients[console].viewConsole;
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

int R_NextViewer()
{
    return resetNextViewer;
}

viewdata_t const *R_ViewData(int consoleNum)
{
    DENG2_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);
    return &viewDataOfConsole[consoleNum];
}

/**
 * The components whose difference is too large for interpolation will be
 * snapped to the sharp values.
 */
void R_CheckViewerLimits(viewer_t *src, viewer_t *dst)
{
    int const MAXMOVE = 32;

    /// @todo Remove this snapping. The game should determine this and disable the
    ///       the interpolation as required.
    if(fabs(dst->origin.x - src->origin.x) > MAXMOVE ||
       fabs(dst->origin.y - src->origin.y) > MAXMOVE)
    {
        src->origin = dst->origin;
    }

    /*
    if(abs(int(dst->angle) - int(src->angle)) >= ANGLE_45)
    {
        LOG_DEBUG("R_CheckViewerLimits: Snap camera angle to %08x.") << dst->angle;
        src->angle = dst->angle;
    }
    */
}

/**
 * Retrieve the current sharp camera position.
 */
viewer_t R_SharpViewer(player_t &player)
{
    DENG2_ASSERT(player.shared.mo != 0);

    ddplayer_t const &ddpl = player.shared;

    viewer_t view(viewDataOfConsole[&player - ddPlayers].latest);

    if((ddpl.flags & DDPF_CHASECAM) && !(ddpl.flags & DDPF_CAMERA))
    {
        /* STUB
         * This needs to be fleshed out with a proper third person
         * camera control setup. Currently we simply project the viewer's
         * position a set distance behind the ddpl.
         */
        float const distance = 90;

        uint angle = view.angle() >> ANGLETOFINESHIFT;
        uint pitch = angle_t(LOOKDIR2DEG(view.pitch) / 360 * ANGLE_MAX) >> ANGLETOFINESHIFT;

        view.origin -= Vector3d(FIX2FLT(fineCosine[angle]),
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

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        viewdata_t *vd = &viewDataOfConsole[i];
        player_t *plr = &ddPlayers[i];

        if(/*(plr->shared.flags & DDPF_LOCAL) &&*/
           (!plr->shared.inGame || !plr->shared.mo))
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

    if(ClientApp::worldSystem().hasMap())
    {
        Map &map = ClientApp::worldSystem().map();
        map.updateTrackedPlanes();
        map.updateScrollingSurfaces();
    }
}

void R_UpdateViewer(int consoleNum)
{
    DENG_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);

    int const VIEWPOS_MAX_SMOOTHDISTANCE = 172;

    viewdata_t *vd   = viewDataOfConsole + consoleNum;
    player_t *player = ddPlayers + consoleNum;

    if(!player->shared.inGame) return;
    if(!player->shared.mo) return;

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
    else //if(!clientPaused)
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
                double time;
                float yaw, pitch;
            };

            static OldAngle oldAngle[DDMAXPLAYERS];
            OldAngle *old = &oldAngle[viewPlayer - ddPlayers];
            float yaw = (double)smoothView.angle() / ANGLE_MAX * 360;

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
                double time;
                Vector3f pos;
            };

            static OldPos oldPos[DDMAXPLAYERS];
            OldPos *old = &oldPos[viewPlayer - ddPlayers];

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
    angle_t const viewYaw = vd->current.angle();

    uint const an = viewYaw >> ANGLETOFINESHIFT;
    vd->viewSin = FIX2FLT(finesine[an]);
    vd->viewCos = FIX2FLT(fineCosine[an]);

    // Calculate the front, up and side unit vectors.
    float const yawRad = ((viewYaw / (float) ANGLE_MAX) *2) * PI;
    float const pitchRad = vd->current.pitch * 85 / 110.f / 180 * PI;

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
    if(player->targetExtraLight != player->shared.extraLight)
    {
        player->targetExtraLight = player->shared.extraLight;
        player->extraLightCounter = MINEXTRALIGHTFRAMES;
    }

    if(player->extraLightCounter > 0)
    {
        player->extraLightCounter--;
        if(player->extraLightCounter == 0)
            player->extraLight = player->targetExtraLight;
    }

    // Why?
    validCount++;

    extraLight = player->extraLight;
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

void R_UseViewPort(viewport_t const *vp)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!vp)
    {
        currentViewport = 0;
        ClientWindow::main().game().glApplyViewport(
                Rectanglei::fromSize(Vector2i(DENG_GAMEVIEW_X, DENG_GAMEVIEW_Y),
                                     Vector2ui(DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT)));
    }
    else
    {
        currentViewport = const_cast<viewport_t *>(vp);
        ClientWindow::main().game().glApplyViewport(vp->geometry);
    }
}

viewport_t const *R_CurrentViewPort()
{
    return currentViewport;
}

void R_RenderBlankView()
{
    UI_DrawDDBackground(Point2Raw(0, 0), Size2Raw(320, 200), 1);
}

void R_SetupPlayerSprites()
{
    psp3d = false;

    // Cameramen have no psprites.
    ddplayer_t *ddpl = &viewPlayer->shared;
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    if(!ddpl->mo)
        return;
    mobj_t *mo = ddpl->mo;

    if(!Mobj_HasCluster(*mo))
        return;
    SectorCluster &cluster = Mobj_Cluster(*mo);

    // Determine if we should be drawing all the psprites full bright?
    dd_bool isFullBright = (levelFullBright != 0);
    if(!isFullBright)
    {
        ddpsprite_t *psp = ddpl->pSprites;
        for(int i = 0; i < DDMAXPSPRITES; ++i, psp++)
        {
            if(!psp->statePtr) continue;

            // If one of the psprites is fullbright, both are.
            if(psp->statePtr->flags & STF_FULLBRIGHT)
                isFullBright = true;
        }
    }

    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    ddpsprite_t *psp = ddpl->pSprites;
    for(int i = 0; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t *spr = &visPSprites[i];

        spr->type = VPSPR_SPRITE;
        spr->psp = psp;

        if(!psp->statePtr) continue;

        // First, determine whether this is a model or a sprite.
        bool isModel = false;
        ModelDef *mf = 0, *nextmf = 0;
        float inter = 0;
        if(useModels)
        {
            // Is there a model for this frame?
            mobj_t dummy;

            // Setup a dummy for the call to R_CheckModelFor.
            dummy.state = psp->statePtr;
            dummy.tics = psp->tics;

            mf = Mobj_ModelDef(dummy, &nextmf, &inter);
            if(mf) isModel = true;
        }

        if(isModel)
        {
            // Yes, draw a 3D model (in Rend_Draw3DPlayerSprites).
            // There are 3D psprites.
            psp3d = true;

            spr->type = VPSPR_MODEL;

            spr->data.model.bspLeaf = &Mobj_BspLeafAtOrigin(*mo);
            spr->data.model.flags = 0;
            // 32 is the raised weapon height.
            spr->data.model.gzt = viewData->current.origin.z;
            spr->data.model.secFloor = cluster.visFloor().heightSmoothed();
            spr->data.model.secCeil  = cluster.visCeiling().heightSmoothed();
            spr->data.model.pClass = 0;
            spr->data.model.floorClip = 0;

            spr->data.model.mf = mf;
            spr->data.model.nextMF = nextmf;
            spr->data.model.inter = inter;
            spr->data.model.viewAligned = true;
            spr->origin[VX] = viewData->current.origin.x;
            spr->origin[VY] = viewData->current.origin.y;
            spr->origin[VZ] = viewData->current.origin.z;

            // Offsets to rotation angles.
            spr->data.model.yawAngleOffset = psp->pos[VX] * weaponOffsetScale - 90;
            spr->data.model.pitchAngleOffset =
                (32 - psp->pos[VY]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
            // Is the FOV shift in effect?
            if(weaponFOVShift > 0 && Rend_FieldOfView() > 90)
                spr->data.model.pitchAngleOffset -= weaponFOVShift * (Rend_FieldOfView() - 90) / 90;
            // Real rotation angles.
            spr->data.model.yaw =
                viewData->current.angle() / (float) ANGLE_MAX *-360 + spr->data.model.yawAngleOffset + 90;
            spr->data.model.pitch = viewData->current.pitch * 85 / 110 + spr->data.model.yawAngleOffset;
            memset(spr->data.model.visOff, 0, sizeof(spr->data.model.visOff));

            spr->data.model.alpha = psp->alpha;
            spr->data.model.stateFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
        }
        else
        {
            // No, draw a 2D sprite (in Rend_DrawPlayerSprites).
            spr->type = VPSPR_SPRITE;

            // Adjust the center slightly so an angle can be calculated.
            spr->origin[VX] = viewData->current.origin.x;
            spr->origin[VY] = viewData->current.origin.y;
            spr->origin[VZ] = viewData->current.origin.z;

            spr->data.sprite.bspLeaf = &Mobj_BspLeafAtOrigin(*mo);
            spr->data.sprite.alpha = psp->alpha;
            spr->data.sprite.isFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
        }
    }
}

#undef R_RenderPlayerView
DENG_EXTERN_C void R_RenderPlayerView(int num)
{
    if(num < 0 || num >= DDMAXPLAYERS) return; // Huh?
    player_t *player = &ddPlayers[num];

    if(!player->shared.inGame) return;
    if(!player->shared.mo) return;

    if(firstFrameAfterLoad)
    {
        // Don't let the clock run yet.  There may be some texture
        // loading still left to do that we have been unable to
        // predetermine.
        firstFrameAfterLoad = false;
        DD_ResetTimer();
    }

    // Too early? Game has not configured the view window?
    viewdata_t *vd = &viewDataOfConsole[num];
    if(vd->window.isNull()) return;

    // Setup for rendering the frame.
    R_SetupFrame(player);

    // Latest possible time to check the real head angles. After this we'll be
    // using the provided values.
    VR::updateHeadOrientation();

    R_SetupPlayerSprites();

    // Hide the viewPlayer's mobj?
    int oldFlags = 0;
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        oldFlags = player->shared.mo->ddFlags;
        player->shared.mo->ddFlags |= DDMF_DONTDRAW;
    }

    // Go to wireframe mode?
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // GL is in 3D transformation state only during the frame.
    GL_SwitchTo3DState(true, currentViewport, vd);

    if(ClientApp::worldSystem().hasMap())
    {
        Rend_RenderMap(ClientApp::worldSystem().map());
    }

    // Orthogonal projection to the view window.
    GL_Restore2DState(1, currentViewport, vd);

    // Don't render in wireframe mode with 2D psprites.
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    Rend_Draw2DPlayerSprites(); // If the 2D versions are needed.

    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Do we need to render any 3D psprites?
    if(psp3d)
    {
        GL_SwitchTo3DState(false, currentViewport, vd);
        Rend_Draw3DPlayerSprites();
    }

    // Restore fullscreen viewport, original matrices and state: back to normal 2D.
    GL_Restore2DState(2, currentViewport, vd);

    // Back from wireframe mode?
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Now we can show the viewPlayer's mobj again.
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        player->shared.mo->ddFlags = oldFlags;
    }

    R_PrintRendPoolInfo();

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
    {
        static float prevPos[3] = { 0, 0, 0 };
        static float prevSpeed = 0;
        static float prevTime;
        float delta[2] = { vd->current.pos[VX] - prevPos[VX],
                           vd->current.pos[VY] - prevPos[VY] };
        float speed = V2f_Length(delta);
        float time = sysTime - devCameraMovementStartTime;
        float elapsed = time - prevTime;

        LOGDEV_MSG("%f,%f,%f,%f,%f") << Sys_GetRealSeconds() - devCameraMovementStartTimeRealSecs
                                     << time << elapsed << speed/elapsed << speed/elapsed - prevSpeed;

        V3f_Copy(prevPos, vd->current.pos);
        prevSpeed = speed/elapsed;
        prevTime = time;
    }
#endif
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

static void clearViewPorts()
{
    GLbitfield bits = GL_DEPTH_BUFFER_BIT;

    if(!devRendSkyMode)
        bits |= GL_STENCIL_BUFFER_BIT;

    if(freezeRLs)
    {
        bits |= GL_COLOR_BUFFER_BIT;
    }
    else
    {
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];

            if(!plr->shared.inGame || !(plr->shared.flags & DDPF_LOCAL))
                continue;

            if(P_IsInVoid(plr))
            {
                bits |= GL_COLOR_BUFFER_BIT;
                break;
            }
        }
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // This is all the clearing we'll do.
    glClear(bits);
}

void R_RenderViewPorts(ViewPortLayer layer)
{
    int oldDisplay = displayPlayer;

    // First clear the viewport.
    if(layer == Player3DViewLayer)
    {
        clearViewPorts();
    }

    // Draw a view for all players with a visible viewport.
    for(int p = 0, y = 0; y < gridRows; ++y)
    {
        for(int x = 0; x < gridCols; x++, ++p)
        {
            viewport_t const *vp = &viewportOfLocalPlayer[p];
            displayPlayer = vp->console;

            R_UseViewPort(vp);

            if(displayPlayer < 0 || (ddPlayers[displayPlayer].shared.flags & DDPF_UNDEFINED_ORIGIN))
            {
                if(layer == Player3DViewLayer)
                {
                    R_RenderBlankView();
                }
                continue;
            }

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();

            /**
             * Use an orthographic projection in real pixel dimensions.
             */
            glOrtho(0, vp->geometry.width(), vp->geometry.height(), 0, -1, 1);

            viewdata_t const *vd = &viewDataOfConsole[vp->console];
            RectRaw vpGeometry(vp->geometry.topLeft.x, vp->geometry.topLeft.y,
                               vp->geometry.width(), vp->geometry.height());

            RectRaw vdWindow(vd->window.topLeft.x, vd->window.topLeft.y,
                             vd->window.width(), vd->window.height());

            switch(layer)
            {
            case Player3DViewLayer:
                R_UpdateViewer(vp->console);
                LensFx_BeginFrame(vp->console);
                gx.DrawViewPort(p, &vpGeometry, &vdWindow, displayPlayer, 0/*layer #0*/);
                LensFx_EndFrame();
                break;

            case ViewBorderLayer:
                R_RenderPlayerViewBorder();
                break;

            case HUDLayer:
                gx.DrawViewPort(p, &vpGeometry, &vdWindow, displayPlayer, 1/*layer #1*/);
                break;
            }

            restoreDefaultGLState();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
        }
    }

    if(layer == Player3DViewLayer)
    {
        // Increment the internal frame count. This does not
        // affect the window's FPS counter.
        frameCount++;

        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
            resetNextViewer = 0;
    }

    // Restore things back to normal.
    displayPlayer = oldDisplay;
    R_UseViewPort(NULL);
}

void R_ClearViewData()
{
    M_Free(luminousDist); luminousDist = 0;
    M_Free(luminousClipped); luminousClipped = 0;
    M_Free(luminousOrder); luminousOrder = 0;
}

bool R_ViewerBspLeafIsVisible(BspLeaf const &bspLeaf)
{
    DENG2_ASSERT(bspLeaf.indexInMap() != MapElement::NoIndex);
    return bspLeafsVisible.testBit(bspLeaf.indexInMap());
}

void R_ViewerBspLeafMarkVisible(BspLeaf const &bspLeaf, bool yes)
{
    DENG2_ASSERT(bspLeaf.indexInMap() != MapElement::NoIndex);
    bspLeafsVisible.setBit(bspLeaf.indexInMap(), yes);
}

bool R_ViewerGeneratorIsVisible(Generator const &generator)
{
    return generatorsVisible.testBit(generator.id());
}

void R_ViewerGeneratorMarkVisible(Generator const &generator, bool yes)
{
    generatorsVisible.setBit(generator.id(), yes);
}

double R_ViewerLumobjDistance(int idx)
{
    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < ClientApp::worldSystem().map().lumobjCount())
    {
        return luminousDist[idx];
    }
    return 0;
}

bool R_ViewerLumobjIsClipped(int idx)
{
    // If we are not yet prepared for this, just say everything is clipped.
    if(!luminousClipped) return true;

    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < ClientApp::worldSystem().map().lumobjCount())
    {
        return CPP_BOOL(luminousClipped[idx]);
    }
    return false;
}

bool R_ViewerLumobjIsHidden(int idx)
{
    // If we are not yet prepared for this, just say everything is hidden.
    if(!luminousClipped) return true;

    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < ClientApp::worldSystem().map().lumobjCount())
    {
        return luminousClipped[idx] == 2;
    }
    return false;
}

/// Used to sort lumobjs by distance from viewpoint.
static int lumobjSorter(void const *e1, void const *e2)
{
    coord_t a = luminousDist[*(uint const *) e1];
    coord_t b = luminousDist[*(uint const *) e2];
    if(a > b) return 1;
    if(a < b) return -1;
    return 0;
}

void R_BeginFrame()
{
    /*
     * Clear the projected texture lists. This is done here as the projections
     * are sensitive to distance from the viewer.
     */
    Rend_ProjectorReset();

    Map &map = ClientApp::worldSystem().map();

    bspLeafsVisible.resize(map.bspLeafCount());
    bspLeafsVisible.fill(false);

    // Clear all generator visibility flags.
    generatorsVisible.fill(false);

    int numLuminous = map.lumobjCount();
    if(!(numLuminous > 0)) return;

    // Resize the associated buffers used for per-frame stuff.
    int maxLuminous = numLuminous;
    luminousDist    = (coord_t *) M_Realloc(luminousDist,    sizeof(*luminousDist)    * maxLuminous);
    luminousClipped =    (byte *) M_Realloc(luminousClipped, sizeof(*luminousClipped) * maxLuminous);
    luminousOrder   =    (uint *) M_Realloc(luminousOrder,   sizeof(*luminousOrder)   * maxLuminous);

    // Update viewer => lumobj distances ready for linking and sorting.
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    foreach(Lumobj *lum, map.lumobjs())
    {
        // Approximate the distance in 3D.
        Vector3d delta = lum->origin() - viewData->current.origin;
        luminousDist[lum->indexInMap()] = M_ApproxDistance3(delta.x, delta.y, delta.z * 1.2 /*correct aspect*/);
    }

    if(rendMaxLumobjs > 0 && numLuminous > rendMaxLumobjs)
    {
        // Sort lumobjs by distance from the viewer. Then clip all lumobjs
        // so that only the closest are visible (max loMaxLumobjs).

        // Init the lumobj indices, sort array.
        for(int i = 0; i < numLuminous; ++i)
        {
            luminousOrder[i] = i;
        }
        qsort(luminousOrder, numLuminous, sizeof(uint), lumobjSorter);

        // Mark all as hidden.
        std::memset(luminousClipped, 2, numLuminous * sizeof(*luminousClipped));

        int n = 0;
        for(int i = 0; i < numLuminous; ++i)
        {
            if(n++ > rendMaxLumobjs)
                break;

            // Unhide this lumobj.
            luminousClipped[luminousOrder[i]] = 1;
        }
    }
    else
    {
        // Mark all as clipped.
        std::memset(luminousClipped, 1, numLuminous * sizeof(*luminousClipped));
    }
}

void R_ViewerClipLumobj(Lumobj *lum)
{
    if(!lum) return;

    // Has this already been occluded?
    int lumIdx = lum->indexInMap();
    if(luminousClipped[lumIdx] > 1)
        return;

    luminousClipped[lumIdx] = 0;

    /// @todo Determine the exact centerpoint of the light in addLuminous!
    Vector3d origin = lum->origin();
    origin.z += lum->zOffset();

    if(!(devNoCulling || P_IsInVoid(&ddPlayers[displayPlayer])))
    {
        if(!C_IsPointVisible(origin))
        {
            luminousClipped[lumIdx] = 1; // Won't have a halo.
        }
    }
    else
    {
        luminousClipped[lumIdx] = 1;

        Vector3d const eye = vOrigin.xzy();

        if(LineSightTest(eye, origin, -1, 1, LS_PASSLEFT | LS_PASSOVER | LS_PASSUNDER)
                .trace(lum->map().bspRoot()))
        {
            luminousClipped[lumIdx] = 0; // Will have a halo.
        }
    }
}

void R_ViewerClipLumobjBySight(Lumobj *lum, BspLeaf *bspLeaf)
{
    if(!lum || !bspLeaf) return;

    // Already clipped?
    int lumIdx = lum->indexInMap();
    if(luminousClipped[lumIdx])
        return;

    // We need to figure out if any of the polyobj's segments lies
    // between the viewpoint and the lumobj.
    Vector3d const eye = vOrigin.xzy();

    foreach(Polyobj *po, bspLeaf->polyobjs())
    foreach(HEdge *hedge, po->mesh().hedges())
    {
        // Is this on the back of a one-sided line?
        if(!hedge->hasMapElement())
            continue;

        // Ignore half-edges facing the wrong way.
        if(hedge->mapElementAs<LineSideSegment>().isFrontFacing())
        {
            coord_t eyeV1[2]       = { eye.x, eye.y };
            coord_t lumOriginV1[2] = { lum->origin().x, lum->origin().y };
            coord_t fromV1[2]      = { hedge->origin().x, hedge->origin().y };
            coord_t toV1[2]        = { hedge->twin().origin().x, hedge->twin().origin().y };
            if(V2d_Intercept2(lumOriginV1, eyeV1, fromV1, toV1, 0, 0, 0))
            {
                luminousClipped[lumIdx] = 1;
                break;
            }
        }
    }
}

D_CMD(ViewGrid)
{
    DENG2_UNUSED2(src, argc);
    // Recalculate viewports.
    return R_SetViewGrid(String(argv[1]).toInt(), String(argv[2]).toInt());
}

angle_t viewer_t::angle() const
{
    angle_t a = _angle;
    if(DD_GetInteger(DD_USING_HEAD_TRACKING))
    {
        // Apply the actual, current yaw offset. The game has omitted the "body yaw"
        // portion from the value already.
        a += (fixed_t)(radianToDegree(VR::getHeadOrientation()[2]) / 180 * ANGLE_180);
    }
    return a;
}
