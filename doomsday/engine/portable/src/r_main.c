/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * r_main.c: Refresh Subsystem
 *
 * The refresh daemon has the highest-level rendering code.
 * The view window is handled by refresh. The more specialized
 * rendering code in rend_*.c does things inside the view window.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_MOBJ_INIT_ADD
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ViewGrid);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern byte rendInfoRPolys;
extern byte freezeRLs;
extern boolean firstFrameAfterLoad;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int validCount = 1; // Increment every time a check is made.
int frameCount; // Just for profiling purposes.
int rendInfoTris = 0;
int useVSync = 0;
float viewX = 0, viewY = 0, viewZ = 0, viewPitch = 0;
int viewAngle = 0;
boolean setSizeNeeded;

// Precalculated math tables.
fixed_t* fineCosine = &finesine[FINEANGLES / 4];

int extraLight; // Bumped light from gun blasts.
float extraLightDelta;

float frameTimePos; // 0...1: fractional part for sharp game tics.

int loadInStartupMode = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int rendCameraSmooth = true; // Smoothed by default.

static boolean resetNextViewer = true;

static viewdata_t viewData[DDMAXPLAYERS];

static byte showFrameTimePos = false;
static byte showViewAngleDeltas = false;
static byte showViewPosDeltas = false;

static int gridCols, gridRows;
static viewport_t viewports[DDMAXPLAYERS], *currentPort;

// CODE --------------------------------------------------------------------

/**
 * Register console variables.
 */
