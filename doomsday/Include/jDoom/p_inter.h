/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

#ifndef __P_INTER__
#define __P_INTER__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifdef __GNUG__
#pragma interface
#endif

boolean         P_GivePower(player_t *, int);
boolean         P_TakePower(player_t *player, int power);
void            P_GiveKey(player_t *player, card_t card);
boolean         P_GiveBody(player_t *player, int num);
void            P_GiveBackpack(player_t *player);
boolean         P_GiveWeapon(player_t *player, weapontype_t weapon, boolean dropped);

#endif
