/** @file h_refresh.h  Heretic specific refresh functions/utilities.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef JHERETIC_REFRESH_H
#define JHERETIC_REFRESH_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "hu_stuff.h"
#include "p_mobj.h"

DE_EXTERN_C float quitDarkenOpacity;

#ifdef __cplusplus
extern "C" {
#endif

void G_RendPlayerView(int player);

void H_DrawViewPort(int port, const RectRaw *portGeometry, const RectRaw *windowGeometry, int player, int layer);

void H_DrawWindow(const Size2Raw *windowSize);

void H_EndFrame(void);

void P_SetDoomsdayFlags(mobj_t *mo);

void R_SetAllDoomsdayFlags(void);

dd_bool R_ViewFilterColor(float rgba[4], int filter);

/**
 * Sets the new palette based upon current values of player->damageCount and player->bonusCount
 */
void R_UpdateViewFilter(int player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // JHERETIC_REFRESH_H