void R_Register(void)
{
    C_VAR_INT("con-show-during-setup", &loadInStartupMode, 0, 0, 1);

    C_VAR_INT("rend-camera-smooth", &rendCameraSmooth, CVF_HIDE, 0, 1);

    C_VAR_BYTE("rend-info-deltas-angles", &showViewAngleDeltas, 0, 0, 1);
    C_VAR_BYTE("rend-info-deltas-pos", &showViewPosDeltas, 0, 0, 1);
    C_VAR_BYTE("rend-info-frametime", &showFrameTimePos, 0, 0, 1);
    C_VAR_BYTE("rend-info-rendpolys", &rendInfoRPolys, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-info-tris", &rendInfoTris, 0, 0, 1);

//    C_VAR_INT("rend-vsync", &useVSync, 0, 0, 1);

    C_CMD("viewgrid", "ii", ViewGrid);

    P_MaterialManagerRegister();
}

/**
 * Will the specified surface be added to the sky mask?
 *
 * @param suf           Ptr to the surface to test.
 * @return boolean      @c true, iff the surface will be masked.
 */
boolean R_IsSkySurface(const surface_t* suf)
{
    if(suf && suf->material && (suf->material->flags & MATF_SKYMASK))
        return true;

    return false;
}

/**
 * Don't really change anything here, because i might be in the middle of
 * a refresh.  The change will take effect next refresh.
 */
void R_SetViewWindow(int x, int y, int w, int h)
{
    viewwindowx = x;
    viewwindowy = y;
    viewwidth = w;
    viewheight = h;
}

/**
 * Retrieve the dimensions of the specified viewport by console player num.
 */
int R_GetViewPort(int player, int* x, int* y, int* w, int* h)
{
    int                 p = P_ConsoleToLocal(player);

    if(p != -1)
    {
        if(x) *x = viewports[p].x;
        if(y) *y = viewports[p].y;
        if(w) *w = viewports[p].width;
        if(h) *h = viewports[p].height;

        return p;
    }

    return -1;
}

/**
 * Sets the view player for a console.
 *
 * @param consoleNum  Player whose view to set.
 * @param viewPlayer  Player that will be viewed by player @a consoleNum.
 */
void R_SetViewPortPlayer(int consoleNum, int viewPlayer)
{
    int p = P_ConsoleToLocal(consoleNum);
    if(p != -1)
    {
        viewports[p].console = viewPlayer;
    }
}

/**
 * Calculate the placement and dimensions of a specific viewport.
 * Assumes that the grid has already been configured.
 */
void R_ViewPortPlacement(viewport_t* port, int x, int y)
{
    float               w = theWindow->width / (float) gridCols;
    float               h = theWindow->height / (float) gridRows;

    port->x = x * w;
    port->y = y * h;

    port->width = (x + 1) * w - port->x;
    port->height = (y + 1) * h - port->y;
}

/**
 * Set up a view grid and calculate the viewports.  Set 'numCols' and
 * 'numRows' to zero to just update the viewport coordinates.
 */
void R_SetViewGrid(int numCols, int numRows)
{
    int                 x, y, p;

    if(numCols > 0 && numRows > 0)
    {
        if(numCols > 16)
            numCols = 16;
        if(numRows > 16)
            numRows = 16;

        gridCols = numCols;
        gridRows = numRows;
    }

    // Reset all viewports to zero.
    memset(viewports, 0, sizeof(viewports));

    for(p = 0, y = 0; y < gridRows; ++y)
    {
        for(x = 0; x < gridCols; ++x, ++p)
        {
            R_ViewPortPlacement(&viewports[p], x, y);

            // The console number is -1 if the viewport belongs to no
            // one.
            viewports[p].console = displayPlayer; //clients[P_LocalToConsole(p)].viewConsole;
        }
    }
}

/**
 * One-time initialization of the refresh daemon. Called by DD_Main.
 * GL has not yet been inited.
 */
void R_Init(void)
{
    R_InitData();
    // viewwidth / viewheight / detailLevel are set by the defaults
    R_SetViewWindow(0, 0, 320, 200);
    R_InitSprites(); // Fully initialize sprites.
    R_InitTranslationTables();
    Rend_Init();
    frameCount = 0;
    R_InitViewBorder();
    P_PtcInit();

    // Defs have been read; we can now init models.
    R_InitModels();

    Def_PostInit();
}

/**
 * Re-initialize almost everything.
 */
void R_Update(void)
{
    uint                i;

    R_UpdateTexturesAndFlats();
    R_InitTextures();
    R_InitFlats();
    R_PreInitSprites();

    // Re-read definitions.
    Def_Read();

    R_UpdateData();
    R_InitSprites(); // Fully reinitialize sprites.
    R_UpdateTranslationTables();

    R_InitModels(); // Defs might've changed.

    // Now that we've read the defs, we can load system textures.
    GL_LoadSystemTextures();

    Def_PostInit();
    P_UpdateParticleGens(); // Defs might've changed.

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t*           plr = &ddPlayers[i];
        ddplayer_t*         ddpl = &plr->shared;

        // States have changed, the states are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = NULL;
    }

    // Update all world surfaces.
    for(i = 0; i < numSectors; ++i)
    {
        uint                j;
        sector_t*           sec = &sectors[i];

        for(j = 0; j < sec->planeCount; ++j)
            Surface_Update(&sec->SP_planesurface(j));
    }

    for(i = 0; i < numSideDefs; ++i)
    {
        sidedef_t*          side = &sideDefs[i];

        Surface_Update(&side->SW_topsurface);
        Surface_Update(&side->SW_middlesurface);
        Surface_Update(&side->SW_bottomsurface);
    }

    for(i = 0; i < numPolyObjs; ++i)
    {
        polyobj_t*          po = polyObjs[i];
        seg_t**             segPtr = po->segs;

        while(*segPtr)
        {
            sidedef_t*          side = SEG_SIDEDEF(*segPtr);

            Surface_Update(&side->SW_middlesurface);

            segPtr++;
        }
    }

    // The rendering lists have persistent data that has changed during
    // the re-initialization.
    RL_DeleteLists();

    // Update the secondary title and the game status.
    Con_InitUI();

#if _DEBUG
    Z_CheckHeap();
#endif
}

/**
 * Shutdown the refresh daemon.
 */
