/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * hu_inventory.h: Heads-up display(s) for the player inventory.
 */

#ifndef __COMMON_HUD_INVENTORY__
#define __COMMON_HUD_INVENTORY__

#if __JHERETIC__ || __JHEXEN__

void            Hu_InventoryRegister(void);
void            Hu_InventoryInit(void);
void            Hu_InventoryTicker(void);

void            Hu_InventoryOpen(int player, boolean show);
boolean         Hu_InventoryIsOpen(int player);
boolean         Hu_InventorySelect(int player, inventoryitemtype_t type);
boolean         Hu_InventoryMove(int player, int dir, boolean canWrap,
                                 boolean silent);
void            Hu_InventoryDraw(int player, int x, int y, float textAlpha, float iconAlpha);
void            Hu_InventoryDraw2(int player, int x, int y, float alpha);

/// \todo refactor this away.
void            Hu_InventoryMarkDirty(int player);

#endif

#endif
