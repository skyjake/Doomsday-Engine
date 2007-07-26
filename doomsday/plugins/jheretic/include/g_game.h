/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

#ifndef __JHERETIC_GAME_H__
#define __JHERETIC_GAME_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"
#include "h_event.h"
#include "h_player.h"

//
// GAME
//
void            G_Register(void);

void            G_DeathMatchSpawnPlayer(int playernum);

boolean         G_ValidateMap(int *episode, int *map);
int             G_GetLevelNumber(int episode, int map);

void            G_InitNew(skillmode_t skill, int episode, int map);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void            G_DeferedInitNew(skillmode_t skill, int episode, int map);

void            G_DeferedPlayDemo(char *demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void            G_LoadGame(char *name);

void            G_DoLoadGame(void);

// Called by M_Responder.
void            G_SaveGame(int slot, char *description);

// Only called by startup code.
void            G_RecordDemo(skillmode_t skill, int numplayers, int episode,
                             int map, char *name);

void            G_PlayDemo(char *name);
void            G_TimeDemo(char *name);
void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

void            G_DoReborn(int playernum);
void            G_LeaveLevel(int map, int position, boolean secret);

void            G_WorldDone(void);

void            G_Ticker(timespan_t tickDuration);
boolean         G_Responder(event_t *ev);

void            G_ScreenShot(void);
#endif
