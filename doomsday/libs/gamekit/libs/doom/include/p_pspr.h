/** @file p_pspr.h  Player weapon sprite animation.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBDOOM_PLAY_PSPR_H
#define LIBDOOM_PLAY_PSPR_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "tables.h"
#include "info.h"

struct player_s;

/**
 * Overlay psprites are scaled shapes drawn directly on the viewscreen,
 * coordinates are in virtual, [320 x 200] viewscreen-space.
 */
typedef enum {
    ps_weapon,
    ps_flash,
    NUMPSPRITES
} psprnum_t;

typedef struct {
    state_t *state; // A NULL state means not active.
    int tics;
    float pos[2]; // [x, y].
} pspdef_t;

#ifdef __cplusplus
extern "C" {
#endif

void R_GetWeaponBob(int player, float *x, float *y);

void P_BringUpWeapon(struct player_s *player);

void P_FireWeapon(struct player_s *player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOM_PLAY_PSPR_H
