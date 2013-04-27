/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_NETSV_H
#define LIBCOMMON_NETSV_H

#include <de/reader.h>
#include "common.h"

DENG_EXTERN_C char cyclingMaps, mapCycleNoExit;
DENG_EXTERN_C int netSvAllowCheats;
DENG_EXTERN_C char* mapCycle;
DENG_EXTERN_C char gameConfigString[];

#ifdef __cplusplus
extern "C" {
#endif

void            P_Telefrag(mobj_t* thing);

void            NetSv_NewPlayerEnters(int plrNum);
void            NetSv_ResetPlayerFrags(int plrNum);
void            NetSv_SendGameState(int flags, int to);
void            NetSv_SendTotalCounts(int to);
void            NetSv_SendPlayerSpawnPosition(int plrNum, float x, float y, float z, int angle);
void            NetSv_SendMessage(int plrNum, const char *msg);
void            NetSv_SendYellowMessage(int plrNum, const char *msg);
void            NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags,
                                      boolean reliable);
void            NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum,
                                       int flags, boolean reliable);
void            NetSv_TellCycleRulesToPlayerAfterTics(int destPlr, int tics);
void            NetSv_PlayerMobjImpulse(mobj_t* mobj, float mx, float my, float mz);
void            NetSv_Sound(mobj_t *origin, int sound_id, int toPlr);   // toPlr=0: broadcast.
void            NetSv_SoundAtVolume(mobj_t *origin, int sound_id, int volume,
                                    int toPlr);
void            NetSv_Intermission(int flags, int state, int time);
/*
void            NetSv_Finale(int flags, const char* script, const boolean* conds,
                             byte numConds); // moved to engine
 */
void            NetSv_ChangePlayerInfo(int from, Reader* reader);
void            NetSv_SendPlayerInfo(int whose, int to_whom);
void            NetSv_Ticker(void);
void            NetSv_SaveGame(unsigned int game_id);
void            NetSv_LoadGame(unsigned int game_id);
void            NetSv_LoadReply(int plnum, int console);
void            NetSv_FragsForAll(player_t* player);
void            NetSv_KillMessage(player_t* killer, player_t* fragged, boolean stomping);
void            NetSv_UpdateGameConfigDescription(void);

/**
 * Inform all clients about a change in the 'pausedness' of a game.
 */
void NetSv_Paused(int pauseState);

void            NetSv_DoCheat(int player, Reader *reader);
void            NetSv_ExecuteCheat(int player, const char* command);
void            NetSv_DoAction(int player, Reader *reader);
void            NetSv_DoDamage(int player, Reader *reader);
void            NetSv_DoFloorHit(int player, Reader* msg);
void            NetSv_SendJumpPower(int target, float power);
void            NetSv_MaybeChangeWeapon(int plrNum, int weapon, int ammo, int force);
void            NetSv_SendLocalMobjState(mobj_t* mobj, const char* stateName);

D_CMD(MapCycle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_NETSV_H */
