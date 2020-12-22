/** @file p_inventory.h  Common code for player inventory.
 *
 * @note The visual representation of the inventory is handled elsewhere.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
#ifndef LIBCOMMON_PLAY_INVENTORY_H
#define LIBCOMMON_PLAY_INVENTORY_H

#include "common.h"

// Inventory Item Flags:
#define IIF_USE_PANIC           0x1 // Item is usable when panicked.
#define IIF_READY_ALWAYS        0x8 // Item is always "ready" (i.e., usable).

typedef struct {
    int gameModeBits; // Game modes the item is available in.
    byte flags;
    char niceName[32];
    char action[32];
    char useSnd[32];
    char patch[9];
    int hotKeyCtrlIdent;
} def_invitem_t;

typedef struct {
    inventoryitemtype_t type;
    textenum_t niceName;
    acfnptr_t action;
    sfxenum_t useSnd;
    patchid_t patchId;
} invitem_t;

DE_EXTERN_C int didUseItem;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Called during (post-game) init and after updating game/engine state.
 */
void P_InitInventory(void);

/**
 * Called once, during shutdown.
 */
void P_ShutdownInventory(void);

const invitem_t *P_GetInvItem(int id);

const def_invitem_t *P_GetInvItemDef(inventoryitemtype_t type);

/**
 * Should be called only outside of normal play (e.g., when starting a new
 * game or similar).
 *
 * @param player  Player to empty the inventory of.
 */
void P_InventoryEmpty(int player);

/**
 * Give one item of the specified type to the player.
 *
 * @param player  Player to give the item to.
 * @param type    Type of item to give.
 * @param silent  Non-zero = don't alert the player.
 *
 * @return  Non-zero iff an item of the specified type was successfully given to the player.
 */
int P_InventoryGive(int player, inventoryitemtype_t type, int silent);

/**
 * Take one of the specified items from the player (if owned).
 *
 * @param player  Player to take the item from.
 * @param type    Type of item being taken.
 * @param silent  Non-zero = don't alert the player.
 *
 * @return  Non-zero iff an item of the specified type was successfully taken from the player.
 */
int P_InventoryTake(int player, inventoryitemtype_t type, int silent);

/**
 * Attempt to use an item of the specified type.
 *
 * @param player  Player using the item.
 * @param type    The type of item being used ELSE
 *                IIT_NONE = Ignored.
 *                NUM_INVENTORYITEM_TYPES = Panic. Use one of everything!!
 * @param silent  Non-zero - don't alert the player.
 *
 * @return  Non-zero iff one (or more) item was successfully used by the player.
 */
int P_InventoryUse(int player, inventoryitemtype_t type, int silent);

/**
 * Attempt to ready an item (or unready a currently readied item).
 *
 * @param player  Player to change the ready item of.
 * @param type    Item type to be readied.
 *
 * @return  Non-zero iff successful.
 */
int P_InventorySetReadyItem(int player, inventoryitemtype_t type);

/**
 * Returns the currently readied item, else @c IIT_NONE.
 */
inventoryitemtype_t P_InventoryReadyItem(int player);

/**
 * Returns the number of items possessed by the player.
 *
 * @param player  Player to check the inventory of.
 * @param type    Type requirement, limits the count to the specified type.
 *                @c IIT_NONE= count all.
 */
uint P_InventoryCount(int player, inventoryitemtype_t type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_PLAY_INVENTORY_H
#endif // __JHERETIC__ || __JHEXEN__ || __JDOOM64__
