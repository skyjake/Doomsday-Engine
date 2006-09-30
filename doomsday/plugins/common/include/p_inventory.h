/* DE1: $Id: template.c 2645 2006-01-21 12:58:39Z skyjake $
 * Copyright (C) 1999- Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * p_inventory.h: Common code for player inventory
 *
 * Compiles for jHeretic and jHexen
 */

#if __JHERETIC__ || __JHEXEN__

#ifndef __COMMON_INVENTORY_H__
#define __COMMON_INVENTORY_H__

#if __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

extern boolean artiskip;
extern boolean usearti;

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
