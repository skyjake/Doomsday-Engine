/** @file r_main.cpp
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cmath>
#include <cstring>

#ifdef __CLIENT__
#include <QBitArray>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_resource.h"
#include "de_network.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_ui.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "edit_bias.h"
#  include "api_render.h"
#  include "render/r_main.h"
#  include "render/vignette.h"
#  include <de/GLState>
#endif
#include "gl/svg.h"

#include "world/linesighttest.h"
#include "world/p_players.h"
#include "Contact"
#include "world/thinkers.h"
#include "BspLeaf"
#include "Surface"

using namespace de;

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
float devCameraMovementStartTime = 0; // sysTime
float devCameraMovementStartTimeRealSecs = 0;
#endif

D_CMD(ViewGrid);

int validCount = 1; // Increment every time a check is made.
int frameCount; // Just for profiling purposes.
int rendInfoTris = 0;

boolean firstFrameAfterLoad;

// Precalculated math tables.
fixed_t *fineCosine = &finesine[FINEANGLES / 4];

float frameTimePos; // 0...1: fractional part for sharp game tics.

int loadInStartupMode = false;

byte precacheSkins = true;
byte precacheMapMaterials = true;
byte precacheSprites = true;
byte texGammaLut[256];

fontid_t fontFixed, fontVariable[FONTSTYLE_COUNT];

static int resetNextViewer = true;

static viewdata_t viewDataOfConsole[DDMAXPLAYERS]; // Indexed by console number.

static byte showFrameTimePos = false;
static byte showViewAngleDeltas = false;
static byte showViewPosDeltas = false;

static int gridCols, gridRows;
static viewport_t viewportOfLocalPlayer[DDMAXPLAYERS];

#ifdef __CLIENT__
static int rendCameraSmooth = true; // Smoothed by default.
static viewport_t *currentViewport;

boolean loInited;

static coord_t *luminousDist;
static byte *luminousClipped;
static uint *luminousOrder;
static QBitArray bspLeafsVisible;
#endif

int psp3d;
float pspLightLevelMultiplier = 1;
float pspOffset[2];
int levelFullBright;
int weaponOffsetScaleY = 1000;

/*
 * Console variables:
 */
float weaponFOVShift    = 45;
float weaponOffsetScale = 0.3183f; // 1/Pi
byte weaponScaleMode    = SCALEMODE_SMART_STRETCH;

void R_Register()
{
#ifdef __CLIENT__
    C_VAR_INT ("con-show-during-setup",     &loadInStartupMode,     0, 0, 1);

    C_VAR_INT ("rend-camera-smooth",        &rendCameraSmooth,      CVF_HIDE, 0, 1);

    C_VAR_BYTE("rend-info-deltas-angles",   &showViewAngleDeltas,   0, 0, 1);
    C_VAR_BYTE("rend-info-deltas-pos",      &showViewPosDeltas,     0, 0, 1);
    C_VAR_BYTE("rend-info-frametime",       &showFrameTimePos,      0, 0, 1);
    C_VAR_BYTE("rend-info-rendpolys",       &rendInfoRPolys,        CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT ("rend-info-tris",            &rendInfoTris,          0, 0, 1);

    C_CMD("viewgrid", "ii", ViewGrid);
#endif
}

void R_BuildTexGammaLut()
{
#ifdef __SERVER__
    double invGamma = 1.0f;
#else
    double invGamma = 1.0f - MINMAX_OF(0, texGamma, 1); // Clamp to a sane range.
#endif

    for(int i = 0; i < 256; ++i)
    {
        texGammaLut[i] = byte(255.0f * pow(double(i / 255.0f), invGamma));
    }
}

#ifdef __CLIENT__
char const *R_ChooseFixedFont()
{
    if(DENG_GAMEVIEW_WIDTH < 300) return "console11";
    if(DENG_GAMEVIEW_WIDTH > 768) return "console18";
    return "console14";
}
#endif

char const *R_ChooseVariableFont(fontstyle_t style, int resX, int resY)
{
    DENG_UNUSED(resX);

    int const SMALL_LIMIT = 500;
    int const MED_LIMIT   = 800;

    switch(style)
    {
    default:
        return (resY < SMALL_LIMIT ? "normal12" :
                resY < MED_LIMIT   ? "normal18" :
                                     "normal24");

    case FS_LIGHT:
        return (resY < SMALL_LIMIT ? "normallight12" :
                resY < MED_LIMIT   ? "normallight18" :
                                     "normallight24");

    case FS_BOLD:
        return (resY < SMALL_LIMIT ? "normalbold12" :
                resY < MED_LIMIT   ? "normalbold18" :
                                     "normalbold24");
    }
}

#ifdef __CLIENT__
static AbstractFont *loadSystemFont(char const *name)
{
    DENG2_ASSERT(name != 0 && name[0]);

    // Compose the resource name.
    de::Uri uri = de::Uri("System:", RC_NULL).setPath(name);

    // Compose the resource data path.
    ddstring_t resourcePath; Str_InitStd(&resourcePath);
    Str_Appendf(&resourcePath, "}data/Fonts/%s.dfn", name);
#if defined(UNIX) && !defined(MACOSX)
    // Case-sensitive file system.
    /// @todo Unkludge this: handle in a more generic manner.
    strlwr(resourcePath.str);
#endif
    F_ExpandBasePath(&resourcePath, &resourcePath);

    AbstractFont *font = App_ResourceSystem().createFontFromFile(uri, Str_Text(&resourcePath));
    if(!font)
    {
        Con_Error("loadSystemFont: Failed loading font \"%s\".", name);
        exit(1); // Unreachable.
    }

    Str_Free(&resourcePath);
    return font;
}

static void loadFontIfNeeded(char const *uri, fontid_t *fid)
{
    *fid = NOFONTID;
    if(uri && uri[0])
    {
        try
        {
            FontManifest &manifest = App_ResourceSystem().findFont(de::Uri(uri, RC_NULL));
            if(manifest.hasResource())
            {
                *fid = fontid_t(manifest.uniqueId());
            }
        }
        catch(ResourceSystem::MissingManifestError const &)
        {}
    }

    if(*fid == NOFONTID)
    {
        *fid = loadSystemFont(uri)->manifest().uniqueId();
    }
}
#endif // __CLIENT__

void R_LoadSystemFonts()
{
#ifdef __CLIENT__

    if(isDedicated) return;

    loadFontIfNeeded(R_ChooseFixedFont(), &fontFixed);
    loadFontIfNeeded(R_ChooseVariableFont(FS_NORMAL, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT), &fontVariable[FS_NORMAL]);
    loadFontIfNeeded(R_ChooseVariableFont(FS_BOLD,   DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT), &fontVariable[FS_BOLD]);
    loadFontIfNeeded(R_ChooseVariableFont(FS_LIGHT,  DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT), &fontVariable[FS_LIGHT]);

    //Con_SetFont(fontFixed);

#endif
}

#undef R_SetViewOrigin
DENG_EXTERN_C void R_SetViewOrigin(int consoleNum, coord_t const origin[3])
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    V3d_Copy(viewDataOfConsole[consoleNum].latest.origin, origin);
}

