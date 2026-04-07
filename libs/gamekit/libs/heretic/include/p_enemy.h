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

/**
 * p_enemy.h: Enemy thinking, AI.
 */

#ifndef __P_ENEMY_H__
#define __P_ENEMY_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void        P_ClearBodyQueue(void);
int         P_Massacre(void);
void        P_NoiseAlert(mobj_t* target, mobj_t* emitter);
int         P_Attack(mobj_t *actor, int meleeDamage, mobjtype_t missileType);
void        P_InitWhirlwind(mobj_t *whirlwind, mobj_t *target);
void        P_DSparilTeleport(mobj_t* actor);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
