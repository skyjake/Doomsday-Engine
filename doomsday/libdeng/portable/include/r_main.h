/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * r_main.h: Refresh Subsystem
 */

#ifndef __DOOMSDAY_REFRESH_MAIN_H__
#define __DOOMSDAY_REFRESH_MAIN_H__

typedef struct viewport_s {
    int             console;
    int             x, y, width, height;
} viewport_t;

extern float    viewX, viewY, viewZ;
extern float    viewFrontVec[3], viewUpVec[3], viewSideVec[3];
extern float    viewXOffset, viewYOffset, viewZOffset;
extern angle_t  viewAngle;
extern float    viewPitch;
extern angle_t  clipAngle;
extern fixed_t  fineTangent[FINEANGLES / 2];

extern float    frameTimePos;      // 0...1: fractional part for sharp game tics
extern boolean  resyncFrameTimePos;
extern int      loadInStartupMode;
extern int      validCount;
extern int      viewwidth, viewheight, viewwindowx, viewwindowy;
extern boolean  setSizeNeeded;
extern int      frameCount;
extern int      viewAngleOffset;
extern int      extraLight;
extern float    extraLightDelta;
extern float    viewCos, viewSin;
extern int      rendInfoTris;

void            R_Register(void);
void            R_Init(void);
void            R_Update(void);
void            R_Shutdown(void);
void            R_BeginWorldFrame(void);
void            R_EndWorldFrame(void);
void            R_RenderPlayerViewBorder(void);
void            R_RenderViewPorts(void);
void            R_ResetViewer(void);
void            R_NewSharpWorld(void);
void            R_SetViewGrid(int numCols, int numRows);

#endif