#undef R_SetViewAngle
DENG_EXTERN_C void R_SetViewAngle(int consoleNum, angle_t angle)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.angle = angle;
}

#undef R_SetViewPitch
DENG_EXTERN_C void R_SetViewPitch(int consoleNum, float pitch)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.pitch = pitch;
}

void R_SetupDefaultViewWindow(int consoleNum)
{
#ifdef __CLIENT__
    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;

    vd->window.origin.x = vd->windowOld.origin.x = vd->windowTarget.origin.x = 0;
    vd->window.origin.y = vd->windowOld.origin.y = vd->windowTarget.origin.y = 0;
    vd->window.size.width  = vd->windowOld.size.width  = vd->windowTarget.size.width  = DENG_GAMEVIEW_WIDTH;
    vd->window.size.height = vd->windowOld.size.height = vd->windowTarget.size.height = DENG_GAMEVIEW_HEIGHT;
    vd->windowInter = 1;
#endif
}

void R_ViewWindowTicker(int consoleNum, timespan_t ticLength)
{
#define LERP(start, end, pos) (end * pos + start * (1 - pos))

    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;

    vd->windowInter += float(.4 * ticLength * TICRATE);
    if(vd->windowInter >= 1)
    {
        std::memcpy(&vd->window, &vd->windowTarget, sizeof(vd->window));
    }
    else
    {
        float const x = LERP(vd->windowOld.origin.x, vd->windowTarget.origin.x, vd->windowInter);
        float const y = LERP(vd->windowOld.origin.y, vd->windowTarget.origin.y, vd->windowInter);
        float const w = LERP(vd->windowOld.size.width,  vd->windowTarget.size.width,  vd->windowInter);
        float const h = LERP(vd->windowOld.size.height, vd->windowTarget.size.height, vd->windowInter);
        vd->window.origin.x = ROUND(x);
        vd->window.origin.y = ROUND(y);
        vd->window.size.width  = ROUND(w);
        vd->window.size.height = ROUND(h);
    }

#undef LERP
}

#undef R_ViewWindowGeometry
DENG_EXTERN_C int R_ViewWindowGeometry(int player, RectRaw *geometry)
{
    if(!geometry) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const *vd = &viewDataOfConsole[player];
    std::memcpy(geometry, &vd->window, sizeof *geometry);
    return true;
}

#undef R_ViewWindowOrigin
DENG_EXTERN_C int R_ViewWindowOrigin(int player, Point2Raw *origin)
{
    if(!origin) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const *vd = &viewDataOfConsole[player];
    std::memcpy(origin, &vd->window.origin, sizeof *origin);
    return true;
}

