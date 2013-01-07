/**\file p_player.h
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
 * Common playsim routines relating to players.
 */

#ifndef LIBCOMMON_PLAYER_H
#define LIBCOMMON_PLAYER_H

#include "common.h"
#include "hu_log.h" /// for @ref logMessageFlags

#if __JDOOM64__
#define NUM_WEAPON_SLOTS        (8)
#elif __JDOOM__ || __JHERETIC__
#define NUM_WEAPON_SLOTS        (7)
#elif __JHEXEN__
#define NUM_WEAPON_SLOTS        (4)
#endif

#if __JHEXEN__
void            P_InitPlayerClassInfo(void);
#endif

void            P_InitWeaponSlots(void);
void            P_FreeWeaponSlots(void);

boolean         P_SetWeaponSlot(weapontype_t type, byte slot);
byte            P_GetWeaponSlot(weapontype_t type);

int             P_IterateWeaponsBySlot(byte slot, boolean reverse,
                                       int (*callback) (weapontype_t, void* context),
                                        void* context);
// A specialized iterator for weapon slot cycling.
weapontype_t    P_WeaponSlotCycle(weapontype_t type, boolean prev);

int             P_GetPlayerNum(player_t* plr);
int             P_GetPlayerCheats(const player_t* plr);

/**
 * Count the number of players currently marked as "in game". Note that
 * an in-game player may not yet have a (CL)Mobj assigned to them, this
 * is a count of players in the current game session.
 *
 * @return  Total count.
 */
int P_CountPlayersInGame(void);

boolean         P_PlayerInWalkState(player_t* plr);

weapontype_t    P_PlayerFindWeapon(player_t* plr, boolean prev);
weapontype_t    P_MaybeChangeWeapon(player_t* plr, weapontype_t weapon,
                                    ammotype_t ammo, boolean force);
boolean         P_CheckAmmo(player_t* plr);
void            P_ShotAmmo(player_t* plr);

#if __JHEXEN__
void            P_PlayerChangeClass(player_t* plr, playerclass_t newClass);
#endif

/**
 * Send a message to the given player and maybe echos it to the console.
 *
 * @param player        The player to send the message to.
 * @param flags         @ref logMessageFlags
 * @param msg           The message to be sent.
 */
void            P_SetMessage(player_t* plr, int flags, const char* msg);

/**
 * Send a yellow message to the given player and maybe echos it to the console.
 *
 * @param player        The player to send the message to.
 * @param flags         @ref logMessageFlags
 * @param msg           The message to be sent.
 */
#if __JHEXEN__
void            P_SetYellowMessage(player_t* plr, int flags, const char* msg);
#endif

void            P_PlayerThinkCamera(player_t* plr);

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void            P_PlayerSetArmorType(player_t* plr, int type);
int             P_PlayerGiveArmorBonus(player_t* plr, int points);
#else // __JHEXEN__
int             P_PlayerGiveArmorBonus(player_t* plr, armortype_t type, int points);
#endif

int             P_CameraXYMovement(mobj_t* mo);
int             P_CameraZMovement(mobj_t* mo);
void            P_Thrust3D(struct player_s* player, angle_t angle, float lookdir, coord_t forwardMove, coord_t sideMove);

#endif /* LIBCOMMON_PLAYER_H */
