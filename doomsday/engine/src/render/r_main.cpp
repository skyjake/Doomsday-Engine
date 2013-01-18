/** @file r_main.cpp
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "gl/svg.h"
#include "map/p_players.h"
#include "map/p_objlink.h"
#include "render/vignette.h"
#include "api_render.h"

using namespace de;

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
float devCameraMovementStartTime = 0; // sysTime
float devCameraMovementStartTimeRealSecs = 0;
#endif

BEGIN_PROF_TIMERS()
  PROF_MOBJ_INIT_ADD
END_PROF_TIMERS()

D_CMD(ViewGrid);

int validCount = 1; // Increment every time a check is made.
int frameCount; // Just for profiling purposes.
int rendInfoTris = 0;

// Precalculated math tables.
fixed_t *fineCosine = &finesine[FINEANGLES / 4];

int extraLight; // Bumped light from gun blasts.
float extraLightDelta;

float frameTimePos; // 0...1: fractional part for sharp game tics.

int loadInStartupMode = false;

byte precacheSkins = true;
byte precacheMapMaterials = true;
byte precacheSprites = true;

fontid_t fontFixed, fontVariable[FONTSTYLE_COUNT];

static int rendCameraSmooth = true; // Smoothed by default.

static boolean resetNextViewer = true;

static viewdata_t viewDataOfConsole[DDMAXPLAYERS]; // Indexed by console number.

static byte showFrameTimePos = false;
static byte showViewAngleDeltas = false;
static byte showViewPosDeltas = false;

static int gridCols, gridRows;
static viewport_t viewportOfLocalPlayer[DDMAXPLAYERS], *currentViewport;

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

    Materials::consoleRegister();
}

char const *R_ChooseFixedFont()
{
    if(Window_Width(theWindow) < 300) return "console11";
    if(Window_Width(theWindow) > 768) return "console18";
    return "console14";
}

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
static fontid_t loadSystemFont(char const *name)
{
    DENG_ASSERT(name && name[0]);

    // Compose the resource name.
    struct uri_s *uri = Uri_NewWithPath2("System:", RC_NULL);
    Uri_SetPath(uri, name);

    // Compose the resource data path.
    ddstring_t resourcePath; Str_InitStd(&resourcePath);
    Str_Appendf(&resourcePath, "}data/Fonts/%s.dfn", name);
#if defined(UNIX) && !defined(MACOSX)
    // Case-sensitive file system.
    /// @todo Unkludge this: handle in a more generic manner.
    strlwr(resourcePath.str);
#endif
    F_ExpandBasePath(&resourcePath, &resourcePath);

    font_t *font = R_CreateFontFromFile(uri, Str_Text(&resourcePath));
    Str_Free(&resourcePath);
    Uri_Delete(uri);

    if(!font)
    {
        Con_Error("loadSystemFont: Failed loading font \"%s\".", name);
        exit(1); // Unreachable.
    }

    return Fonts_Id(font);
}

static void loadFontIfNeeded(char const *uri, fontid_t *fid)
{
    *fid = Fonts_ResolveUriCString(uri);
    if(*fid == NOFONTID)
    {
        *fid = loadSystemFont(uri);
    }
}
#endif // __CLIENT__

void R_LoadSystemFonts()
{
#ifdef __CLIENT__

    if(!Fonts_IsInitialized() || isDedicated) return;

    loadFontIfNeeded(R_ChooseFixedFont(), &fontFixed);
    loadFontIfNeeded(R_ChooseVariableFont(FS_NORMAL, Window_Width(theWindow), Window_Height(theWindow)), &fontVariable[FS_NORMAL]);
    loadFontIfNeeded(R_ChooseVariableFont(FS_BOLD,   Window_Width(theWindow), Window_Height(theWindow)), &fontVariable[FS_BOLD]);
    loadFontIfNeeded(R_ChooseVariableFont(FS_LIGHT,  Window_Width(theWindow), Window_Height(theWindow)), &fontVariable[FS_LIGHT]);

    Con_SetFont(fontFixed);

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
    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;

    vd->window.origin.x = vd->windowOld.origin.x = vd->windowTarget.origin.x = 0;
    vd->window.origin.y = vd->windowOld.origin.y = vd->windowTarget.origin.y = 0;
    vd->window.size.width  = vd->windowOld.size.width  = vd->windowTarget.size.width  = Window_Width(theWindow);
    vd->window.size.height = vd->windowOld.size.height = vd->windowTarget.size.height = Window_Height(theWindow);
    vd->windowInter = 1;
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
    DENG_ASSERT(port);

    RectRaw *rect = &port->geometry;
    int const x = col * Window_Width(theWindow)  / gridCols;
    int const y = row * Window_Height(theWindow) / gridRows;
    int const width  = (col+1) * Window_Width(theWindow)  / gridCols - x;
    int const height = (row+1) * Window_Height(theWindow) / gridRows - y;
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
    R_InitColorPalettes();
    R_InitTranslationTables();
    R_InitRawTexs();
    R_InitSvgs();
    R_InitViewWindow();
    Rend_Init();
    frameCount = 0;
}

static void R_UpdateMap()
{
    if(!theMap) return;

#ifdef __CLIENT__
    // Update all world surfaces.
    for(uint i = 0; i < NUM_SECTORS; ++i)
    {
        Sector *sec = GameMap_Sector(theMap, i);
        for(uint j = 0; j < sec->planeCount; ++j)
        {
            Surface_Update(&sec->SP_planesurface(j));
        }
    }

    for(uint i = 0; i < NUM_SIDEDEFS; ++i)
    {
        SideDef *side = GameMap_SideDef(theMap, i);
        Surface_Update(&side->SW_topsurface);
        Surface_Update(&side->SW_middlesurface);
        Surface_Update(&side->SW_bottomsurface);
    }

    for(uint i = 0; i < NUM_POLYOBJS; ++i)
    {
        Polyobj *po = GameMap_PolyobjByID(theMap, i);
        for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
        {
            LineDef *line = *lineIter;
            SideDef *side = line->L_frontsidedef;
            Surface_Update(&side->SW_middlesurface);
        }
    }
#endif

    R_MapInitSurfaceLists();

    // See what mapinfo says about this map.
    ded_mapinfo_t *mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
    if(!mapInfo)
    {
        struct uri_s *mapUri = Uri_NewWithPath2("*", RC_NULL);
        mapInfo = Def_GetMapInfo(mapUri);
        Uri_Delete(mapUri);
    }

    // Reconfigure the sky
    ded_sky_t *skyDef = 0;
    if(mapInfo)
    {
        skyDef = Def_GetSky(mapInfo->skyID);
        if(!skyDef) skyDef = &mapInfo->sky;
    }
#ifdef __CLIENT__
    Sky_Configure(skyDef);
#endif

    if(mapInfo)
    {
        theMap->globalGravity     = mapInfo->gravity;
        theMap->ambientLightLevel = mapInfo->ambient * 255;
    }
    else
    {
        // No theMap info found, so set some basic stuff.
        theMap->globalGravity = 1.0f;
        theMap->ambientLightLevel = 0;
    }

    theMap->effectiveGravity = theMap->globalGravity;

    // Recalculate the light range mod matrix.
    Rend_CalcLightModRange();
}

void R_Update()
{
    // Reset file IDs so previously seen files can be processed again.
    F_ResetFileIds();
    // Re-read definitions.
    Def_Read();

    R_UpdateRawTexs();
    R_InitSprites(); // Fully reinitialize sprites.
    Models_Init(); // Defs might've changed.

    R_UpdateTranslationTables();

    Def_PostInit();
    P_UpdateParticleGens(); // Defs might've changed.

    // Reset the archived map cache (the available maps may have changed).
    DAM_Init();

    for(uint i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];
        ddplayer_t *ddpl = &plr->shared;
        // States have changed, the states are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = NULL;
    }

    R_UpdateMap();

#ifdef __CLIENT__
    // The rendering lists have persistent data that has changed during
    // the re-initialization.
    RL_DeleteLists();

    // Update the secondary title and the game status.
    Rend_ConsoleUpdateTitle();
#endif

#if _DEBUG
    Z_CheckHeap();
#endif
}

void R_Shutdown()
{
    R_ClearAnimGroups();
    R_ShutdownSprites();
    Models_Shutdown();
    R_ShutdownSvgs();
#ifdef __CLIENT__
    R_ShutdownViewWindow();
    Fonts_Shutdown();
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
        Con_Message("R_CheckViewerLimits: Snap camera angle to %08x.\n", dst->angle);
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

    R_UpdateTrackedPlanes();
    R_UpdateSurfaceScroll();
}

void R_CreateMobjLinks()
{
#ifdef __CLIENT__
#ifdef DD_PROFILE
    static int p;

    if(++p > 40)
    {
        p = 0;
        PRINT_PROF( PROF_MOBJ_INIT_ADD );
    }
#endif

    if(!theMap) return;

BEGIN_PROF( PROF_MOBJ_INIT_ADD );

    for(uint i = 0; i < NUM_SECTORS; ++i)
    {
        Sector *sec = GameMap_Sector(theMap, i);
        for(mobj_t *iter = sec->mobjList; iter; iter = iter->sNext)
        {
            R_ObjlinkCreate(iter, OT_MOBJ); // For spreading purposes.
        }
    }

END_PROF( PROF_MOBJ_INIT_ADD );

#endif
}

void R_BeginWorldFrame()
{
#ifdef __CLIENT__

    R_ClearSectorFlags();

    R_InterpolateTrackedPlanes(resetNextViewer);
    R_InterpolateSurfaceScroll(resetNextViewer);

    if(!freezeRLs)
    {
        LG_Update();
        SB_BeginFrame();
        LO_BeginWorldFrame();
        R_ClearObjlinksForFrame(); // Zeroes the links.

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

#endif
}

void R_EndWorldFrame()
{
#ifdef __CLIENT__
    if(!freezeRLs)
    {
        // Wrap up with Source, Bias lights.
        SB_EndFrame();
    }
#endif
}

void R_UpdateViewer(int consoleNum)
{
    DENG_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);

    int const VIEWPOS_MAX_SMOOTHDISTANCE = 172;

    viewdata_t *vd = viewDataOfConsole + consoleNum;
    player_t *player = ddPlayers + consoleNum;
    viewer_t sharpView, smoothView;
    float yawRad, pitchRad;
    uint an;

    if(!player->shared.inGame || !player->shared.mo) return;

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
                double time;
                float x, y, z;
            } oldpos_t;

            static oldpos_t oldpos[DDMAXPLAYERS];
            oldpos_t *old = &oldpos[viewPlayer - ddPlayers];

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f dz=%-10.3f dx/dt=%-10.3f dy/dt=%-10.3f\n",
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
    an = vd->current.angle >> ANGLETOFINESHIFT;
    vd->viewSin = FIX2FLT(finesine[an]);
    vd->viewCos = FIX2FLT(fineCosine[an]);

    // Calculate the front, up and side unit vectors.
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
    extraLight = player->extraLight;
    extraLightDelta = extraLight / 16.0f;

    // Why?
    validCount++;

    if(!freezeRLs)
    {
        R_ClearVisSprites();
    }

#undef MINEXTRALIGHTFRAMES
}

#ifdef __CLIENT__
void R_RenderPlayerViewBorder()
{
    R_DrawViewBorder();
}

void R_UseViewPort(viewport_t *vp)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!vp)
    {
        currentViewport = NULL;
        glViewport(0, FLIP(0 + Window_Height(theWindow) - 1),
            Window_Width(theWindow), Window_Height(theWindow));
    }
    else
    {
        currentViewport = vp;
        glViewport(vp->geometry.origin.x,
            FLIP(vp->geometry.origin.y + vp->geometry.size.height - 1),
            vp->geometry.size.width, vp->geometry.size.height);
    }
}

viewport_t const *R_CurrentViewPort()
{
    return currentViewport;
}

void R_RenderBlankView()
{
    Point2Raw origin(0, 0);
    Size2Raw size(320, 200);
    UI_DrawDDBackground(&origin, &size, 1);
}

#undef R_RenderPlayerView
DENG_EXTERN_C void R_RenderPlayerView(int num)
{
    if(num < 0 || num >= DDMAXPLAYERS) return; // Huh?
    player_t *player = &ddPlayers[num];

    if(!player->shared.inGame || !player->shared.mo) return;

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
    if(vd->window.size.width == 0 || vd->window.size.height == 0) return;

    // Setup for rendering the frame.
    R_SetupFrame(player);

    R_ProjectPlayerSprites(); // Only if 3D models exists for them.

    // Hide the viewPlayer's mobj?
    int oldFlags = 0;
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        oldFlags = player->shared.mo->ddFlags;
        player->shared.mo->ddFlags |= DDMF_DONTDRAW;
    }

    // Go to wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // GL is in 3D transformation state only during the frame.
    GL_SwitchTo3DState(true, currentViewport, vd);

    Rend_RenderMap();

    // Orthogonal projection to the view window.
    GL_Restore2DState(1, currentViewport, vd);

    // Don't render in wireframe mode with 2D psprites.
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    Rend_Draw2DPlayerSprites(); // If the 2D versions are needed.

    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // The colored filter.
    if(GL_FilterIsVisible())
    {
        GL_DrawFilter();
    }

    Vignette_Render(&vd->window, fieldOfView);

    // Now we can show the viewPlayer's mobj again.
    if(!(player->shared.flags & DDPF_CHASECAM))
        player->shared.mo->ddFlags = oldFlags;

    // Should we be counting triangles?
    if(rendInfoTris)
    {
        // This count includes all triangles drawn since R_SetupFrame.
        //Con_Printf("Tris: %-4i (Mdl=%-4i)\n", polyCounter, modelTriCount);
        //modelTriCount = 0;
        //polyCounter = 0;
    }

    if(rendInfoLums)
    {
        Con_Printf("LumObjs: %-4i\n", LO_GetNumLuminous());
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

        Con_Message("%f,%f,%f,%f,%f\n", Sys_GetRealSeconds() - devCameraMovementStartTimeRealSecs,
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

void R_RenderViewPorts()
{
    int oldDisplay = displayPlayer, x, y, p;
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

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // This is all the clearing we'll do.
    glClear(bits);

    // Draw a view for all players with a visible viewport.
    for(p = 0, y = 0; y < gridRows; ++y)
        for(x = 0; x < gridCols; x++, ++p)
        {
            viewport_t *vp = &viewportOfLocalPlayer[p];
            displayPlayer = vp->console;
            R_UseViewPort(vp);

            if(displayPlayer < 0 || (ddPlayers[displayPlayer].shared.flags & DDPF_UNDEFINED_ORIGIN))
            {
                R_RenderBlankView();
                continue;
            }

            R_UpdateViewer(vp->console);

            viewdata_t *vd = &viewDataOfConsole[vp->console];

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();

            /**
             * Use an orthographic projection in real pixel dimensions.
             */
            glOrtho(0, vp->geometry.size.width, vp->geometry.size.height, 0, -1, 1);

            gx.DrawViewPort(p, &vp->geometry, &vd->window, displayPlayer, 0/*layer #0*/);
            restoreDefaultGLState();

            R_RenderPlayerViewBorder();

            gx.DrawViewPort(p, &vp->geometry, &vd->window, displayPlayer, 1/*layer #1*/);
            restoreDefaultGLState();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();

            // Increment the internal frame count. This does not
            // affect the FPS counter.
            frameCount++;
        }

    // Keep reseting until a new sharp world has arrived.
    if(resetNextViewer > 1)
        resetNextViewer = 0;

    // Restore things back to normal.
    displayPlayer = oldDisplay;
    R_UseViewPort(NULL);
}

