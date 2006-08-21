/* DE1: $Id: template.c 2645 2006-01-21 12:58:39Z skyjake $
 * Copyright (C) 2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *                    (add more authors here)
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_player.h: Common playsim routines relating to players.
 */

#if  __DOOM64TC__
# include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

int         P_GetPlayerNum(player_t *player);

weapontype_t P_PlayerFindWeapon(player_t *player, boolean next);
weapontype_t P_MaybeChangeWeapon(player_t *player, weapontype_t weapon,
                                 ammotype_t ammo, boolean force);
void        P_ShotAmmo(player_t *player);

void        P_PlayerThink(player_t *player);
void        P_RaiseDeadPlayer(player_t *player);

void        P_SetMessage(player_t *pl, char *msg, boolean noHide);
#if __JHEXEN__ || __JSTRIFE__
void        P_SetYellowMessage(player_t *pl, char *msg, boolean noHide);
#endif
void        P_ClearMessage(player_t *player);

boolean     P_IsCamera(mobj_t *mo);
void        P_PlayerThinkCamera(player_t *player);
