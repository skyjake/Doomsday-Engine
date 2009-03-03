/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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

#if __JHERETIC__ || __JHEXEN__

#ifndef __COMMON_INVENTORY_H__
#define __COMMON_INVENTORY_H__

#if __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

extern boolean useArti;

boolean         P_InventoryGive(player_t* player, artitype_e arti);
void            P_InventoryTake(player_t* player, artitype_e arti);

boolean         P_InventoryUse(player_t* player, artitype_e arti);
uint            P_InventoryCount(player_t* player, artitype_e arti);

#endif
#endif
