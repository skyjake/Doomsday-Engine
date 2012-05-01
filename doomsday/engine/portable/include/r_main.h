/**\file r_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Refresh Subsystem.
 */

#ifndef LIBDENG_REFRESH_MAIN_H
#define LIBDENG_REFRESH_MAIN_H

#include "rect.h"

#ifdef __cplusplus
extern "C" {
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

extern float    frameTimePos;      // 0...1: fractional part for sharp game tics
extern int      loadInStartupMode;
extern int      validCount;
extern int      frameCount;
extern int      extraLight;
extern float    extraLightDelta;
extern int      rendInfoTris;
extern fontid_t fontFixed, fontVariable[FONTSTYLE_COUNT];

extern fixed_t  fineTangent[FINEANGLES / 2];

void R_Register(void);
void R_Init(void);
void R_Update(void);
void R_Shutdown(void);

void R_Ticker(timespan_t time);

void R_BeginWorldFrame(void);
void R_EndWorldFrame(void);

void R_RenderViewPorts(void);

void R_RenderBlankView(void);
void R_RenderPlayerView(int num);
void R_RenderPlayerViewBorder(void);

/// @return  Current viewport else @c NULL.
const viewport_t* R_CurrentViewPort(void);

void R_UseViewPort(viewport_t* vp);

const viewdata_t* R_ViewData(int consoleNum);

void R_UpdateViewer(int consoleNum);

void R_ResetViewer(void);

void R_NewSharpWorld(void);

boolean R_SetViewGrid(int numCols, int numRows);

void R_SetupDefaultViewWindow(int consoleNum);

/**
 * Update the view origin position for player @a consoleNum.
 *
 * @param consoleNum  Console number.
 *
 * @note Part of the Doomsday public API.
 */
void R_SetViewOrigin(int consoleNum, coord_t const origin[3]);

/**
 * Update the view yaw angle for player @a consoleNum.
 *
 * @param consoleNum  Console number.
 *
 * @note Part of the Doomsday public API.
 */
void R_SetViewAngle(int consoleNum, angle_t angle);

/**
 * Update the view pitch angle for player @a consoleNum.
 *
 * @param consoleNum  Console number.
 *
 * @note Part of the Doomsday public API.
 */
void R_SetViewPitch(int consoleNum, float pitch);

int R_ViewWindowGeometry(int consoleNum, RectRaw* geometry);
int R_ViewWindowOrigin(int consoleNum, Point2Raw* origin);
int R_ViewWindowSize(int consoleNum, Size2Raw* size);

void R_SetViewWindowGeometry(int consoleNum, const RectRaw* geometry, boolean interpolate);

/**
 * Animates the view window towards the target values.
 */
void R_ViewWindowTicker(int consoleNum, timespan_t ticLength);

int R_ViewPortGeometry(int consoleNum, RectRaw* geometry);
int R_ViewPortOrigin(int consoleNum, Point2Raw* origin);
int R_ViewPortSize(int consoleNum, Size2Raw* size);

void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

void R_LoadSystemFonts(void);

const char* R_ChooseFixedFont(void);
const char* R_ChooseVariableFont(fontstyle_t style, int resX, int resY);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REFRESH_MAIN_H */
