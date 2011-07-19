/**\file r_common.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Common routines for refresh.
 */

#ifndef LIBCOMMON_REFRESH_H
#define LIBCOMMON_REFRESH_H

// Translate between fixed screen dimensions to actual, current.
#define FIXXTOSCREENX(x) (scrwidth * ((x) / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * ((y) / (float) SCREENHEIGHT))
#define SCREENXTOFIXX(x) ((float) SCREENWIDTH * ((x) / scrwidth))
#define SCREENYTOFIXY(y) ((float) SCREENHEIGHT * ((y) / scrheight))

/**
 * @defgroup resizeViewWindowFlags  Resize ViewWindow Flags.
 * @{
 */
#define RWF_FORCE               0x1 /// Update regardless of tracked state.
#define RWF_NO_LERP             0x2 /// Do not interpolate.
/**@}*/

/**
 * @param flags  @see resizeViewWindowFlags
 */
void R_ResizeViewWindow(int flags);

int R_UpdateViewport(int hookType, int param, void* data);

void R_PrecachePSprites(void);

void R_GetGammaMessageStrings(void);
void R_CycleGammaLevel(void);

#endif /* LIBCOMMON_REFRESH_H */
