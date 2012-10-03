/**
 * @file dd_games.h
 *
 * The Game collection.
 *
 * @ingroup core
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_GAMES_H
#define LIBDENG_GAMES_H

#include <de/types.h>
#include "game.h"

struct gameinfo_s;
struct gamedef_s;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

/// Currently active game.
extern Game* theGame;

/// The special "null-game" object instance.
extern Game* nullGame;

void Games_Shutdown(void);

/// @return  Total number of registered games.
int Games_Count(void);

/// @return  Number of games marked as currently playable.
int Games_NumPlayable(void);

/**
 * @param game  Game instance.
 * @return Unique identifier associated with @a game.
 */
gameid_t Games_Id(Game* game);

/**
 * @return  Game associated with unique index @a idx else @c NULL.
 */
Game* Games_ByIndex(int idx);

/**
 * @return  Game associated with @a identityKey else @c NULL.
 */
Game* Games_ByIdentityKey(char const* identityKey);

/**
 * Is this the special "null-game" object (not a real playable game).
 * @todo Implement a proper null-game object for this.
 */
boolean Games_IsNullObject(Game const* game);

/// @return The first playable game in the collection according to registration order.
Game* Games_FirstPlayable(void);

/**
 * Print a game mode banner with rulers.
 */
void Games_PrintBanner(Game* game);

/**
 * Print the list of resources for @a Game.
 *
 * @param game          Game to list resources of.
 * @param printStatus   @c true= Include the current availability/load status
 *                      of each resource.
 * @param rflags        Only consider resources whose @ref resourceFlags match
 *                      this value. If @c <0 the flags are ignored.
 */
void Games_PrintResources(Game* game, boolean printStatus, int rflags);

/**
 * @defgroup printGameFlags  Print Game Flags.
 */
///@{
#define PGF_BANNER                 0x1
#define PGF_STATUS                 0x2
#define PGF_LIST_STARTUP_RESOURCES 0x4
#define PGF_LIST_OTHER_RESOURCES   0x8

#define PGF_EVERYTHING             (PGF_BANNER|PGF_STATUS|PGF_LIST_STARTUP_RESOURCES|PGF_LIST_OTHER_RESOURCES)
///@}

/**
 * Print extended information about game @a info.
 * @param info  Game record to be printed.
 * @param flags  &see printGameFlags
 */
void Games_Print(Game* game, int flags);

boolean DD_GameInfo(struct gameinfo_s* info);

void DD_AddGameResource(gameid_t gameId, resourceclass_t rclass, int rflags, char const* _names, void* params);

gameid_t DD_DefineGame(struct gamedef_s const* def);

gameid_t DD_GameIdForKey(char const* identityKey);

D_CMD(ListGames);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GAMES_H */