#undef R_ViewWindowSize
DENG_EXTERN_C int R_ViewWindowSize(int player, Size2Raw* size)
{
    if(!size) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const *vd = &viewDataOfConsole[player];
    std::memcpy(size, &vd->window.size, sizeof *size);
    return true;
}

/**
 * @note Do not change values used during refresh here because we might be
 * partway through rendering a frame. Changes should take effect on next
 * refresh only.
 */
#undef R_SetViewWindowGeometry
DENG_EXTERN_C void R_SetViewWindowGeometry(int player, RectRaw const *geometry, boolean interpolate)
{
    int p = P_ConsoleToLocal(player);
    if(p < 0) return;

    viewport_t const *vp = &viewportOfLocalPlayer[p];
    viewdata_t *vd = &viewDataOfConsole[player];
    RectRaw newGeom;

    // Clamp to valid range.
    newGeom.origin.x = MINMAX_OF(0, geometry->origin.x, vp->geometry.size.width);
    newGeom.origin.y = MINMAX_OF(0, geometry->origin.y, vp->geometry.size.height);
    newGeom.size.width  = abs(geometry->size.width);
    newGeom.size.height = abs(geometry->size.height);
    if(newGeom.origin.x + newGeom.size.width > vp->geometry.size.width)
        newGeom.size.width = vp->geometry.size.width - newGeom.origin.x;
    if(newGeom.origin.y + newGeom.size.height > vp->geometry.size.height)
        newGeom.size.height = vp->geometry.size.height - newGeom.origin.y;

    // Already at this target?
    if(vd->window.origin.x    == newGeom.origin.x &&
       vd->window.origin.y    == newGeom.origin.y &&
       vd->window.size.width  == newGeom.size.width &&
       vd->window.size.height == newGeom.size.height)
        return;

    // Record the new target.
    std::memcpy(&vd->windowTarget, &newGeom, sizeof vd->windowTarget);

    // Restart or advance the interpolation timer?
    // If dimensions have not yet been set - do not interpolate.
    if(interpolate && !(vd->window.size.width == 0 && vd->window.size.height == 0))
    {
        vd->windowInter = 0;
        std::memcpy(&vd->windowOld, &vd->window, sizeof(vd->windowOld));
    }
    else
    {
        vd->windowInter = 1; // Update on next frame.
        std::memcpy(&vd->windowOld, &vd->windowTarget, sizeof(vd->windowOld));
    }
}

#undef R_ViewPortGeometry
DENG_EXTERN_C int R_ViewPortGeometry(int player, RectRaw *geometry)
{
    if(!geometry) return false;

    int p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t* vp = &viewportOfLocalPlayer[p];
    std::memcpy(geometry, &vp->geometry, sizeof *geometry);
    return true;
}

#undef R_ViewPortOrigin
DENG_EXTERN_C int R_ViewPortOrigin(int player, Point2Raw *origin)
{
    if(!origin) return false;

    int p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t *vp = &viewportOfLocalPlayer[p];
    std::memcpy(origin, &vp->geometry.origin, sizeof *origin);
    return true;
}

#undef R_ViewPortSize
DENG_EXTERN_C int R_ViewPortSize(int player, Size2Raw *size)
{
    if(!size) return false;

    int p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t *vp = &viewportOfLocalPlayer[p];
    std::memcpy(size, &vp->geometry.size, sizeof *size);
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
#ifdef __CLIENT__
    DENG_ASSERT(port);

    RectRaw *rect = &port->geometry;
    int const x = DENG_GAMEVIEW_X + col * DENG_GAMEVIEW_WIDTH  / gridCols;
    int const y = DENG_GAMEVIEW_Y + row * DENG_GAMEVIEW_HEIGHT / gridRows;
    int const width  = (col+1) * DENG_GAMEVIEW_WIDTH  / gridCols - x;
    int const height = (row+1) * DENG_GAMEVIEW_HEIGHT / gridRows - y;
    ddhook_viewport_reshape_t p;
    bool doReshape = false;

    if(rect->origin.x == x && rect->origin.y == y &&
       rect->size.width == width && rect->size.height == height)
        return;

    if(port->console != -1 && Plug_CheckForHook(HOOK_VIEWPORT_RESHAPE))
    {
        std::memcpy(&p.oldGeometry, rect, sizeof(p.oldGeometry));
        doReshape = true;
    }

    rect->origin.x = x;
    rect->origin.y = y;
    rect->size.width  = width;
    rect->size.height = height;

    if(doReshape)
    {
        std::memcpy(&p.geometry, rect, sizeof(p.geometry));
        DD_CallHooks(HOOK_VIEWPORT_RESHAPE, port->console, (void*)&p);
    }
#endif // __CLIENT__
}

