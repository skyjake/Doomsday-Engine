/** @file p_players.h  World player entities.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include <de/String>

/**
 * Describes a player interaction impulse.
 *
 * @ingroup playsim
 */
struct PlayerImpulse
{
    int id = 0;
    impulsetype_t type = IT_ANALOG;
    de::String name;                ///< Symbolic. Used when resolving or generating textual binding descriptors.
    de::String bindContextName;     ///< Symbolic name of the associated binding context.
};

struct player_t
{
    byte extraLightCounter; // Num tics to go till extraLight is disabled.
    int extraLight;
    int targetExtraLight;
    ddplayer_t shared; // The public player data.
};

extern player_t *viewPlayer;
extern player_t ddPlayers[DDMAXPLAYERS];
extern int consolePlayer;
extern int displayPlayer;

/**
 * Determine which console is used by the given local player. Local players
 * are numbered starting from zero.
 */
int P_LocalToConsole(int localPlayer);

/**
 * Determine the local player number used by a particular console. Local
 * players are numbered starting from zero.
 */
int P_ConsoleToLocal(int playerNum);

/**
 * Given a ptr to ddplayer_t, return it's logical index.
 */
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
bool P_IsInVoid(player_t *player);

short P_LookDirToShort(float lookDir);
float P_ShortToLookDir(short s);

/**
 * Remove all the player impulse definitions and destroy the associated accumulators
 * of all players.
 */
void P_ClearPlayerImpulses();

/**
 * Lookup a player impulse defintion by it's unique @a id.
 *
 * @param id  Unique identifier of the player impulse definition to lookup.
 *
 * @return  The associated PlayerImpulse if found; otherwise @c nullptr.
 */
PlayerImpulse *P_PlayerImpulsePtr(int id);

/**
 * Lookup a player impulse defintion by it's symbolic @a name.
 *
 * @param name  Symbolic name of the player impulse definition to lookup.
 *
 * @return  The associated PlayerImpulse if found; otherwise @c nullptr.
 */
PlayerImpulse *P_PlayerImpulseByName(de::String const &name);

/**
 * Register the console commands and variables of this module.
 */
void P_ConsoleRegister();

#endif // DENG_WORLD_P_PLAYERS_H
