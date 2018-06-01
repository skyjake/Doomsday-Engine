/**\file g_game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOM64_G_GAME_H
#define LIBDOOM64_G_GAME_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomdef.h"
#include "d_player.h"
#include "gamerules.h"
#include "intermission.h"

DE_EXTERN_C player_t players[MAXPLAYERS];

DE_EXTERN_C int totalKills, totalItems, totalSecret;
DE_EXTERN_C int bodyQueueSlot;
DE_EXTERN_C dd_bool paused;
DE_EXTERN_C dd_bool precache;
DE_EXTERN_C dd_bool customPal;
DE_EXTERN_C dd_bool briefDisabled;

#ifdef __cplusplus

extern bool secretExit;

extern wbstartstruct_t wmInfo;

extern "C" {
#endif

void G_ConsoleRegister(void);
void G_CommonPreInit(void);
void G_CommonPostInit(void);

/**
 * To be called post-game initialization, to examine the command line to determine if
 * a new game session should be started automatically, or, begin the title loop.
 */
void G_AutoStartOrBeginTitleLoop(void);

void G_CommonShutdown(void);

void R_InitRefresh(void);
void G_DeathMatchSpawnPlayer(int playernum);

void G_DeferredPlayDemo(char* demo);

void G_QuitGame(void);

void G_StopDemo(void);

// Confusing no?
//void G_DoReborn(int playernum);
void G_PlayerReborn(int player);

void G_Ticker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int G_PrivilegedResponder(event_t* ev);

/// @return  @c true if the input event @a ev was eaten.
int G_Responder(event_t* ev);

void G_PrepareWIData(void);

void G_QueueBody(mobj_t* body);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDOOM64_G_GAME_H */