boolean R_SetViewGrid(int numCols, int numRows)
{
    int x, y, p, console;

    if(numCols > 0 && numRows > 0)
    {
        if(numCols * numRows > DDMAXPLAYERS)
            return false;

        if(numCols > DDMAXPLAYERS)
            numCols = DDMAXPLAYERS;
        if(numRows > DDMAXPLAYERS)
            numRows = DDMAXPLAYERS;

        gridCols = numCols;
        gridRows = numRows;
    }

    p = 0;
    for(y = 0; y < gridRows; ++y)
    {
        for(x = 0; x < gridCols; ++x)
        {
            // The console number is -1 if the viewport belongs to no one.
            viewport_t *vp = viewportOfLocalPlayer + p;

            console = P_LocalToConsole(p);
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
    }

    return true;
}

void R_Init()
{
    R_LoadSystemFonts();
    R_InitTranslationTables();
    R_InitRawTexs();
    R_InitSvgs();
#ifdef __CLIENT__
    R_InitViewWindow();
    Rend_Init();
#endif
    frameCount = 0;
}

void R_Update()
{
    // Reset file IDs so previously seen files can be processed again.
    F_ResetFileIds();
    // Re-read definitions.
    Def_Read();

    R_UpdateRawTexs();
    App_ResourceSystem().initSprites(); // Fully reinitialize sprites.
    Models_Init(); // Defs might've changed.

    R_UpdateTranslationTables();

    Def_PostInit();

    App_World().update();

#ifdef __CLIENT__
    // Recalculate the light range mod matrix.
    Rend_UpdateLightModMatrix();

    // The rendering lists have persistent data that has changed during the
    // re-initialization.
    ClientApp::renderSystem().clearDrawLists();

    /// @todo fixme: Update the game title and the status.
#endif

#ifdef DENG_DEBUG
    Z_CheckHeap();
#endif
}

void R_Shutdown()
{
    App_ResourceSystem().clearAllSprites();
    Models_Shutdown();
    R_ShutdownSvgs();
#ifdef __CLIENT__
    R_ShutdownViewWindow();
    Rend_Shutdown();
#endif
}

void R_Ticker(timespan_t time)
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        R_ViewWindowTicker(i, time);
    }
}

void R_ResetViewer()
{
    resetNextViewer = 1;
}

int R_NextViewer()
{
    return resetNextViewer;
}

void R_InterpolateViewer(viewer_t *start, viewer_t *end, float pos, viewer_t *out)
{
    float inv = 1 - pos;
    int delta;

    out->origin[VX] = inv * start->origin[VX] + pos * end->origin[VX];
    out->origin[VY] = inv * start->origin[VY] + pos * end->origin[VY];
    out->origin[VZ] = inv * start->origin[VZ] + pos * end->origin[VZ];

    delta = int(end->angle) - int(start->angle);
    out->angle = start->angle + int(pos * delta);
    out->pitch = inv * start->pitch + pos * end->pitch;
}

void R_CopyViewer(viewer_t *dst, viewer_t const *src)
{
    V3d_Copy(dst->origin, src->origin);
    dst->angle = src->angle;
    dst->pitch = src->pitch;
}

viewdata_t const *R_ViewData(int consoleNum)
{
    DENG_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);
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
    if(fabs(dst->origin[VX] - src->origin[VX]) > MAXMOVE ||
       fabs(dst->origin[VY] - src->origin[VY]) > MAXMOVE)
    {
        V3d_Copy(src->origin, dst->origin);
    }

    if(abs(int(dst->angle) - int(src->angle)) >= ANGLE_45)
    {
#ifdef _DEBUG
        Con_Message("R_CheckViewerLimits: Snap camera angle to %08x.", dst->angle);
#endif
        src->angle = dst->angle;
    }
}

/**
 * Retrieve the current sharp camera position.
 */
void R_GetSharpView(viewer_t *view, player_t *player)
{
    if(!player || !player->shared.mo) return;

    ddplayer_t *ddpl = &player->shared;
    viewdata_t *vd = &viewDataOfConsole[player - ddPlayers];

    R_CopyViewer(view, &vd->latest);

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

        view->origin[VX] -= distance * FIX2FLT(fineCosine[angle]);
        view->origin[VY] -= distance * FIX2FLT(finesine[angle]);
        view->origin[VZ] -= distance * FIX2FLT(finesine[pitch]);
    }

    // Check that the viewZ doesn't go too high or low.
    // Cameras are not restricted.
    if(!(ddpl->flags & DDPF_CAMERA))
    {
        if(view->origin[VZ] > ddpl->mo->ceilingZ - 4)
        {
            view->origin[VZ] = ddpl->mo->ceilingZ - 4;
        }

        if(view->origin[VZ] < ddpl->mo->floorZ + 4)
        {
            view->origin[VZ] = ddpl->mo->floorZ + 4;
        }
    }
}

