/** @file p_pspr.h  Player weapon sprite animation.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_PLAY_PSPR_H
#define LIBHEXEN_PLAY_PSPR_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

struct player_s;

typedef enum {
    ps_weapon,
    ps_flash,
    NUMPSPRITES
} psprnum_t;

typedef struct {
    state_t *state; // @c NULL means not active.
    int tics;
    float pos[2];
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

#endif // LIBHEXEN_PLAY_PSPR_H
