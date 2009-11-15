/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * g_game.h:
 */

#ifndef __G_GAME_H__
#define __G_GAME_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "p_mobj.h"

extern int debugSound; // debug flag for displaying sound info

extern int gameEpisode;
extern int gameMap;
extern skillmode_t gameSkill;
extern boolean deathmatch;
extern boolean userGame;

extern int rebornPosition;
extern int leaveMap;
extern int leavePosition;
extern boolean secretExit;

extern int gsvMapMusic;

void            R_InitRefresh(void);
void            R_GetTranslation(int plrClass, int plrColor, int* tclass,
                                 int* tmap);
void            R_SetTranslation(mobj_t* mo);

void            G_PrintMapList(void);
void            G_PlayerReborn(int player);
void            G_SaveGame(int slot, const char* description);

void            P_GetMapLumpName(int episode, int map, char* lumpName);
#endif