void R_Shutdown(void)
{
    R_ShutdownModels();
    R_ShutdownData();
    R_ShutdownResourceLocator();
    // Most allocated memory goes down with the zone.
}

void R_ResetViewer(void)
{
    resetNextViewer = 1;
}

void R_InterpolateViewer(viewer_t* start, viewer_t* end, float pos,
                         viewer_t* out)
{
    float               inv = 1 - pos;

    out->pos[VX] = inv * start->pos[VX] + pos * end->pos[VX];
    out->pos[VY] = inv * start->pos[VY] + pos * end->pos[VY];
    out->pos[VZ] = inv * start->pos[VZ] + pos * end->pos[VZ];

    out->angle = start->angle + pos * ((int) end->angle - (int) start->angle);
    out->pitch = inv * start->pitch + pos * end->pitch;
}

void R_CopyViewer(viewer_t* dst, const viewer_t* src)
{
    dst->pos[VX] = src->pos[VX];
    dst->pos[VY] = src->pos[VY];
    dst->pos[VZ] = src->pos[VZ];
    dst->angle = src->angle;
    dst->pitch = src->pitch;
}

const viewdata_t* R_ViewData(int localPlayerNum)
{
    assert(localPlayerNum >= 0 && localPlayerNum < DDMAXPLAYERS);

    return &viewData[localPlayerNum];
}

/**
 * The components whose difference is too large for interpolation will be
 * snapped to the sharp values.
 */
void R_CheckViewerLimits(viewer_t* src, viewer_t* dst)
{
#define MAXMOVE 32

    if(fabs(dst->pos[VX] - src->pos[VX]) > MAXMOVE ||
       fabs(dst->pos[VY] - src->pos[VY]) > MAXMOVE)
    {
        src->pos[VX] = dst->pos[VX];
        src->pos[VY] = dst->pos[VY];
        src->pos[VZ] = dst->pos[VZ];
    }
    if(abs((int) dst->angle - (int) src->angle) >= ANGLE_45)
        src->angle = dst->angle;

#undef MAXMOVE
}

/**
 * Retrieve the current sharp camera position.
 */
void R_GetSharpView(viewer_t* view, player_t* player)
{
    ddplayer_t* ddpl;

    if(!player || !player->shared.mo)
    {
        return;
    }

    ddpl = &player->shared;

    view->pos[VX] = viewX;
    view->pos[VY] = viewY;
    view->pos[VZ] = viewZ;
    /* $unifiedangles */
    view->angle = viewAngle;
    view->pitch = viewPitch;

    if((ddpl->flags & DDPF_CHASECAM) && !(ddpl->flags & DDPF_CAMERA))
    {
        /* STUB
         * This needs to be fleshed out with a proper third person
         * camera control setup. Currently we simply project the viewer's
         * position a set distance behind the ddpl.
         */
        angle_t pitch = LOOKDIR2DEG(view->pitch) / 360 * ANGLE_MAX;
        angle_t angle = view->angle;
        float distance = 90;

        angle = view->angle >> ANGLETOFINESHIFT;
        pitch >>= ANGLETOFINESHIFT;

        view->pos[VX] -= distance * FIX2FLT(fineCosine[angle]);
        view->pos[VY] -= distance * FIX2FLT(finesine[angle]);
        view->pos[VZ] -= distance * FIX2FLT(finesine[pitch]);
    }

    // Check that the viewZ doesn't go too high or low.
    // Cameras are not restricted.
    if(!(ddpl->flags & DDPF_CAMERA))
    {
        if(view->pos[VZ] > ddpl->mo->ceilingZ - 4)
        {
            view->pos[VZ] = ddpl->mo->ceilingZ - 4;
        }

        if(view->pos[VZ] < ddpl->mo->floorZ + 4)
        {
            view->pos[VZ] = ddpl->mo->floorZ + 4;
        }
    }
}

