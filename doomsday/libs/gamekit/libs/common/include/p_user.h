/** @file p_user.h Common player related logic.
 *
 * Bobbing POV/weapon, movement, pending weapon, etc...
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_PLAYSIM_USER_H
#define LIBCOMMON_PLAYSIM_USER_H

#include "dd_types.h"

#define PLAYER_REBORN_TICS (1 * TICSPERSEC)

DE_EXTERN_C dd_bool onground;

DE_EXTERN_C dd_bool onground;
DE_EXTERN_C float   turboMul; // Multiplier for running speed.
DE_EXTERN_C int     maxHealth;

#if __JDOOM__ || __JDOOM64__
DE_EXTERN_C int healthLimit;
DE_EXTERN_C int godModeHealth;
DE_EXTERN_C int soulSphereLimit;
DE_EXTERN_C int megaSphereHealth;
DE_EXTERN_C int soulSphereHealth;
DE_EXTERN_C int armorPoints[4];
DE_EXTERN_C int armorClass[4];
#endif

#ifdef __cplusplus
extern "C" {
#endif

void            P_Thrust(player_t *player, angle_t angle, coord_t move);
dd_bool         P_IsPlayerOnGround(player_t *player);
void            P_CheckPlayerJump(player_t *player);
void            P_MovePlayer(player_t *player);
void            P_PlayerReborn(player_t *player);
void            P_DeathThink(player_t *player);

void            P_PlayerThink(player_t *player, timespan_t ticLength);
void            P_PlayerThinkState(player_t *player);
void            P_PlayerThinkCheat(player_t *player);
void            P_PlayerThinkAttackLunge(player_t *player);
dd_bool         P_PlayerThinkDeath(player_t *player);
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
void            P_PlayerThinkLookYaw(player_t *player, timespan_t ticLength);
void            P_PlayerThinkLookPitch(player_t *player, timespan_t ticLength);
void            P_PlayerThinkUpdateControls(player_t *player);

#if __JHERETIC__ || __JHEXEN__
void            P_MorphThink(player_t *player);
dd_bool         P_UndoPlayerMorph(player_t *player);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_PLAYSIM_USER_H */
