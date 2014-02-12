/** @file player.h  Common playsim routines relating to players.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifdef __cplusplus
extern "C" {
#endif

#if __JHEXEN__
void P_InitPlayerClassInfo(void);
#endif

void P_InitWeaponSlots(void);

void P_FreeWeaponSlots(void);

dd_bool P_SetWeaponSlot(weapontype_t type, byte slot);

byte P_GetWeaponSlot(weapontype_t type);

/**
 * Iterate the weapons of a given weapon slot.
 *
 * @param slot      Weapon slot number.
 * @param reverse   Iff @c = true, the traversal is done in reverse.
 * @param callback  Ptr to the callback to make for each element.
 *                  If the callback returns @c 0, iteration ends.
 * @param context   Passed as an argument to @a callback.
 *
 * @return  Non-zero if no weapon is bound to the slot @a slot, or callback @a callback signals
 * an end to iteration.
 */
int P_IterateWeaponsBySlot(byte slot, dd_bool reverse, int (*callback) (weapontype_t, void *context),
                           void *context);

// A specialized iterator for weapon slot cycling.
weapontype_t P_WeaponSlotCycle(weapontype_t type, dd_bool prev);

/**
 * @param player  The player to work with.
 *
 * @return  Number of the given player.
 */
int P_GetPlayerNum(player_t const *plr);

/**
 * Return a bit field for the current player's cheats.
 *
 * @param  The player to work with.
 *
 * @return  Cheats active for the given player in a bitfield.
 */
int P_GetPlayerCheats(player_t const *plr);

/**
 * Count the number of players currently marked as "in game". Note that an in-game player may not
 * yet have a (CL)Mobj assigned to them, this is a count of players in the current game session.
 *
 * @return  Total count.
 */
int P_CountPlayersInGame(void);

/**
 * Determines whether the player's state is one of the walking states.
 *
 * @param pl  Player whose state to check.
 *
 * @return  @c true, if the player is walking.
 */
dd_bool P_PlayerInWalkState(player_t *plr);

/**
 * Return the next weapon for the given player. Can return the existing
 * weapon if no other valid choices. Preferences are NOT user selectable.
 *
 * @param player  The player to work with.
 * @param prev    Search direction @c true = previous, @c false = next.
 */
weapontype_t P_PlayerFindWeapon(player_t *plr, dd_bool prev);

/**
 * Decides if an automatic weapon change should occur and does it.
 *
 * Called when:
 * - the player has ran out of ammo for the readied weapon.
 * - the player has been given a NEW weapon.
 * - the player is ABOUT TO be given some ammo.
 *
 * If @a weapon is non-zero then we'll always try to change weapon.
 * If @a ammo is non-zero then we'll consider the ammo level of weapons that use this ammo type.
 * If both are non-zero - no more ammo for the current weapon.
 *
 * @todo Should be called AFTER ammo is given but we need to remember the old count before the change.
 *
 * @param player  The player given the weapon.
 * @param weapon  The weapon given to the player (if any).
 * @param ammo    The ammo given to the player (if any).
 * @param force   @c true= Force a weapon change.
 *
 * @return  The weapon we changed to OR WT_NOCHANGE.
 */
weapontype_t P_MaybeChangeWeapon(player_t *plr, weapontype_t weapon, ammotype_t ammo, dd_bool force);

/**
 * Checks if the player has enough ammo to fire their readied weapon.
 * If not, a weapon change is instigated.
 *
 * @return  @c true, if there is enough ammo to fire.
 */
dd_bool P_CheckAmmo(player_t *plr);

/**
 * Subtract the appropriate amount of ammo from the player for firing
 * the current ready weapon.
 *
 * @param player  The player doing the firing.
 */
void P_ShotAmmo(player_t *plr);

#if __JHEXEN__
/**
 * Changes the class of the given player. Will not work if the player
 * is currently morphed.
 */
void P_PlayerChangeClass(player_t *plr, playerclass_t newClass);
#endif

/**
 * Send a message to the given player and maybe echos it to the console.
 *
 * @param player  The player to send the message to.
 * @param flags   @ref logMessageFlags
 * @param msg     The message to be sent.
 */
void P_SetMessage(player_t *plr, int flags, char const *msg);

/**
 * Send a yellow message to the given player and maybe echos it to the console.
 *
 * @param player  The player to send the message to.
 * @param flags   @ref logMessageFlags
 * @param msg     The message to be sent.
 */
#if __JHEXEN__
void P_SetYellowMessage(player_t *plr, int flags, char const *msg);
#endif

/**
 * Set appropriate parameters for a camera.
 */
void P_PlayerThinkCamera(player_t *plr);

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_PlayerSetArmorType(player_t *plr, int type);
#endif

/**
 * Give the player an armor bonus (points delta).
 *
 * @param player  Player to receive the bonus.
 * @param points  Points delta.
 *
 * @return  Number of points applied.
 */
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int P_PlayerGiveArmorBonus(player_t *plr, int points);
#else // __JHEXEN__
int P_PlayerGiveArmorBonus(player_t *plr, armortype_t type, int points);
#endif

int P_CameraXYMovement(mobj_t *mo);

int P_CameraZMovement(mobj_t *mo);

void P_Thrust3D(player_t *player, angle_t angle, float lookdir, coord_t forwardMove, coord_t sideMove);

/**
 * Determine the viewing yaw angle for a player. If a body yaw has been applied to the player,
 * it will be undone here so that during rendering, the actual head tracking angle can be applied.
 *
 * @param playerNum  Player/console number.
 *
 * @return View yaw angle.
 */
angle_t Player_ViewYawAngle(int playerNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_PLAYER_H
