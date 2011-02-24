/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * d_netcl.h:
 */

#ifndef __D_NETCL_H__
#define __D_NETCL_H__

#include "d_net.h"

void*           NetCl_WriteCommands(ticcmd_t* cmd, int count);
void            NetCl_UpdateGameState(byte* data);
void            NetCl_CheatRequest(const char* command);
void            NetCl_UpdatePlayerState(byte* data, int plrNum);
void            NetCl_UpdatePlayerState2(byte* data, int plrNum);
void            NetCl_UpdatePSpriteState(byte* data);
void            NetCl_UpdateJumpPower(void* data);
void            NetCl_Intermission(byte* data);
void            NetCl_Finale(int packetType, byte* data);
void            NetCl_UpdatePlayerInfo(byte* data);
void            NetCl_SendPlayerInfo(void);
void            NetCl_SaveGame(void* data);
void            NetCl_LoadGame(void* data);
void            NetCl_Paused(boolean setPause);
void            NetCl_PlayerActionRequest(player_t* player, int actionType);

#endif