/**
 * Update the sharp world data by rotating the stored values of plane
 * heights and sharp camera positions.
 */
void R_NewSharpWorld(void)
{
    extern boolean firstFrameAfterLoad;

    int                 i;

    if(firstFrameAfterLoad)
    {
        /**
         * We haven't yet drawn the world. Everything *is* sharp so simply
         * reset the viewer data.
         * \fixme A bit of a kludge?
         */
        memset(viewData, 0, sizeof(viewData));
        return;
    }

    if(resetNextViewer)
        resetNextViewer = 2;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        viewer_t sharpView;
        viewdata_t* vd = &viewData[i];
        player_t* plr = &ddPlayers[i];

        if(/*(plr->shared.flags & DDPF_LOCAL) &&*/
           (!plr->shared.inGame || !plr->shared.mo))
        {
            continue;
        }

        R_GetSharpView(&sharpView, plr);

        // Update the camera angles that will be used when the camera is
        // not smoothed.
        vd->frozenAngle = sharpView.angle;
        vd->frozenPitch = sharpView.pitch;

        // The game tic has changed, which means we have an updated sharp
        // camera position.  However, the position is at the beginning of
        // the tic and we are most likely not at a sharp tic boundary, in
        // time.  We will move the viewer positions one step back in the
        // buffer.  The effect of this is that [0] is the previous sharp
        // position and [1] is the current one.

        memcpy(&vd->lastSharp[0], &vd->lastSharp[1], sizeof(viewer_t));
        memcpy(&vd->lastSharp[1], &sharpView, sizeof(sharpView));

        R_CheckViewerLimits(vd->lastSharp, &sharpView);
    }

    R_UpdateWatchedPlanes(watchedPlaneList);
    R_UpdateMovingSurfaces();
}

void R_CreateMobjLinks(void)
{
    uint                i;
    sector_t*           seciter;
#ifdef DD_PROFILE
    static int          p;

    if(++p > 40)
    {
        p = 0;
        PRINT_PROF( PROF_MOBJ_INIT_ADD );
    }
#endif

BEGIN_PROF( PROF_MOBJ_INIT_ADD );

    for(i = 0, seciter = sectors; i < numSectors; seciter++, ++i)
    {
        mobj_t*             iter;

        for(iter = seciter->mobjList; iter; iter = iter->sNext)
        {
            R_ObjLinkCreate(iter, OT_MOBJ); // For spreading purposes.
        }
    }

END_PROF( PROF_MOBJ_INIT_ADD );
}

/**
 * Prepare for rendering view(s) of the world.
 */
void R_BeginWorldFrame(void)
{
    R_ClearSectorFlags();

    R_InterpolateWatchedPlanes(watchedPlaneList, resetNextViewer);
    R_InterpolateMovingSurfaces(resetNextViewer);

    if(!freezeRLs)
    {
        LG_Update();
        SB_BeginFrame();
        LO_ClearForFrame();
        R_ClearObjLinksForFrame(); // Zeroes the links.

        // Clear the objlinks.
        R_InitForNewFrame();

        // Generate surface decorations for the frame.
        Rend_InitDecorationsForFrame();

        // Spawn omnilights for decorations.
        Rend_AddLuminousDecorations();

        // Spawn omnilights for mobjs.
        LO_AddLuminousMobjs();

        // Create objlinks for mobjs.
        R_CreateMobjLinks();

        // Link all active particle generators into the world.
        P_CreatePtcGenLinks();

        // Link objs to all contacted surfaces.
        R_LinkObjs();
    }
}

/**
 * Wrap up after drawing view(s) of the world.
 */
void R_EndWorldFrame(void)
{
    if(!freezeRLs)
    {
        // Wrap up with Source, Bias lights.
        SB_EndFrame();
    }
}

/**
 * Prepare rendering the view of the given player.
 */
