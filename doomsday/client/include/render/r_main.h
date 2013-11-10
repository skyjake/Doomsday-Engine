/** @file r_main.h
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

#ifndef LIBDENG_RENDER_R_MAIN_H
#define LIBDENG_RENDER_R_MAIN_H

#include <de/rect.h>
#include "dd_share.h"

#ifdef __CLIENT__
class BspLeaf;
class Lumobj;
#endif

typedef struct viewport_s {
    int console;
    RectRaw geometry;
} viewport_t;

typedef struct viewer_s {
    coord_t origin[3];
    angle_t angle;
    float pitch;
} viewer_t;

typedef struct viewdata_s {
    viewer_t current;
    viewer_t lastSharp[2]; ///< For smoothing.
    viewer_t latest; ///< "Sharp" values taken from here.

    /**
     * These vectors are in the DGL coordinate system, which is a left-handed
     * one (same as in the game, but Y and Z have been swapped). Anyone who uses
     * these must note that it might be necessary to fix the aspect ratio of the
     * Y axis by dividing the Y coordinate by 1.2.
     */
    float frontVec[3], upVec[3], sideVec[3];

    float viewCos, viewSin;

    RectRaw window, windowTarget, windowOld;
    float windowInter;
} viewdata_t;

typedef enum fontstyle_e {
    FS_NORMAL,
    FS_BOLD,
    FS_LIGHT,
    FONTSTYLE_COUNT
} fontstyle_t;

DENG_EXTERN_C float    frameTimePos;      // 0...1: fractional part for sharp game tics
DENG_EXTERN_C int      loadInStartupMode;
DENG_EXTERN_C int      validCount;
DENG_EXTERN_C int      frameCount;
DENG_EXTERN_C int      rendInfoTris;

DENG_EXTERN_C int      levelFullBright;

DENG_EXTERN_C float    pspOffset[2], pspLightLevelMultiplier;
DENG_EXTERN_C int      psp3d;
DENG_EXTERN_C float    weaponOffsetScale, weaponFOVShift;
DENG_EXTERN_C int      weaponOffsetScaleY;
DENG_EXTERN_C byte     weaponScaleMode; // cvar

DENG_EXTERN_C boolean  firstFrameAfterLoad;

DENG_EXTERN_C byte     precacheMapMaterials, precacheSprites, precacheSkins;

DENG_EXTERN_C fontid_t fontFixed, fontVariable[FONTSTYLE_COUNT];

DENG_EXTERN_C fixed_t  fineTangent[FINEANGLES / 2];

DENG_EXTERN_C byte     texGammaLut[256];
#ifdef __CLIENT__
DENG_EXTERN_C boolean  loInited;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register console variables.
 */
void R_Register(void);

void R_BuildTexGammaLut(void);

/**
 * One-time initialization of the refresh daemon. Called by DD_Main.
 */
void R_Init(void);

/**
 * Re-initialize almost everything.
 */
void R_Update(void);

/**
 * Shutdown the refresh daemon.
 */
void R_Shutdown(void);

void R_Ticker(timespan_t time);

namespace ui {
    enum ViewPortLayer {
        Player3DViewLayer,
        HUDLayer
    };
}

/**
 * Render all view ports in the viewport grid.
 */
void R_RenderViewPorts(ui::ViewPortLayer layer);

/**
 * Render a blank view for the specified player.
 */
void R_RenderBlankView(void);

/**
 * Draw the border around the view window.
 */
void R_RenderPlayerViewBorder(void);

/// @return  Current viewport; otherwise @c NULL.
viewport_t const *R_CurrentViewPort(void);

/**
 * Set the current GL viewport.
 */
void R_UseViewPort(viewport_t const *vp);

viewdata_t const *R_ViewData(int consoleNum);

void R_UpdateViewer(int consoleNum);

void R_ResetViewer(void);

int R_NextViewer(void);

#ifdef __CLIENT__

void R_ClearViewData(void);

/**
 * To be called at the beginning of a render frame to perform necessary initialization.
 */
void R_BeginFrame(void);

/**
 * Returns @c true iff the BSP leaf is marked as visible for the current frame.
 *
 * @see R_ViewerBspLeafMarkVisible()
 */
bool R_ViewerBspLeafIsVisible(BspLeaf const &bspLeaf);

/**
 * Mark the BSP leaf as visible for the current frame.
 *
 * @see R_ViewerBspLeafIsVisible()
 */
void R_ViewerBspLeafMarkVisible(BspLeaf const &bspLeaf, bool yes = true);

/// @return  Distance in map space units between the lumobj and viewer.
double R_ViewerLumobjDistance(int idx);

/// @return  @c true if the lumobj is clipped for the viewer.
bool R_ViewerLumobjIsClipped(int idx);

/// @return  @c true if the lumobj is hidden for the viewer.
bool R_ViewerLumobjIsHidden(int idx);

/**
 * Clipping strategy:
 *
 * If culling world surfaces with the angle clipper and the viewer is not in the
 * void; use the angle clipper. Otherwise, use the BSP-based LOS algorithm.
 */
void R_ViewerClipLumobj(Lumobj *lum);

void R_ViewerClipLumobjBySight(Lumobj *lum, BspLeaf *bspLeaf);

#endif // __CLIENT__

/**
 * Update the sharp world data by rotating the stored values of plane
 * heights and sharp camera positions.
 */
void R_NewSharpWorld(void);

/**
 * Attempt to set up a view grid and calculate the viewports. Set 'numCols' and
 * 'numRows' to zero to just update the viewport coordinates.
 */
boolean R_SetViewGrid(int numCols, int numRows);

void R_SetupDefaultViewWindow(int consoleNum);

/**
 * Animates the view window towards the target values.
 */
void R_ViewWindowTicker(int consoleNum, timespan_t ticLength);

void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

void R_LoadSystemFonts(void);

#ifdef __CLIENT__
char const *R_ChooseFixedFont(void);
char const *R_ChooseVariableFont(fontstyle_t style, int resX, int resY);
#endif

/**
 * Prepare resources for the current Map.
 */
void Rend_CacheForMap(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REFRESH_MAIN_H */
