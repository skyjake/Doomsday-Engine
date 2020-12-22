/** @file d_netsv.h  Common code related to net games (server-side).
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

#ifndef LIBCOMMON_NETSV_H
#define LIBCOMMON_NETSV_H

#include <de/legacy/reader.h>
#include "common.h"

DE_EXTERN_C char cyclingMaps, mapCycleNoExit;
DE_EXTERN_C int netSvAllowCheats;
DE_EXTERN_C int netSvAllowSendMsg;
DE_EXTERN_C char *mapCycle;
DE_EXTERN_C char gameConfigString[];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Server calls this when new players enter the game.
 */
void NetSv_NewPlayerEnters(int plrNum);

/**
 * Resets a player's frag count and other players' frag counts toward the player.
 *
 * @param plrNum  Player to reset.
 */
void NetSv_ResetPlayerFrags(int plrNum);

void NetSv_SendGameState(int flags, int to);

void NetSv_SendTotalCounts(int to);

/**
 * Sends the initial player position to a client. This is the position defined
 * by the map's start spots. It is sent immediately after the server determines
 * where a player is to spawn.
 */
void NetSv_SendPlayerSpawnPosition(int plrNum, float x, float y, float z, int angle);

void NetSv_SendMessage(int plrNum, const char *msg);

void NetSv_SendYellowMessage(int plrNum, const char *msg);

void NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags, dd_bool reliable);

/**
 * More player state information. Had to be separate because of backwards
 * compatibility.
 */
void NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum, int flags, dd_bool reliable);

void NetSv_TellCycleRulesToPlayerAfterTics(int destPlr, int tics);

/**
 * Informs a player of an impulse momentum that needs to be applied to the player's mobj.
 */
void NetSv_PlayerMobjImpulse(mobj_t *mobj, float mx, float my, float mz);

/**
 * Forcibly dismisses HUDs (automap, inventory) of a particular player.
 *
 * @param player  Player number.
 * @param fast    Quick dismiss.
 */
void NetSv_DismissHUDs(int player, dd_bool fast);
    
/**
 * @param origin
 * @param soundId
 * @param toPlr    @c 0= broadcast.
 */
void NetSv_Sound(mobj_t *origin, int soundId, int toPlr);

void NetSv_SoundAtVolume(mobj_t *origin, int soundId, int volume, int toPlr);

void NetSv_Intermission(int flags, int state, int time);

void NetSv_ChangePlayerInfo(int from, Reader1 *reader);

void NetSv_SendPlayerInfo(int whose, int toWhom);

/**
 * Sharp ticker, i.e., called at 35 Hz.
 */
void NetSv_Ticker(void);

void NetSv_SaveGame(uint gameId);

void NetSv_LoadGame(uint gameId);

void NetSv_LoadReply(int plnum, int console);

/**
 * Sends the frags of player 'whose' to all other players.
 */
void NetSv_FragsForAll(player_t *player);

/**
 * Send one of the kill messages, depending on the weapon of the killer.
 */
void NetSv_KillMessage(player_t *killer, player_t *fragged, dd_bool stomping);

/**
 * Update the game config string with keywords that describe the game.
 * The string is sent out in netgames (also to the master).
 * Keywords: dm, coop, jump, nomonst, respawn, skillN
 */
void NetSv_UpdateGameConfigDescription(void);

/**
 * Inform all clients about a change in the 'pausedness' of a game.
 */
void NetSv_Paused(int pauseState);

/**
 * Process the requested cheat command, if possible.
 */
void NetSv_DoCheat(int player, Reader1 *reader);

void NetSv_ExecuteCheat(int player, const char *command);

/**
 * Process the requested player action, if possible.
 */
void NetSv_DoAction(int player, Reader1 *reader);

void NetSv_DoDamage(int player, Reader1 *reader);

void NetSv_DoFloorHit(int player, Reader1 *msg);

/**
 * The default jump power is 9.
 */
void NetSv_SendJumpPower(int target, float power);

void NetSv_MaybeChangeWeapon(int plrNum, int weapon, int ammo, int force);

void NetSv_SendLocalMobjState(mobj_t *mobj, const char *stateName);

D_CMD(MapCycle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_NETSV_H