void R_SetupFrame(player_t* player)
{
#define VIEWPOS_MAX_SMOOTHDISTANCE  172
#define MINEXTRALIGHTFRAMES         2

    int                 tableAngle;
    float               yawRad, pitchRad;
    viewer_t            sharpView, smoothView;
    viewdata_t*         vd;

    // Reset the GL triangle counter.
    polyCounter = 0;

    viewPlayer = player;
    vd = &viewData[viewPlayer - ddPlayers];

    R_GetSharpView(&sharpView, viewPlayer);

    if(resetNextViewer ||
       V3_Distance(vd->current.pos, sharpView.pos) > VIEWPOS_MAX_SMOOTHDISTANCE)
    {
#ifdef _DEBUG
        Con_Message("R_SetupFrame: resetNextViewer = %i\n", resetNextViewer);
#endif

        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
            resetNextViewer = 0;

        // Just view from the sharp position.
        R_CopyViewer(&vd->current, &sharpView);

        memcpy(&vd->lastSharp[0], &sharpView, sizeof(sharpView));
        memcpy(&vd->lastSharp[1], &sharpView, sizeof(sharpView));
    }
    // While the game is paused there is no need to calculate any
    // time offsets or interpolated camera positions.
    else //if(!clientPaused)
    {
        // Calculate the smoothed camera position, which is somewhere
        // between the previous and current sharp positions. This
        // introduces a slight delay (max. 1/35 sec) to the movement
        // of the smoothed camera.
        R_InterpolateViewer(vd->lastSharp, vd->lastSharp + 1, frameTimePos,
                            &smoothView);

        // Use the latest view angles known to us, if the interpolation flags
        // are not set. The interpolation flags are used when the view angles
        // are updated during the sharp tics and need to be smoothed out here.
        // For example, view locking (dead or camera setlock).
        if(!(player->shared.flags & DDPF_INTERYAW))
            smoothView.angle = sharpView.angle;
        if(!(player->shared.flags & DDPF_INTERPITCH))
            smoothView.pitch = sharpView.pitch;

        R_CopyViewer(&vd->current, &smoothView);

        // Monitor smoothness of yaw/pitch changes.
        if(showViewAngleDeltas)
        {
            typedef struct oldangle_s {
                double           time;
                float            yaw, pitch;
            } oldangle_t;

            static oldangle_t       oldangle[DDMAXPLAYERS];
            oldangle_t*             old = &oldangle[viewPlayer - ddPlayers];
            float                   yaw =
                (double)smoothView.angle / ANGLE_MAX * 360;

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f "
                        "Rdx=%-10.3f Rdy=%-10.3f\n",
                        SECONDS_TO_TICKS(gameTime),
                        frameTimePos,
                        sysTime - old->time,
                        yaw - old->yaw,
                        smoothView.pitch - old->pitch,
                        (yaw - old->yaw) / (sysTime - old->time),
                        (smoothView.pitch - old->pitch) / (sysTime - old->time));
            old->yaw = yaw;
            old->pitch = smoothView.pitch;
            old->time = sysTime;
        }

        // The Rdx and Rdy should stay constant when moving.
        if(showViewPosDeltas)
        {
            typedef struct oldpos_s {
                double           time;
                float            x, y, z;
            } oldpos_t;

            static oldpos_t         oldpos[DDMAXPLAYERS];
            oldpos_t*               old = &oldpos[viewPlayer - ddPlayers];

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f dz=%-10.3f dx/dt=%-10.3f\n",
                        //"Rdx=%-10.3f Rdy=%-10.3f\n",
                        SECONDS_TO_TICKS(gameTime),
                        frameTimePos,
                        sysTime - old->time,
                        smoothView.pos[0] - old->x,
                        smoothView.pos[1] - old->y,
                        smoothView.pos[2] - old->z,
                        (smoothView.pos[0] - old->x) / (sysTime - old->time) /*,
                        smoothView.pos[1] - old->y / (sysTime - old->time)*/);
            old->x = smoothView.pos[VX];
            old->y = smoothView.pos[VY];
            old->z = smoothView.pos[VZ];
            old->time = sysTime;
        }
    }

    // Update viewer.
    tableAngle = vd->current.angle >> ANGLETOFINESHIFT;
    vd->viewSin = FIX2FLT(finesine[tableAngle]);
    vd->viewCos = FIX2FLT(fineCosine[tableAngle]);

    // Calculate the front, up and side unit vectors.
    // The vectors are in the DGL coordinate system, which is a left-handed
    // one (same as in the game, but Y and Z have been swapped). Anyone
    // who uses these must note that it might be necessary to fix the aspect
    // ratio of the Y axis by dividing the Y coordinate by 1.2.
    yawRad = ((vd->current.angle / (float) ANGLE_MAX) *2) * PI;

    pitchRad = vd->current.pitch * 85 / 110.f / 180 * PI;

    // The front vector.
    vd->frontVec[VX] = cos(yawRad) * cos(pitchRad);
    vd->frontVec[VZ] = sin(yawRad) * cos(pitchRad);
    vd->frontVec[VY] = sin(pitchRad);

    // The up vector.
    vd->upVec[VX] = -cos(yawRad) * sin(pitchRad);
    vd->upVec[VZ] = -sin(yawRad) * sin(pitchRad);
    vd->upVec[VY] = cos(pitchRad);

    // The side vector is the cross product of the front and up vectors.
    M_CrossProduct(vd->frontVec, vd->upVec, vd->sideVec);

    if(showFrameTimePos)
    {
        Con_Printf("frametime = %f\n", frameTimePos);
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
    extraLight = player->extraLight;
    extraLightDelta = extraLight / 16.0f;

    // Why?
    validCount++;

#undef MINEXTRALIGHTFRAMES
#undef VIEWPOS_MAX_SMOOTHDISTANCE
}

