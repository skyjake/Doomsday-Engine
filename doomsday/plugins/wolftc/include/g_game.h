/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * g_game.h: Top-level (common) game routines WolfTC - specific.
 */

#ifndef __G_GAME_H__
#define __G_GAME_H__

#ifndef __WOLFTC__
#  error "Using WolfTC headers without __WOLFTC__"
#endif

#include "doomdef.h"
#include "d_event.h"
#include "d_player.h"

extern player_t players[MAXPLAYERS];
extern boolean secretExit;
extern int nextMap;
extern int totalKills, totalItems, totalSecret;

void            G_Register(void);
void            G_CommonPreInit(void);
void            G_CommonPostInit(void);

void            G_DeathMatchSpawnPlayer(int playernum);

void            G_PrintMapList(void);
boolean         G_ValidateMap(int* episode, int* map);
int             G_GetLevelNumber(int episode, int map);

void            G_InitNew(skillmode_t skill, int episode, int map);

// Can be called by the startup code or Hu_MenuResponder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void            G_DeferedInitNew(skillmode_t skill, int episode, int map);

void            G_DeferedPlayDemo(char* demo);

// Can be called by the startup code or Hu_MenuResponder,
// calls P_SetupLevel or W_EnterWorld.
void            G_LoadGame(char* name);

void            G_DoLoadGame(void);

// Called by Hu_MenuResponder.
void            G_SaveGame(int slot, char* description);

// Only called by startup code.
void            G_RecordDemo(char* name);

void            G_BeginRecording(void);

void            G_PlayDemo(char* name);
void            G_TimeDemo(char* name);
void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

void            G_DoReborn(int playernum);
void            G_LeaveLevel(int map, int position, boolean secret);

void            G_WorldDone(void);

void            G_Ticker(timespan_t ticLength);
boolean         G_Responder(event_t* ev);

void            G_ScreenShot(void);

void            G_PrepareWIData(void);

void            G_QueueBody(mobj_t* body);

#endif
