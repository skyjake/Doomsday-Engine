/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
extern boolean secretExit;
extern int nextMap;
extern skillmode_t gameSkill;
extern int gameEpisode;
extern int gameMap;
extern int nextMap; // If non zero this will be the next map.
extern int prevMap;
extern int totalKills, totalItems, totalSecret;
extern boolean deathmatch;
extern boolean respawnMonsters;
extern boolean userGame;
extern boolean paused;
extern boolean precache;
extern wbstartstruct_t wmInfo;
extern int bodyQueueSlot;
extern int mapStartTic;

extern int gsvMapMusic;

void            G_Register(void);
void            G_CommonPostInit(void);
void            R_InitRefresh(void);

void            G_DeathMatchSpawnPlayer(int playernum);

void            G_PrintMapList(void);
boolean         G_ValidateMap(int* episode, int* map);
int             G_GetMapNumber(int episode, int map);

void            G_InitNew(skillmode_t skill, int episode, int map);

// Can be called by the startup code or Hu_MenuResponder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void            G_DeferedInitNew(skillmode_t skill, int episode, int map);

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

void            G_DoReborn(int playernum);
void            G_PlayerReborn(int player);
void            G_LeaveMap(int map, int position, boolean secret);

void            G_WorldDone(void);

boolean         G_Responder(event_t* ev);

void            G_ScreenShot(void);

void            G_PrepareWIData(void);

void            G_QueueBody(mobj_t* body);

#endif