void R_NewSharpWorld()
{
    if(resetNextViewer)
        resetNextViewer = 2;

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        viewer_t sharpView;
        viewdata_t *vd = &viewDataOfConsole[i];
        player_t *plr = &ddPlayers[i];

        if(/*(plr->shared.flags & DDPF_LOCAL) &&*/
           (!plr->shared.inGame || !plr->shared.mo))
        {
            continue;
        }

        R_GetSharpView(&sharpView, plr);

        // The game tic has changed, which means we have an updated sharp
        // camera position.  However, the position is at the beginning of
        // the tic and we are most likely not at a sharp tic boundary, in
        // time.  We will move the viewer positions one step back in the
        // buffer.  The effect of this is that [0] is the previous sharp
        // position and [1] is the current one.

        std::memcpy(&vd->lastSharp[0], &vd->lastSharp[1], sizeof(viewer_t));
        std::memcpy(&vd->lastSharp[1], &sharpView, sizeof(sharpView));

        R_CheckViewerLimits(vd->lastSharp, &sharpView);
    }

#ifdef __CLIENT__
    if(App_World().hasMap())
    {
        Map &map = App_World().map();
        map.updateTrackedPlanes();
        map.updateScrollingSurfaces();
    }
#endif
}

void R_UpdateViewer(int consoleNum)
{
    DENG_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);

    int const VIEWPOS_MAX_SMOOTHDISTANCE = 172;

    viewdata_t *vd   = viewDataOfConsole + consoleNum;
    player_t *player = ddPlayers + consoleNum;

    if(!player->shared.inGame || !player->shared.mo) return;

    viewer_t sharpView;
    R_GetSharpView(&sharpView, player);

    if(resetNextViewer ||
       V3d_Distance(vd->current.origin, sharpView.origin) > VIEWPOS_MAX_SMOOTHDISTANCE)
    {
        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
            resetNextViewer = 0;

        // Just view from the sharp position.
        R_CopyViewer(&vd->current, &sharpView);

        std::memcpy(&vd->lastSharp[0], &sharpView, sizeof(sharpView));
        std::memcpy(&vd->lastSharp[1], &sharpView, sizeof(sharpView));
    }
    // While the game is paused there is no need to calculate any
    // time offsets or interpolated camera positions.
    else //if(!clientPaused)
    {
        // Calculate the smoothed camera position, which is somewhere between
        // the previous and current sharp positions. This introduces a slight
        // delay (max. 1/35 sec) to the movement of the smoothed camera.
        viewer_t smoothView;
        R_InterpolateViewer(vd->lastSharp, vd->lastSharp + 1, frameTimePos, &smoothView);

        // Use the latest view angles known to us, if the interpolation flags
        // are not set. The interpolation flags are used when the view angles
        // are updated during the sharp tics and need to be smoothed out here.
        // For example, view locking (dead or camera setlock).
        /*if(!(player->shared.flags & DDPF_INTERYAW))
            smoothView.angle = sharpView.angle;*/
        /*if(!(player->shared.flags & DDPF_INTERPITCH))
            smoothView.pitch = sharpView.pitch;*/

        R_CopyViewer(&vd->current, &smoothView);

        // Monitor smoothness of yaw/pitch changes.
        if(showViewAngleDeltas)
        {
            typedef struct oldangle_s {
                double time;
                float yaw, pitch;
            } oldangle_t;

            static oldangle_t oldangle[DDMAXPLAYERS];
            oldangle_t *old = &oldangle[viewPlayer - ddPlayers];
            float yaw = (double)smoothView.angle / ANGLE_MAX * 360;

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f "
                        "Rdx=%-10.3f Rdy=%-10.3f",
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
            struct OldPos
            {
                double time;
                float x, y, z;
            };

            static OldPos oldpos[DDMAXPLAYERS];
            OldPos *old = &oldpos[viewPlayer - ddPlayers];

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f dz=%-10.3f dx/dt=%-10.3f dy/dt=%-10.3f",
                        //"Rdx=%-10.3f Rdy=%-10.3f\n",
                        SECONDS_TO_TICKS(gameTime),
                        frameTimePos,
                        sysTime - old->time,
                        smoothView.origin[0] - old->x,
                        smoothView.origin[1] - old->y,
                        smoothView.origin[2] - old->z,
                        (smoothView.origin[0] - old->x) / (sysTime - old->time),
                        (smoothView.origin[1] - old->y) / (sysTime - old->time));

            old->x = smoothView.origin[VX];
            old->y = smoothView.origin[VY];
            old->z = smoothView.origin[VZ];
            old->time = sysTime;
        }
    }

    // Update viewer.
    uint an = vd->current.angle >> ANGLETOFINESHIFT;
    vd->viewSin = FIX2FLT(finesine[an]);
    vd->viewCos = FIX2FLT(fineCosine[an]);

    // Calculate the front, up and side unit vectors.
    float yawRad = ((vd->current.angle / (float) ANGLE_MAX) *2) * PI;
    float pitchRad = vd->current.pitch * 85 / 110.f / 180 * PI;

    // The front vector.
    vd->frontVec[VX] = cos(yawRad) * cos(pitchRad);
    vd->frontVec[VZ] = sin(yawRad) * cos(pitchRad);
    vd->frontVec[VY] = sin(pitchRad);

    // The up vector.
    vd->upVec[VX] = -cos(yawRad) * sin(pitchRad);
    vd->upVec[VZ] = -sin(yawRad) * sin(pitchRad);
    vd->upVec[VY] = cos(pitchRad);

    // The side vector is the cross product of the front and up vectors.
    V3f_CrossProduct(vd->sideVec, vd->frontVec, vd->upVec);
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

    // Why?
    validCount++;

