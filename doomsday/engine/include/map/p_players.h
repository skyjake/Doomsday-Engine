/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * p_players.h: Players
 */

#ifndef __DOOMSDAY_PLAYERS_H__
#define __DOOMSDAY_PLAYERS_H__

#include "api_player.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct player_s {
    byte                extraLightCounter; // Num tics to go till extraLight is disabled.
    int                 extraLight, targetExtraLight;
    ddplayer_t          shared; // The public player data.
} player_t;

extern player_t *viewPlayer;
extern player_t ddPlayers[DDMAXPLAYERS];
extern int consolePlayer;
extern int displayPlayer;

int             P_LocalToConsole(int localPlayer);
int             P_ConsoleToLocal(int playerNum);
int             P_GetDDPlayerIdx(ddplayer_t *ddpl);

boolean         P_IsInVoid(player_t *p);
short           P_LookDirToShort(float lookDir);
float           P_ShortToLookDir(short s);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
