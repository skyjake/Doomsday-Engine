/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * p_inventory.h: Common code for player inventory.
 */

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__

#ifndef __COMMON_INVENTORY_H__
#define __COMMON_INVENTORY_H__

#include "common.h"

// Inventory Item Flags:
#define IIF_USE_PANIC           0x1 // Item is usable when panicked.
#define IIF_READY_ALWAYS        0x8 // Item is always "ready" (i.e., usable).

typedef struct {
    byte            flags;
    char            niceName[32];
    char            action[32];
    char            useSnd[32];
    char            patch[9];
    int             hotKeyCtrlIdent;
} def_invitem_t;

typedef struct {
    inventoryitemtype_t type;
    textenum_t      niceName;
    acfnptr_t       action;
    sfxenum_t       useSnd;
    patchid_t       patchId;
} invitem_t;

DENG_EXTERN_C int didUseItem;

#ifdef __cplusplus
extern "C" {
#endif

void            P_InitInventory(void);
void            P_ShutdownInventory(void);

const invitem_t* P_GetInvItem(int id);
const def_invitem_t* P_GetInvItemDef(inventoryitemtype_t type);

void            P_InventoryEmpty(int player);
int             P_InventoryGive(int player, inventoryitemtype_t type,
                                int silent);
int             P_InventoryTake(int player, inventoryitemtype_t type,
                                int silent);
int             P_InventoryUse(int player, inventoryitemtype_t type,
                               int silent);

int             P_InventorySetReadyItem(int player, inventoryitemtype_t type);
inventoryitemtype_t P_InventoryReadyItem(int player);

unsigned int    P_InventoryCount(int player, inventoryitemtype_t type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

#endif
