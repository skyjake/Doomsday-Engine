/**\file g_game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Top-level (common) game routines jHeretic - specific.
 */

#ifndef LIBJHERETIC_G_GAME_H
#define LIBJHERETIC_G_GAME_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"
#include "h_event.h"
#include "h_player.h"

extern int gaSaveGameSaveSlot;
extern int gaLoadGameSaveSlot;

extern boolean deathmatch;
extern boolean respawnMonsters;
extern boolean userGame;
extern player_t players[MAXPLAYERS];

extern skillmode_t gameSkill;
extern uint gameEpisode;
extern uint gameMap;
extern uint gameMapEntryPoint;

extern uint nextMap;
extern boolean secretExit;
extern int mapStartTic;
extern int totalKills, totalItems, totalSecret;
extern wbstartstruct_t wmInfo;
extern boolean customPal;
extern boolean briefDisabled;

extern int gsvMapMusic;

void            G_Register(void);
void            G_CommonPreInit(void);
void            G_CommonPostInit(void);
void            G_CommonShutdown(void);
void            R_InitRefresh(void);

void            G_DeathMatchSpawnPlayer(int playernum);

void            G_PrintMapList(void);
boolean         G_ValidateMap(uint* episode, uint* map);
uint            G_GetMapNumber(uint episode, uint map);

void            G_DeferedPlayDemo(char* demo);

void            G_QuitGame(void);

/// @return  @c true = loading is presently possible.
boolean G_IsLoadGamePossible(void);

/**
 * To be called to schedule a load game-save action.
 * @param slot  Logical identifier of the save slot to use.
 * @return  @c true iff @a saveSlot is in use and loading is presently possible.
 */
boolean G_LoadGame(int slot);

/// @return  @c true = saving is presently possible.
boolean G_IsSaveGamePossible(void);

/**
 * To be called to schedule a save game-save action.
 * @param slot  Logical identifier of the save slot to use.
 * @param name  New name for the game-save. Can be @c NULL in which case
 *      the name will not change if the slot has already been used.
 *      If an empty string a new name will be generated automatically.
 * @return  @c true iff @a saveSlot is valid and saving is presently possible.
 */
boolean G_SaveGame2(int slot, const char* name);
boolean G_SaveGame(int slot);

void            G_StopDemo(void);

int             G_BriefingEnabled(uint episode, uint map, ddfinale_t* fin);
int             G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin);

void            G_DoReborn(int playernum);
void            G_PlayerReborn(int player);
void            G_LeaveMap(uint newMap, uint entryPoint, boolean secretExit);

uint            G_GetNextMap(uint episode, uint map, boolean secretExit);

/**
 * Compose a Uri for the identified @a episode and @a map combination.
 *
 * @param episode  Logical episode number.
 * @param map  Logical map number.
 * @return  Resultant Uri. Caller should destroy with Uri_Delete.
 */
Uri* G_ComposeMapUri(uint episode, uint map);

/**
 * Compose the name of the map identifier.
 * \note Deprecated. Prefer to use G_ComposeMapUri
 */
void G_MapId(uint episode, uint map, lumpname_t mapId);

void            G_WorldDone(void);

void            G_Ticker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int G_PrivilegedResponder(event_t* ev);

/// @return  @c true if the input event @a ev was eaten.
int G_Responder(event_t* ev);

void            G_ScreenShot(void);

#endif /* LIBJHERETIC_G_GAME_H */