static int findSpriteOwner(thinker_t *th, void *context)
{    mobj_t *mo = (mobj_t *) th;
    spritedef_t *sprDef = (spritedef_t *) context;

    if(mo->type >= 0 && mo->type < defs.count.mobjs.num)
    {
        /// @todo optimize: traverses the entire state list!
        for(int i = 0; i < defs.count.states.num; ++i)
        {
            if(stateOwners[i] != &mobjInfo[mo->type]) continue;

            if(&sprites[states[i].sprite] == sprDef)
                return true; // Found one.
        }
    }

    return false; // Continue iteration.
}

static void cacheSpritesForState(int stateIndex, MaterialVariantSpec const &spec)
{
    if(stateIndex < 0 || stateIndex >= defs.count.states.num) return;

    state_t *state = &states[stateIndex];
    spritedef_t *sprDef = &sprites[state->sprite];

    for(int i = 0; i < sprDef->numFrames; ++i)
    {
        spriteframe_t *sprFrame = &sprDef->spriteFrames[i];
        for(int k = 0; k < 8; ++k)
        {
            App_Materials()->cache(*sprFrame->mats[k], spec);
        }
    }
}

#undef Rend_CacheForMobjType
DENG_EXTERN_C void Rend_CacheForMobjType(int num)
{
    if(novideo || !((useModels && precacheSkins) || precacheSprites)) return;
    if(num < 0 || num >= defs.count.mobjs.num) return;

    MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec();

    /// @todo Optimize: Traverses the entire state list!
    for(int i = 0; i < defs.count.states.num; ++i)
    {
        if(stateOwners[i] != &mobjInfo[num]) continue;

        Models_CacheForState(i);

        if(precacheSprites)
        {
            cacheSpritesForState(i, spec);
        }
        /// @todo What about sounds?
    }
}

