/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * h_items.h: Items, key cards/weapons/ammunition...
 */

#ifndef __JHERETIC_ITEMS_H__
#define __JHERETIC_ITEMS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"

#define WEAPON_INFO(weaponnum, pclass, fmode) ( \
    &weaponInfo[(weaponnum)][(pclass)].mode[(fmode)])

typedef enum {
    WSN_UP,
    WSN_DOWN,
    WSN_READY,
    WSN_ATTACK,
    WSN_ATTACK_HOLD,
    WSN_FLASH,
    NUM_WEAPON_STATE_NAMES
} weaponstatename_t;

typedef struct {
    int             gameModeBits;  // Game modes, weapon is available in.
    int             ammoType[NUM_AMMO_TYPES]; // required ammo types.
    int             perShot[NUM_AMMO_TYPES]; // Ammo used per shot of each type.
    dd_bool         autoFire; // @c true = fire when raised if fire held.
    int             states[NUM_WEAPON_STATE_NAMES];
    int             raiseSound; // Sound played when weapon is raised.
    int             readySound; // Sound played WHILE weapon is readyied.
    int             staticSwitch; // Weapon is not lowered during switch.
} weaponmodeinfo_t;

// Weapon info: sprite frames, ammunition use.
typedef struct {
    weaponmodeinfo_t mode[NUMWEAPLEVELS];
} weaponinfo_t;

DE_EXTERN_C weaponinfo_t weaponInfo[NUM_WEAPON_TYPES][NUM_PLAYER_CLASSES];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the AmmoDef for the specified ammunition @a type; otherwise @c 0.
 */
const AmmoDef *P_AmmoDef(ammotype_t type);

void P_InitWeaponInfo(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __JHERETIC_ITEMS_H__
