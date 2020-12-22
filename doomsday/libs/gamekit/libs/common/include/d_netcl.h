/** @file d_netcl.h  Common code related to netgames (client-side).
 * @ingroup client
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_D_NETCL_H
#define LIBCOMMON_D_NETCL_H

#include "common.h"
#include <de/legacy/reader.h>

#ifdef __cplusplus
extern "C" {
#endif

void NetCl_UpdateGameState(Reader1 *msg);

void NetCl_PlayerSpawnPosition(Reader1 *msg);

void NetCl_UpdateTotalCounts(Reader1 *msg);

void NetCl_UpdatePlayerState(Reader1 *msg, int plrNum);

void NetCl_UpdatePlayerState2(Reader1 *msg, int plrNum);

void NetCl_UpdatePSpriteState(Reader1 *msg);

/**
 * Set the jump power used in client mode.
 */
void NetCl_UpdateJumpPower(Reader1 *msg);
    
void NetCl_DismissHUDs(Reader1 *msg);

void NetCl_Intermission(Reader1 *msg);

void NetCl_LocalMobjState(Reader1 *msg);

/**
 * Clients have other players' info, but it's only "FYI"; they don't really need it.
 */
void NetCl_UpdatePlayerInfo(Reader1 *msg);

void NetCl_SaveGame(Reader1 *msg);

void NetCl_LoadGame(Reader1 *msg);

void NetCl_Paused(Reader1 *msg);

void NetCl_MobjImpulse(Reader1 *msg);

void NetCl_SendPlayerInfo(void);

/**
 * Sends a player action request. The server will execute the action.
 * This is more reliable than sending via the ticcmds, as the client will
 * determine exactly when and where the action takes place. On serverside,
 * the clients position and angle may not be up to date when a ticcmd
 * arrives.
 */
void NetCl_PlayerActionRequest(player_t *player, int actionType, int actionParam);

void NetCl_DamageRequest(mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage);

/**
 * Send a GPT_CHEAT_REQUEST packet to the server. If the server is allowing netgame cheating,
 * the cheat will be executed on the server.
 */
void NetCl_CheatRequest(const char *command);

void NetCl_FloorHitRequest(player_t *player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_D_NETCL_H
