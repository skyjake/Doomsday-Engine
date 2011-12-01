/**\file r_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

typedef struct viewport_s {
    int console;
    RectRawi geometry;
} viewport_t;

typedef struct viewer_s {
    float pos[3];
    angle_t angle;
    float pitch;
} viewer_t;

typedef struct viewdata_s {
    viewer_t sharp;
    viewer_t current;
    viewer_t lastSharp[2]; // For smoothing.
    float frontVec[3], upVec[3], sideVec[3];
    float viewCos, viewSin;
    RectRawi window, windowTarget, windowOld;
    float windowInter;
} viewdata_t;

typedef enum fontstyle_e {
    FS_NORMAL,
    FS_BOLD,
    FS_LIGHT,
    FONTSTYLE_COUNT
} fontstyle_t;

extern float viewX, viewY, viewZ, viewPitch;
extern angle_t viewAngle;

extern float    frameTimePos;      // 0...1: fractional part for sharp game tics
extern int      loadInStartupMode;
extern int      validCount;
extern int      frameCount;
extern int      extraLight;
extern float    extraLightDelta;
extern int      rendInfoTris;
extern fontid_t fontFixed, fontVariable[FONTSTYLE_COUNT];

extern fixed_t  fineTangent[FINEANGLES / 2];

void            R_Register(void);
void            R_Init(void);
void            R_Update(void);
void            R_Shutdown(void);

void            R_Ticker(timespan_t time);
void            R_BeginWorldFrame(void);
void            R_EndWorldFrame(void);
void            R_RenderPlayerView(int num);
void            R_RenderPlayerViewBorder(void);
void            R_RenderBlankView(void);
void            R_RenderViewPorts(void);

/// @return  Current viewport else @c NULL.
const viewport_t* R_CurrentViewPort(void);

const viewdata_t* R_ViewData(int localPlayerNum);
void            R_ResetViewer(void);

void            R_NewSharpWorld(void);

boolean R_SetViewGrid(int numCols, int numRows);

void R_SetupDefaultViewWindow(int player);

void R_LoadSystemFonts(void);

const char* R_ChooseFixedFont(void);
const char* R_ChooseVariableFont(fontstyle_t style, int resX, int resY);

int R_ViewWindowGeometry(int player, RectRawi* geometry);
int R_ViewWindowOrigin(int player, Point2Rawi* origin);
int R_ViewWindowSize(int player, Size2Rawi* size);

void R_SetViewWindowGeometry(int player, const RectRawi* geometry, boolean interpolate);

/**
 * Animates the view window towards the target values.
 */
void R_ViewWindowTicker(int player, timespan_t ticLength);

int R_ViewPortGeometry(int player, RectRawi* geometry);
int R_ViewPortOrigin(int player, Point2Rawi* origin);
int R_ViewPortSize(int player, Size2Rawi* size);

void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

#endif /* LIBDENG_REFRESH_MAIN_H */