#ifdef __CLIENT__
    extraLight = player->extraLight;
    extraLightDelta = extraLight / 16.0f;

    if(!freezeRLs)
    {
        R_ClearVisSprites();
    }
#endif

#undef MINEXTRALIGHTFRAMES
}

#ifdef __CLIENT__
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
        currentViewport = NULL;
        ClientWindow::main().game().glApplyViewport(DENG_GAMEVIEW_X,
                                                    DENG_GAMEVIEW_Y,
                                                    DENG_GAMEVIEW_WIDTH,
                                                    DENG_GAMEVIEW_HEIGHT);
    }
    else
    {
        currentViewport = const_cast<viewport_t *>(vp);
        ClientWindow::main().game().glApplyViewport(vp->geometry.origin.x,
                                                    vp->geometry.origin.y,
                                                    vp->geometry.size.width,
                                                    vp->geometry.size.height);
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
    boolean isFullBright = (levelFullBright != 0);
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
        modeldef_t *mf = 0, *nextmf = 0;
        float inter = 0;
        if(useModels)
        {
            // Is there a model for this frame?
            mobj_t dummy;

            // Setup a dummy for the call to R_CheckModelFor.
            dummy.state = psp->statePtr;
            dummy.tics = psp->tics;

            inter = Models_ModelDefForMobj(&dummy, &mf, &nextmf);
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
            spr->data.model.gzt = viewData->current.origin[VZ];
            spr->data.model.secFloor = cluster.visFloor().heightSmoothed();
            spr->data.model.secCeil  = cluster.visCeiling().heightSmoothed();
            spr->data.model.pClass = 0;
            spr->data.model.floorClip = 0;

            spr->data.model.mf = mf;
            spr->data.model.nextMF = nextmf;
            spr->data.model.inter = inter;
            spr->data.model.viewAligned = true;
            spr->origin[VX] = viewData->current.origin[VX];
            spr->origin[VY] = viewData->current.origin[VY];
            spr->origin[VZ] = viewData->current.origin[VZ];

            // Offsets to rotation angles.
            spr->data.model.yawAngleOffset = psp->pos[VX] * weaponOffsetScale - 90;
            spr->data.model.pitchAngleOffset =
                (32 - psp->pos[VY]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
            // Is the FOV shift in effect?
            if(weaponFOVShift > 0 && Rend_FieldOfView() > 90)
                spr->data.model.pitchAngleOffset -= weaponFOVShift * (Rend_FieldOfView() - 90) / 90;
            // Real rotation angles.
            spr->data.model.yaw =
                viewData->current.angle / (float) ANGLE_MAX *-360 + spr->data.model.yawAngleOffset + 90;
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
            spr->origin[VX] = viewData->current.origin[VX];
            spr->origin[VY] = viewData->current.origin[VY];
            spr->origin[VZ] = viewData->current.origin[VZ];

            spr->data.sprite.bspLeaf = &Mobj_BspLeafAtOrigin(*mo);
            spr->data.sprite.alpha = psp->alpha;
            spr->data.sprite.isFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
        }
    }
}