void Rend_CacheForMap()
{
#ifdef __SERVER__
    return;
#endif

    // Don't precache when playing a demo (why not? -ds).
    if(playback) return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    if(precacheMapMaterials)
    {
        MaterialVariantSpec const &spec = Rend_MapSurfaceMaterialSpec();

        for(uint i = 0; i < NUM_SIDEDEFS; ++i)
        {
            SideDef *side = SIDE_PTR(i);

            if(side->SW_middlematerial)
                App_Materials()->cache(*side->SW_middlematerial, spec);

            if(side->SW_topmaterial)
                App_Materials()->cache(*side->SW_topmaterial, spec);

            if(side->SW_bottommaterial)
                App_Materials()->cache(*side->SW_bottommaterial, spec);
        }

        for(uint i = 0; i < NUM_SECTORS; ++i)
        {
            Sector *sec = SECTOR_PTR(i);
            if(!sec->lineDefCount) continue;

            for(uint k = 0; k < sec->planeCount; ++k)
            {
                if(sec->SP_planematerial(k))
                    App_Materials()->cache(*sec->SP_planematerial(k), spec);
            }
        }
    }

    if(precacheSprites)
    {
        MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec();

        for(int i = 0; i < numSprites; ++i)
        {
            spritedef_t *sprDef = &sprites[i];

            if(GameMap_IterateThinkers(theMap, reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                       0x1/* All mobjs are public*/,
                                       findSpriteOwner, sprDef))
            {
                // This sprite is used by some state of at least one mobj.

                // Precache all the frames.
                for(int k = 0; k < sprDef->numFrames; ++k)
                {
                    spriteframe_t *sprFrame = &sprDef->spriteFrames[k];
                    for(int m = 0; m < 8; ++m)
                    {
                        App_Materials()->cache(*sprFrame->mats[m], spec);
                    }
                }
            }
        }
    }

     // Sky models usually have big skins.
    Sky_Cache();

    // Precache model skins?
    if(useModels && precacheSkins)
    {
        // All mobjs are public.
        GameMap_IterateThinkers(theMap, reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                0x1, Models_CacheForMobj, NULL);
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
