/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_user.c : Player related stuff.
 *
 * Bobbing POV/weapon, movement, pending weapon...
 * Compiles for jDoom, jHeretic and jHexen.
 */

#ifndef __P_USER_H__
#define __P_USER_H__

#include "dd_types.h"

#define PLAYER_REBORN_TICS      (1*TICSPERSEC)

extern boolean onground;

extern int maxHealth;

#if __JDOOM__ || __JDOOM64__
extern int healthLimit;
extern int godModeHealth;
extern int soulSphereLimit;
extern int megaSphereHealth;
extern int soulSphereHealth;
extern int armorPoints[4];
extern int armorClass[4];
#endif

extern classinfo_t classInfo[];

void            P_Thrust(player_t *player, angle_t angle, coord_t move);
boolean         P_IsPlayerOnGround(player_t *player);
void            P_CheckPlayerJump(player_t *player);
void            P_MovePlayer(player_t *player);
void            P_PlayerReborn(player_t *player);
void            P_DeathThink(player_t *player);

void            P_PlayerThink(player_t *player, timespan_t ticLength);
void            P_PlayerThinkState(player_t *player);
void            P_PlayerThinkCheat(player_t *player);
void            P_PlayerThinkAttackLunge(player_t *player);
boolean         P_PlayerThinkDeath(player_t *player);
void            P_PlayerThinkMorph(player_t *player);
void            P_PlayerThinkMove(player_t *player);
void            P_PlayerThinkFly(player_t *player);
void            P_PlayerThinkJump(player_t *player);
void            P_PlayerThinkView(player_t *player);
void            P_PlayerThinkSpecial(player_t *player);
void            P_PlayerThinkSounds(player_t *player);
#if __JHERETIC__ || __JHEXEN__
void            P_PlayerThinkInventory(player_t* player);
#endif
void            P_PlayerThinkItems(player_t *player);
void            P_PlayerThinkWeapons(player_t *player);
void            P_PlayerThinkUse(player_t *player);
void            P_PlayerThinkPsprites(player_t *player);
void            P_PlayerThinkPowers(player_t *player);
void            P_PlayerThinkLookYaw(player_t *player);
void            P_PlayerThinkLookPitch(player_t *player, timespan_t ticLength);
void            P_PlayerThinkUpdateControls(player_t* player);

#if __JHERETIC__ || __JHEXEN__
void            P_MorphThink(player_t *player);
boolean         P_UndoPlayerMorph(player_t *player);
#endif

#endif
