/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef JDOOM64_REFRESH_H
#define JDOOM64_REFRESH_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomsday.h"
#include "p_mobj.h"

DE_EXTERN_C float quitDarkenOpacity;

#ifdef __cplusplus
extern "C" {
#endif

void G_RendPlayerView(int player);
void G_RendSpecialFilter(int player, const RectRaw *region);

void D_DrawViewPort(int port, const RectRaw* portGeometry, const RectRaw* windowGeometry, int player, int layer);
void D64_DrawWindow(const Size2Raw* windowSize);
void D64_EndFrame(void);

void R_DrawSpecialFilter(int pnum);
void R_DrawMapTitle(void);

void P_SetDoomsdayFlags(mobj_t* mo);
void R_SetAllDoomsdayFlags(void);
dd_bool R_ViewFilterColor(float rgba[4], int filter);
void R_UpdateViewFilter(int player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* JDOOM64_REFRESH_H */
