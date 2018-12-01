/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_inter.h: Handling mobj vs mobj interactions (i.e., collisions).
 */

#ifndef __P_INTER_H__
#define __P_INTER_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "jdoom64.h"

DE_EXTERN_C int maxAmmo[];
DE_EXTERN_C int clipAmmo[];

#ifdef __cplusplus
extern "C" {
#endif

dd_bool         P_GivePower(player_t*,      int);
dd_bool         P_TakePower(player_t*,      int);
dd_bool         P_TogglePower(player_t *,   powertype_t);

void            P_GiveKey(player_t* plr, keytype_t keyType);
dd_bool         P_GiveBody(player_t* plr, int num);
void            P_GiveBackpack(player_t* plr);
dd_bool         P_GiveWeapon(player_t* plr, weapontype_t weapon, dd_bool dropped);
dd_bool         P_GiveArmor(player_t* plr, int type, int points);
dd_bool         P_GiveAmmo(player_t *player, ammotype_t ammo, int num);

void            P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher);
int             P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, dd_bool stomping);
int             P_DamageMobj2(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, dd_bool stomping, dd_bool skipNetworkCheck);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
