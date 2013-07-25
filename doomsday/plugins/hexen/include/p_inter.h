/** @file p_inter.h
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_P_INTER_H
#define LIBHEXEN_P_INTER_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "x_player.h"

boolean P_GiveAmmo(player_t *player, ammotype_t ammoType, int numRounds);

boolean P_GiveKey(player_t *player, keytype_t keyType);

/**
 * @return  @c true if the weapon or its ammo was accepted.
 */
boolean P_GiveWeapon2(player_t *player, weapontype_t weaponType, playerclass_t matchClass);
boolean P_GiveWeapon(player_t *player, weapontype_t weaponType/*, playerclass_t matchClass = player->class_*/);

boolean P_GiveWeaponPiece2(player_t *player, int pieceValue, playerclass_t matchClass);
boolean P_GiveWeaponPiece(player_t *player, int pieceValue/*, playerclass_t matchClass = player->class_*/);

boolean P_GiveArmor(player_t *player, armortype_t armorType);

boolean P_GiveArmorAlt(player_t *player, armortype_t armorType, int armorPoints);

boolean P_GiveHealth(player_t *player, int amount);

boolean P_GivePower(player_t *player, powertype_t powerType);

boolean P_MorphPlayer(player_t *player);

#endif // LIBHEXEN_P_INTER_H
