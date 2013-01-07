/**
 * @file d_netcl.h
 * Common code related to netgames (client-side). @ingroup client
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef __D_NETCL_H__
#define __D_NETCL_H__

#include "d_net.h"

void            NetCl_UpdateGameState(Reader* msg);
void            NetCl_PlayerSpawnPosition(Reader* msg);
void            NetCl_UpdatePlayerState(Reader* msg, int plrNum);
void            NetCl_UpdatePlayerState2(Reader* msg, int plrNum);
void            NetCl_UpdatePSpriteState(Reader* msg);
void            NetCl_UpdateJumpPower(Reader* msg);
void            NetCl_Intermission(Reader* msg);
void            NetCl_LocalMobjState(Reader* msg);
/*
void            NetCl_Finale(int packetType, Reader* msg); // moved to engine
*/
void            NetCl_UpdatePlayerInfo(Reader* msg);
void            NetCl_SaveGame(Reader* msg);
void            NetCl_LoadGame(Reader* msg);
void            NetCl_Paused(Reader* msg);
void            NetCl_MobjImpulse(Reader* msg);

void            NetCl_SendPlayerInfo(void);
void            NetCl_PlayerActionRequest(player_t* player, int actionType, int actionParam);
void            NetCl_DamageRequest(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage);
void            NetCl_CheatRequest(const char* command);
void            NetCl_FloorHitRequest(player_t* player);

#endif
