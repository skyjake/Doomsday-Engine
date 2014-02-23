/**\file g_game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * Top-level (common) game routines jDoom64 - specific.
 */

#ifndef LIBJDOOM64_G_GAME_H
#define LIBJDOOM64_G_GAME_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomdef.h"
#include "d_player.h"
#include "gamerules.h"
#include "wi_stuff.h"

DENG_EXTERN_C int gaSaveGameSaveSlot;
DENG_EXTERN_C int gaLoadGameSaveSlot;

DENG_EXTERN_C player_t players[MAXPLAYERS];
DENG_EXTERN_C uint nextMap;

DENG_EXTERN_C dd_bool gameInProgress;
DENG_EXTERN_C uint gameEpisode;
DENG_EXTERN_C uint gameMap;
DENG_EXTERN_C Uri *gameMapUri;
DENG_EXTERN_C uint gameMapEntrance;
DENG_EXTERN_C GameRuleset gameRules;

DENG_EXTERN_C uint nextMap; // If non zero this will be the next map.
DENG_EXTERN_C dd_bool secretExit;
DENG_EXTERN_C int totalKills, totalItems, totalSecret;
DENG_EXTERN_C wbstartstruct_t wmInfo;
DENG_EXTERN_C int bodyQueueSlot;
DENG_EXTERN_C dd_bool paused;
DENG_EXTERN_C dd_bool precache;
DENG_EXTERN_C dd_bool customPal;
DENG_EXTERN_C int gsvMapMusic;
DENG_EXTERN_C dd_bool briefDisabled;

#ifdef __cplusplus
extern "C" {
#endif

void G_Register(void);
void G_CommonPreInit(void);
void G_CommonPostInit(void);
void G_CommonShutdown(void);

void R_InitRefresh(void);
void G_DeathMatchSpawnPlayer(int playernum);

void G_PrintMapList(void);

void G_DeferredPlayDemo(char* demo);

void G_QuitGame(void);

/// @return  @c true = loading is presently possible.
dd_bool G_IsLoadGamePossible(void);

/// @return  @c true = saving is presently possible.
dd_bool G_IsSaveGamePossible(void);

void G_StopDemo(void);

int G_BriefingEnabled(Uri const *mapUri, ddfinale_t *fin);
int G_DebriefingEnabled(Uri const *mapUri, ddfinale_t *fin);

// Confusing no?
void G_DoReborn(int playernum);
void G_PlayerReborn(int player);

void G_IntermissionDone(void);

void G_Ticker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int G_PrivilegedResponder(event_t* ev);

/// @return  @c true if the input event @a ev was eaten.
int G_Responder(event_t* ev);

void G_ScreenShot(void);

void G_PrepareWIData(void);

void G_QueueBody(mobj_t* body);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBJDOOM64_G_GAME_H */
