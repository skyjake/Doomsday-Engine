/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

extern int maxAmmo[];
extern int clipAmmo[];

boolean         P_GivePower(player_t* plr, int);
boolean         P_TakePower(player_t* plr, int power);
void            P_GiveKey(player_t* plr, keytype_t keyType);
boolean         P_GiveBody(player_t* plr, int num);
void            P_GiveBackpack(player_t* plr);
boolean         P_GiveWeapon(player_t* plr, weapontype_t weapon, boolean dropped);
boolean         P_GiveArmor(player_t* plr, int type, int points);

void            P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher);
int             P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, boolean stomping);
int             P_DamageMobj2(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, boolean stomping, boolean skipNetworkCheck);

#endif