/**
 * Draw the border around the view window.
 */
void R_RenderPlayerViewBorder(void)
{
    R_DrawViewBorder();
}

/**
 * Set the GL viewport.
 */
void R_UseViewPort(viewport_t* port)
{
    if(!port)
    {
        glViewport(0, FLIP(0 + theWindow->height - 1), theWindow->width, theWindow->height);
    }
    else
    {
        currentPort = port;
        glViewport(port->x, FLIP(port->y + port->height - 1), port->width, port->height);
    }
}

/**
 * Render a blank view for the specified player.
 */
void R_RenderBlankView(void)
{
    UI_DrawDDBackground(0, 0, 320, 200, 1);
}

/**
 * Draw the view of the player inside the view window.
 */
void R_RenderPlayerView(int num)
{
    extern boolean      firstFrameAfterLoad;
    extern int          psp3d, modelTriCount;

    int                 oldFlags = 0;
    player_t*           player;

    if(num < 0 || num >= DDMAXPLAYERS)
        return; // Huh?
    player = &ddPlayers[num];

    if(!player->shared.inGame || !player->shared.mo)
        return;

    if(firstFrameAfterLoad)
    {
        // Don't let the clock run yet.  There may be some texture
        // loading still left to do that we have been unable to
        // predetermine.
        firstFrameAfterLoad = false;
        DD_ResetTimer();
    }

    // Setup for rendering the frame.
    R_SetupFrame(player);
    if(!freezeRLs)
        R_ClearSprites();

    R_ProjectPlayerSprites(); // Only if 3D models exists for them.

    // Hide the viewPlayer's mobj?
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        oldFlags = player->shared.mo->ddFlags;
        player->shared.mo->ddFlags |= DDMF_DONTDRAW;
    }
    // Go to wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // GL is in 3D transformation state only during the frame.
    GL_SwitchTo3DState(true, currentPort);
    Rend_RenderMap();
    // Orthogonal projection to the view window.
    GL_Restore2DState(1);

    // Don't render in wireframe mode with 2D psprites.
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    Rend_Draw2DPlayerSprites(); // If the 2D versions are needed.
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Fullscreen viewport.
    GL_Restore2DState(2);
    // Do we need to render any 3D psprites?
    if(psp3d)
    {
        GL_SwitchTo3DState(false, currentPort);
        Rend_Draw3DPlayerSprites();
        GL_Restore2DState(2);   // Restore viewport.
    }
    // Original matrices and state: back to normal 2D.
    GL_Restore2DState(3);

    // Back from wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Now we can show the viewPlayer's mobj again.
    if(!(player->shared.flags & DDPF_CHASECAM))
        player->shared.mo->ddFlags = oldFlags;

    // Should we be counting triangles?
    if(rendInfoTris)
    {
        // This count includes all triangles drawn since R_SetupFrame.
        Con_Printf("Tris: %-4i (Mdl=%-4i)\n", polyCounter, modelTriCount);
        modelTriCount = 0;
        polyCounter = 0;
    }

    if(rendInfoLums)
    {
        Con_Printf("LumObjs: %-4i\n", LO_GetNumLuminous());
    }

    R_InfoRendVerticesPool();

    // The colored filter.
    GL_DrawFilter();
}

