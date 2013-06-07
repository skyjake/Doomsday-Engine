/** @file world/p_players.h: World Player Entities.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_P_PLAYERS_H
#define DENG_WORLD_P_PLAYERS_H

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

int P_LocalToConsole(int localPlayer);
int P_ConsoleToLocal(int playerNum);
int P_GetDDPlayerIdx(ddplayer_t *ddpl);

/**
 * Do we THINK the given (camera) player is currently in the void.
 * The method used to test this is to compare the position of the mobj
 * each time it is linked into a BSP leaf.
 *
 * @note Cannot be 100% accurate so best not to use it for anything critical...
 *
 * @param player  The player to test.
 *
 * @return  @c true if the player is thought to be in the void.
 */
boolean P_IsInVoid(player_t *p);

short P_LookDirToShort(float lookDir);
float P_ShortToLookDir(short s);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DENG_WORLD_P_PLAYERS_H
