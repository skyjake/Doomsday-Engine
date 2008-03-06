/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
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

boolean         P_GiveArtifact(player_t *player, artitype_e arti, mobj_t *mo);

void            P_InventoryResetCursor(player_t *player);

void            P_InventoryRemoveArtifact(player_t *player, int slot);
boolean         P_InventoryUseArtifact(player_t *player, artitype_e arti);
void            P_InventoryNextArtifact(player_t *player);

#if __JHERETIC__
void            P_InventoryCheckReadyArtifact(player_t *player);
#endif

boolean         P_UseArtifactOnPlayer(player_t *player, artitype_e arti);

DEFCC(CCmdInventory);

#endif
#endif