static void setupPSpriteParams(rendpspriteparams_t *params, vispsprite_t *spr)
{
    ddpsprite_t *psp      = spr->psp;
    int const spriteIdx   = psp->statePtr->sprite;
    int const frameIdx    = psp->statePtr->frame;
    float const offScaleY = weaponOffsetScaleY / 1000.0f;

    SpriteViewAngle const &sprViewAngle = App_ResourceSystem().spritePtr(spriteIdx, frameIdx)->viewAngle(0);
    Material *material = sprViewAngle.material;
    bool flip          = sprViewAngle.mirrorX;

    MaterialVariantSpec const &spec =
        App_ResourceSystem().materialSpec(PSpriteContext, 0, 1, 0, 0,
                                          GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                          0, -2, 0, false, true, true, false);
    MaterialSnapshot const &ms = material->prepare(spec);

    Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
    variantspecification_t const &texSpec = ms.texture(MTU_PRIMARY).spec().variant;

#define WEAPONTOP   32   /// @todo Currently hardcoded here and in the plugins.

    params->pos[VX] = psp->pos[VX] + tex.origin().x + pspOffset[VX] - texSpec.border;
    params->pos[VY] = WEAPONTOP + offScaleY * (psp->pos[VY] - WEAPONTOP) + tex.origin().y +
                      pspOffset[VY] - texSpec.border;
    params->width  = ms.width() + texSpec.border*2;
    params->height = ms.height() + texSpec.border*2;

    ms.texture(MTU_PRIMARY).glCoords(&params->texOffset[0], &params->texOffset[1]);

    params->texFlip[0] = flip;
    params->texFlip[1] = false;

    params->mat = material;
    params->ambientColor[CA] = spr->data.sprite.alpha;

    if(spr->data.sprite.isFullBright)
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        Map &map = App_World().map();

        if(useBias && map.hasLightGrid())
        {
            // Evaluate the position in the light grid.
            Vector3f tmp = map.lightGrid().evaluate(spr->origin);
            V3f_Set(params->ambientColor, tmp.x, tmp.y, tmp.z);
        }
        else
        {
            SectorCluster &cluster = spr->data.sprite.bspLeaf->cluster();
            Vector3f const &secColor = Rend_SectorLightColor(cluster);

            // No need for distance attentuation.
            float lightLevel = cluster.sector().lightLevel();

            // Add extra light plus bonus.
            lightLevel += Rend_ExtraLightDelta();
            lightLevel *= pspLightLevelMultiplier;

            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor in affect.
            for(int i = 0; i < 3; ++i)
            {
                params->ambientColor[i] = lightLevel * secColor[i];
            }
        }

        Rend_ApplyTorchLight(params->ambientColor, 0);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(spr->origin);
        lparams.bspLeaf      = spr->data.sprite.bspLeaf;
        lparams.ambientColor = Vector3f(params->ambientColor);

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }

#undef WEAPONTOP
}

/**
 * Draws 2D HUD sprites. If they were already drawn 3D, this won't do anything.
 */
static void drawPlayerSprites()
{
    ddplayer_t *ddpl = &viewPlayer->shared;

    // Cameramen have no HUD sprites.
    if(ddpl->flags & DDPF_CAMERA) return;
    if(ddpl->flags & DDPF_CHASECAM) return;

    if(usingFog)
    {
        glEnable(GL_FOG);
    }

    // Check for fullbright.
    int i;
    ddpsprite_t *psp;
    for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t *spr = &visPSprites[i];

        // Should this psprite be drawn?
        if(spr->type != VPSPR_SPRITE) continue; // No...

        // Draw as separate sprites.
        if(spr->psp && spr->psp->statePtr)
        {
            rendpspriteparams_t params;

            setupPSpriteParams(&params, spr);
            Rend_DrawPSprite(params);
        }
    }

    if(usingFog)
    {
        glDisable(GL_FOG);
    }
}

