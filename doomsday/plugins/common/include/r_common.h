/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * r_common.h : Common routines for refresh.
 */

#ifndef __GAME_COMMON_REFRESH_H__
#define __GAME_COMMON_REFRESH_H__

// A combination of patch data and its lump number.
typedef struct dpatch_s {
    int             width, height;
    int             leftOffset, topOffset;
    lumpnum_t       lump;
} dpatch_t;

// Translate between fixed screen dimensions to actual, current.
#define FIXXTOSCREENX(x) (scrwidth * ((x) / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * ((y) / (float) SCREENHEIGHT))
#define SCREENXTOFIXX(x) ((float) SCREENWIDTH * ((x) / scrwidth))
#define SCREENYTOFIXY(y) ((float) SCREENHEIGHT * ((y) / scrheight))

#define LERP(start, end, pos) (end * pos + start * (1 - pos))

void            R_SetViewWindowTarget(int x, int y, int w, int h);
void            R_ViewWindowTicker(void);
void            R_GetViewWindow(float* x, float* y, float* w, float* h);
boolean         R_IsFullScreenViewWindow(void);
boolean         R_MapObscures(int playerid, int x, int y, int w, int h);

void            R_PrecachePSprites(void);
void            R_CachePatch(dpatch_t* dp, const char* name);

void            R_GetGammaMessageStrings(void);
void            R_CycleGammaLevel(void);

#endif