/**
 * Should be called when returning from a game-side drawing method to ensure
 * that our assumptions of the GL state are valid. This is necessary because
 * DGL affords the user the posibility of modifiying the GL state.
 *
 * Todo: A cleaner approach would be a DGL state stack which could simply pop.
 */
static void restoreDefaultGLState(void)
{
    // Here we use the DGL methods as this ensures it's state is kept in sync.
    DGL_Disable(DGL_FOG);
    DGL_Disable(DGL_SCISSOR_TEST);
    DGL_Enable(DGL_TEXTURING);
    DGL_Enable(DGL_LINE_SMOOTH);
    DGL_Enable(DGL_POINT_SMOOTH);
}

/**
 * Render all view ports in the viewport grid.
 */
void R_RenderViewPorts(void)
{
    int                 oldDisplay = displayPlayer;
    int                 x, y, p;
    GLbitfield          bits = GL_DEPTH_BUFFER_BIT;

    if(/*firstFrameAfterLoad ||*/ freezeRLs)
    {
        bits |= GL_COLOR_BUFFER_BIT;
    }
    else
    {
        int                 i;

        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];

            if(!plr->shared.inGame || !(plr->shared.flags & DDPF_LOCAL))
                continue;

            if(P_IsInVoid(plr))
            {
                bits |= GL_COLOR_BUFFER_BIT;
                break;
            }
        }
    }

    // This is all the clearing we'll do.
    glClear(bits);

    // Draw a view for all players with a visible viewport.
    for(p = 0, y = 0; y < gridRows; ++y)
        for(x = 0; x < gridCols; x++, ++p)
        {
            displayPlayer = viewports[p].console;
            R_UseViewPort(viewports + p);

            if(displayPlayer < 0 || ddPlayers[displayPlayer].shared.flags & DDPF_UNDEFINED_POS)
            {
                R_RenderBlankView();
                continue;
            }

            // Draw in-window game graphics (layer 0).
            gx.G_Drawer(0);
            restoreDefaultGLState();

            // Draw the view border.
            R_RenderPlayerViewBorder();

            // Draw in-window game graphics (layer 1).
            gx.G_Drawer(1);
            restoreDefaultGLState();

            // Increment the internal frame count. This does not
            // affect the FPS counter.
            frameCount++;
        }

    // Restore things back to normal.
    displayPlayer = oldDisplay;
    R_UseViewPort(NULL);
}

D_CMD(ViewGrid)
{
    if(argc != 3)
    {
        Con_Printf("Usage: %s (cols) (rows)\n", argv[0]);
        return true;
    }

    // Recalculate viewports.
    R_SetViewGrid(strtol(argv[1], NULL, 0), strtol(argv[2], NULL, 0));
    return true;
}
