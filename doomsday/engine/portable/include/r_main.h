/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * r_main.h: Refresh Subsystem
 */

#ifndef __DOOMSDAY_REFRESH_MAIN_H__
#define __DOOMSDAY_REFRESH_MAIN_H__

typedef struct viewport_s {
    int             console;
    int             x, y, width, height;
} viewport_t;

typedef struct viewer_s {
    float       pos[3];
    angle_t     angle;
    float       pitch;
} viewer_t;

typedef struct viewdata_s {
    viewer_t        current; // Current view paramaters.
    viewer_t        lastSharp[2]; // For smoothing.
    float           frontVec[3], upVec[3], sideVec[3];
    float           viewCos, viewSin;

    // These are used when camera smoothing is disabled.
    angle_t         frozenAngle;
    float           frozenPitch;
} viewdata_t;

extern float    viewX, viewY, viewZ, viewPitch;
extern int      viewAngle;

extern float    frameTimePos;      // 0...1: fractional part for sharp game tics
extern int      loadInStartupMode;
extern int      validCount;
extern int      viewwidth, viewheight, viewwindowx, viewwindowy;
extern int      frameCount;
extern int      extraLight;
extern float    extraLightDelta;
extern int      rendInfoTris;

extern fixed_t  fineTangent[FINEANGLES / 2];

void            R_Register(void);
void            R_Init(void);
void            R_Update(void);
void            R_Shutdown(void);
void            R_BeginWorldFrame(void);
void            R_EndWorldFrame(void);
void            R_RenderPlayerView(int num);
void            R_RenderPlayerViewBorder(void);
void            R_RenderViewPorts(void);

const viewport_t* R_CurrentViewPort(void);

const viewdata_t* R_ViewData(int localPlayerNum);
void            R_ResetViewer(void);

void            R_SetViewWindow(int x, int y, int w, int h);
void            R_NewSharpWorld(void);

boolean         R_SetViewGrid(int numCols, int numRows);
void            R_SetViewWindow(int x, int y, int w, int h);
#endif
