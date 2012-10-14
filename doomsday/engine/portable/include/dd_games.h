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

#include <QList>
#include <de/str.h>

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

namespace de
{
    /**
     * Games encapsulates a collection of de::Game instances and the logical
     * operations which are performed upon it (such as searches and various
     * index printing algorithms).
     */
    class Games
    {
    public:
        struct GameListItem
        {
            Game* game;

            GameListItem(Game* _game = 0) : game(_game)
            {}

            /// @return  @c true= this game's title is lexically less than that of @a other.
            bool operator < (GameListItem const& other) const
            {
                return Str_CompareIgnoreCase(&game->title(), Str_Text(&other.game->title())) < 0;
            }
        };
        typedef QList<GameListItem> GameList;

    public:
        Games();
        ~Games();

        static void consoleRegister();

        /// @return  The currently active Game instance.
        Game& currentGame() const;

        /// @return  The special "null" Game instance.
        Game& nullGame() const;

        /// Change the currently active game.
        Games& setCurrentGame(Game& game);

        /// @return  @c true= @a game is the currently active game.
        inline bool isCurrentGame(Game const& game) const {
            return &game == &currentGame();
        }

        /// @todo Implement a proper null-game object for this.
        /// @return  @c true= @a game is the special "null-game" object (not a real playable game).
        inline bool isNullGame(Game const& game) const {
            return &game == &nullGame();
        }

        /// @return  Total number of registered games.
        int count() const;

        /// @return  Number of games marked as currently playable.
        int numPlayable() const;

        /**
         * @param game  Game instance.
         * @return Unique identifier associated with @a game.
         */
        gameid_t id(Game& game) const;

        gameid_t gameIdForKey(char const* identityKey) const;

        /// @return  Game associated with unique index @a idx else @c NULL.
        Game* byIndex(int idx) const;

        /// @return  Game associated with @a identityKey else @c NULL.
        Game* byIdentityKey(char const* identityKey) const;

        /// @return  Game associated with @a gameId else @c NULL.
        Game* byId(gameid_t gameId) const;

        /**
         * Finds all games.
         *
         * @param found         Set of games that match the result.
         *
         * @return  Number of games found.
         */
        int findAll(GameList& found);

        /**
         * Add a new Game to this collection.
         *
         * @attention Assumes @a game is not already in the collection.
         */
        Games& add(Game& game);

        /// @return The first playable game in the collection according to registration order.
        Game* firstPlayable() const;

        /**
         * Try to locate all startup resources for @a game.
         */
        Games& locateStartupResources(Game& game);

        /**
         * Try to locate all startup resources for all registered games.
         */
        Games& locateAllResources();

        /**
         * Print extended information about game @a info.
         * @param info  Game record to be printed.
         * @param flags  &ref printGameFlags
         */
        void print(Game& game, int flags) const;

        /**
         * Print a game mode banner with rulers.
         */
        static void printBanner(Game& game);

        /**
         * Print the list of resources for @a Game.
         *
         * @param game          Game to list resources of.
         * @param printStatus   @c true= Include the current availability/load status
         *                      of each resource.
         * @param rflags        Only consider resources whose @ref resourceFlags match
         *                      this value. If @c <0 the flags are ignored.
         */
        static void printResources(Game& game, bool printStatus, int rflags);

    private:
        struct Instance;
        Instance* d;
    };

} // namespace de

extern "C" {
#endif

/**
 * C wrapper API:
 */

struct games_s;
typedef struct games_s Games;

/// @return  The currently active Game instance.
Game* Games_CurrentGame(Games* games);

/// @return  The special "null" Game instance.
Game* Games_NullGame(Games* games);

/// @return  Total number of registered games.
int Games_Count(Games* games);

/// @return  Number of games marked as currently playable.
int Games_NumPlayable(Games* games);

/**
 * @param game  Game instance.
 * @return Unique identifier associated with @a game.
 */
gameid_t Games_Id(Games* games, Game* game);

/**
 * @return  Game associated with unique index @a idx else @c NULL.
 */
Game* Games_ByIndex(Games* games, int idx);

/**
 * @return  Game associated with @a identityKey else @c NULL.
 */
Game* Games_ByIdentityKey(Games* games, char const* identityKey);

/**
 * @return  Game associated with @a gameId else @c NULL.
 */
Game* Games_ById(Games* games, gameid_t gameId);

/**
 * Is this the special "null-game" object (not a real playable game).
 * @todo Implement a proper null-game object for this.
 */
boolean Games_IsNullObject(Games* games, Game const* game);

/// @return The first playable game in the collection according to registration order.
Game* Games_FirstPlayable(Games* games);

/**
 * Try to locate all startup resources for all registered games.
 */
void Games_LocateAllResources(Games* games);

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
void Games_Print(Games* games, Game* game, int flags);

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

D_CMD(ListGames);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GAMES_H */
