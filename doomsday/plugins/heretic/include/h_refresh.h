/**\file h_refresh.h
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

#ifndef JHERETIC_REFRESH_H
#define JHERETIC_REFRESH_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "hu_stuff.h"
#include "p_mobj.h"

extern float quitDarkenOpacity;

void H_DrawViewPort(int port, const RectRaw* portGeometry, const RectRaw* windowGeometry, int player, int layer);
void H_DrawWindow(const Size2Raw* windowSize);
void H_EndFrame(void);

void P_SetDoomsdayFlags(mobj_t* mo);
void R_SetAllDoomsdayFlags(void);
boolean R_ViewFilterColor(float rgba[4], int filter);
void R_UpdateViewFilter(int player);

#endif /* JHERETIC_REFRESH_H */
