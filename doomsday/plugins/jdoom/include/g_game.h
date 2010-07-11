/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * g_game.h: Top-level (common) game routines jDoom - specific.
 */

#ifndef __G_GAME_H__
#define __G_GAME_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomdef.h"
#include "d_event.h"
#include "d_player.h"

extern player_t players[MAXPLAYERS];
extern uint nextMap;
extern skillmode_t gameSkill;
extern uint gameEpisode;
extern uint gameMap;
extern uint nextMap; // If non zero this will be the next map.
extern boolean secretExit;
extern int totalKills, totalItems, totalSecret;
extern boolean deathmatch;
extern boolean respawnMonsters;
extern boolean userGame;
extern boolean paused;
extern boolean precache;
extern boolean customPal;
extern wbstartstruct_t wmInfo;
extern int bodyQueueSlot;
extern int mapStartTic;
extern boolean briefDisabled;

extern int gsvMapMusic;

void            G_Register(void);
void            G_CommonPreInit(void);
void            G_CommonPostInit(void);
void            R_InitRefresh(void);

void            G_PrintMapList(void);
boolean         G_ValidateMap(uint* episode, uint* map);
uint            G_GetMapNumber(uint episode, uint map);

void            G_InitNew(skillmode_t skill, uint episode, uint map);

// Can be called by the startup code or Hu_MenuResponder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void            G_DeferedInitNew(skillmode_t skill, uint episode, uint map);

void            G_DeferedPlayDemo(char* demo);

// Can be called by the startup code or Hu_MenuResponder,
// calls P_SetupMap or W_EnterWorld.
void            G_LoadGame(const char* name);

void            G_DoLoadGame(void);

// Called by Hu_MenuResponder.
void            G_SaveGame(int slot, const char* description);

void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

int             G_BriefingEnabled(uint episode, uint map, ddfinale_t* fin);
int             G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin);

void            G_DoReborn(int playernum);
void            G_PlayerReborn(int player);
void            G_LeaveMap(uint newMap, uint entryPoint, boolean secretExit);

uint            G_GetNextMap(uint episode, uint map, boolean secretExit);

boolean         P_MapExists(uint episode, uint map);
const char*     P_MapSourceFile(uint episode, uint map);
void            P_MapId(uint episode, uint map, char* name);

void            G_WorldDone(void);

void            G_Ticker(timespan_t ticLength);
boolean         G_Responder(event_t* ev);

void            G_ScreenShot(void);

void            G_PrepareWIData(void);

void            G_QueueBody(mobj_t* body);
#endif
