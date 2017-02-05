/** @file hu_inventory.h  HUD player inventory widget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_HUD_INVENTORY
#define LIBCOMMON_HUD_INVENTORY
#if __JHERETIC__ || __JHEXEN__

#ifdef __cplusplus
extern "C" {
#endif

void Hu_InventoryInit(void);

void Hu_InventoryTicker(void);

void Hu_InventoryOpen(int player, dd_bool show);

dd_bool Hu_InventoryIsOpen(int player);

dd_bool Hu_InventorySelect(int player, inventoryitemtype_t type);

dd_bool Hu_InventoryMove(int player, int dir, dd_bool canWrap, dd_bool silent);

void Hu_InventoryDraw(int player, int x, int y, float textOpacity, float iconOpacity);
void Hu_InventoryDraw2(int player, int x, int y, float alpha);

/**
 * Mark the HUD inventory as dirty (i.e., the player inventory state has
 * changed in such a way that would require the HUD inventory display(s)
 * to be updated e.g., the player gains a new item).
 *
 * @param player        Player whoose in HUD inventory is dirty.
 *
 * @todo refactor away.
 */
void Hu_InventoryMarkDirty(int player);

/**
 * Register the console commands and variables of this module.
 */
void Hu_InventoryRegister(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __JHERETIC__ || __JHEXEN__
#endif // LIBCOMMON_HUD_INVENTORY