static void setupModelParamsForVisPSprite(rendmodelparams_t *params, vispsprite_t *spr)
{
    params->mf = spr->data.model.mf;
    params->nextMF = spr->data.model.nextMF;
    params->inter = spr->data.model.inter;
    params->alwaysInterpolate = false;
    params->id = spr->data.model.id;
    params->selector = spr->data.model.selector;
    params->flags = spr->data.model.flags;
    params->origin[VX] = spr->origin[VX];
    params->origin[VY] = spr->origin[VY];
    params->origin[VZ] = spr->origin[VZ];
    params->srvo[VX] = spr->data.model.visOff[VX];
    params->srvo[VY] = spr->data.model.visOff[VY];
    params->srvo[VZ] = spr->data.model.visOff[VZ] - spr->data.model.floorClip;
    params->gzt = spr->data.model.gzt;
    params->distance = -10;
    params->yaw = spr->data.model.yaw;
    params->extraYawAngle = 0;
    params->yawAngleOffset = spr->data.model.yawAngleOffset;
    params->pitch = spr->data.model.pitch;
    params->extraPitchAngle = 0;
    params->pitchAngleOffset = spr->data.model.pitchAngleOffset;
    params->extraScale = 0;
    params->viewAlign = spr->data.model.viewAligned;
    params->mirror = (mirrorHudModels? true : false);
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shineTranslateWithViewerPos = false;
    params->shinepspriteCoordSpace = true;
    params->ambientColor[CA] = spr->data.model.alpha;

    if((levelFullBright || spr->data.model.stateFullBright) &&
       !spr->data.model.mf->testSubFlag(0, MFF_DIM))
    {
        params->ambientColor[CR] = params->ambientColor[CG] = params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        Map &map = App_World().map();

        if(useBias && map.hasLightGrid())
        {
            Vector3f tmp = map.lightGrid().evaluate(params->origin);
            V3f_Set(params->ambientColor, tmp.x, tmp.y, tmp.z);
        }
        else
        {
            SectorCluster &cluster = spr->data.model.bspLeaf->cluster();
            Vector3f const &secColor = Rend_SectorLightColor(cluster);

            // Diminished light (with compression).
            float lightLevel = cluster.sector().lightLevel();

            // No need for distance attentuation.

            // Add extra light.
            lightLevel += Rend_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor in effect.
            for(int i = 0; i < 3; ++i)
            {
                params->ambientColor[i] = lightLevel * secColor[i];
            }
        }

        Rend_ApplyTorchLight(params->ambientColor, params->distance);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(spr->origin);
        lparams.bspLeaf      = spr->data.model.bspLeaf;
        lparams.ambientColor = Vector3f(params->ambientColor);
        lparams.starkLight   = true;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

/**
 * Draws 3D HUD models.
 */
static void drawPlayerModels()
{
    // Setup the modelview matrix.
    Rend_ModelViewMatrix(false /* don't apply view angle rotation */);

    // Clear Z buffer. This will prevent the psprites from being clipped
    // by nearby polygons.
    glClear(GL_DEPTH_BUFFER_BIT);

    for(int i = 0; i < DDMAXPSPRITES; ++i)
    {
        vispsprite_t *spr = &visPSprites[i];

        if(spr->type != VPSPR_MODEL) continue; // Not used.

        rendmodelparams_t parms; zap(parms);
        setupModelParamsForVisPSprite(&parms, spr);
        Rend_DrawModel(parms);
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
    if(vd->window.size.width == 0) return;
    if(vd->window.size.height == 0) return;

    // Setup for rendering the frame.
    R_SetupFrame(player);

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

    if(App_World().hasMap())
    {
        Rend_RenderMap(App_World().map());
    }

    // Orthogonal projection to the view window.
    GL_Restore2DState(1, currentViewport, vd);

    // Don't render in wireframe mode with 2D psprites.
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    drawPlayerSprites(); // If the 2D versions are needed.

    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Do we need to render any 3D psprites?
    if(psp3d)
    {
        GL_SwitchTo3DState(false, currentViewport, vd);
        drawPlayerModels();
    }

    // Restore fullscreen viewport, original matrices and state: back to normal 2D.
    GL_Restore2DState(2, currentViewport, vd);

    // Back from wireframe mode?
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // The colored filter.
    if(GL_FilterIsVisible())
    {
        GL_DrawFilter();
    }

    Vignette_Render(&vd->window, Rend_FieldOfView());

    // Now we can show the viewPlayer's mobj again.
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        player->shared.mo->ddFlags = oldFlags;
    }

    // Should we be counting triangles?
    /*if(rendInfoTris)
    {
        // This count includes all triangles drawn since R_SetupFrame.
        Con_Printf("Tris: %-4i (Mdl=%-4i)\n", polyCounter, modelTriCount);
        modelTriCount = 0;
        polyCounter = 0;
    }*/

    /*if(rendInfoLums)
    {
        Con_Printf("LumObjs: %-4i\n", LO_GetNumLuminous());
    }*/

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

        Con_Message("%f,%f,%f,%f,%f", Sys_GetRealSeconds() - devCameraMovementStartTimeRealSecs,
                    time, elapsed, speed/elapsed, speed/elapsed - prevSpeed);

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

void R_RenderViewPorts(ui::ViewPortLayer layer)
{
    int oldDisplay = displayPlayer;

    // First clear the viewport.
    if(layer == ui::Player3DViewLayer)
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
                if(layer == ui::Player3DViewLayer)
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
            glOrtho(0, vp->geometry.size.width, vp->geometry.size.height, 0, -1, 1);

            viewdata_t const *vd = &viewDataOfConsole[vp->console];

            if(layer == ui::Player3DViewLayer)
            {
                R_UpdateViewer(vp->console);

                gx.DrawViewPort(p, &vp->geometry, &vd->window, displayPlayer, 0/*layer #0*/);
                restoreDefaultGLState();

                R_RenderPlayerViewBorder();
            }
            else if(layer == ui::HUDLayer)
            {
                gx.DrawViewPort(p, &vp->geometry, &vd->window, displayPlayer, 1/*layer #1*/);
            }

            restoreDefaultGLState();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
        }
    }

    if(layer == ui::Player3DViewLayer)
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

double R_ViewerLumobjDistance(int idx)
{
    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < App_World().map().lumobjCount())
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
    if(idx >= 0 && idx < App_World().map().lumobjCount())
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
    if(idx >= 0 && idx < App_World().map().lumobjCount())
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

    Map &map = App_World().map();

    bspLeafsVisible.resize(map.bspLeafCount());
    bspLeafsVisible.fill(false);

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
        Vector3d delta = lum->origin() - Vector3d(viewData->current.origin);
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

    // objLinks already contains links if there are any light decorations
    // currently in use.
    loInited = true;
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

        Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

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
    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

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

#endif // __CLIENT__

D_CMD(ViewGrid)
{
    DENG_UNUSED(src);

    if(argc != 3)
    {
        Con_Printf("Usage: %s (cols) (rows)\n", argv[0]);
        return true;
    }

    // Recalculate viewports.
    return R_SetViewGrid(strtol(argv[1], NULL, 0), strtol(argv[2], NULL, 0));
}
