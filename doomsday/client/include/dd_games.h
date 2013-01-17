/**
 * @file dd_games.h
 *
 * The Game collection.
 *
 * @ingroup core
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

namespace de
{
    /**
     * Encapsulates a collection of de::Game instances and the logical operations
     * which are performed upon it (such as searches and various index printing
     * algorithms).
     */
    class GameCollection
    {
    public:
        /// The requested game does not exist in the collection. @ingroup errors
        DENG2_ERROR(NotFoundError);

        /// Used for returning the result of game searches. @see findAll()
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

        typedef QList<Game*> Games;

    public:
        GameCollection();
        ~GameCollection();

        /// Register the console commands, variables, etc..., of this module.
        static void consoleRegister();

        /// @return  The currently active Game instance.
        Game& currentGame() const;

        /// @return  The special "null" Game instance.
        Game& nullGame() const;

        /// Change the currently active game.
        GameCollection& setCurrentGame(Game& game);

        /// @return  @c true= @a game is the currently active game.
        inline bool isCurrentGame(Game const& game) const {
            return &game == &currentGame();
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

        /**
         * @return  Game associated with @a identityKey.
         *
         * @throws NotFoundError if no game is associated with @a identityKey.
         */
        Game& byIdentityKey(char const* identityKey) const;

        /**
         * @return  Game associated with @a gameId.
         *
         * @throws NotFoundError if no game is associated with @a gameId.
         */
        Game& byId(gameid_t gameId) const;

        /**
         * Provides access to the games for efficient traversals.
         */
        Games const& games() const;

        /**
         * Finds all games.
         *
         * @param found  List of found games.
         * @return  Number of games found.
         */
        int findAll(GameList& found);

        /**
         * Find the first playable game in this collection (in registration order).
         * @return  The found game else @c NULL.
         */
        Game* firstPlayable() const;

        /**
         * Add a new Game to this collection. If @a game is already present in the
         * collection this is no-op.
         *
         * @param game  Game to be added.
         * @return  This collection.
         */
        GameCollection& add(Game& game);

        /**
         * Try to locate all startup resources for @a game.
         * @return  This collection.
         */
        GameCollection& locateStartupResources(Game& game);

        /**
         * Try to locate all startup resources for all registered games.
         * @return  This collection.
         */
        GameCollection& locateAllResources();

        /**
         * @return  Game associated with unique index @a idx.
         *
         * @deprecated Iterate games() instead.
         *
         * @throws NotFoundError if no game is associated with index @a idx.
         */
        Game& byIndex(int idx) const;

    private:
        struct Instance;
        Instance* d;
    };

} // namespace de

extern "C" {
#endif

/*
 * C wrapper API:
 */

struct gamecollection_s;
typedef struct gamecollection_s GameCollection;

/// @return  The currently active Game instance.
Game* GameCollection_CurrentGame(GameCollection* games);

/// @return  Total number of registered games.
int GameCollection_Count(GameCollection* games);

/// @return  Number of games marked as currently playable.
int GameCollection_NumPlayable(GameCollection* games);

/**
 * Returns the unique identifier associated with @a game.
 *
 * @param games  Game collection.
 * @param game   Game instance.
 *
 * @return Game identifier.
 */
gameid_t GameCollection_Id(GameCollection* games, Game* game);

/**
 * Finds a game with a particular identifier in the game collection.
 *
 * @param games   Game collection.
 * @param gameId  Game identifier (see GameCollection_Id()).
 *
 * @return  Game associated with @a gameId else @c NULL.
 */
Game* GameCollection_ById(GameCollection* games, gameid_t gameId);

/**
 * @return  Game associated with @a identityKey else @c NULL.
 */
Game* GameCollection_ByIdentityKey(GameCollection* games, char const* identityKey);

/**
 * Locates a game in the collection.
 *
 * @param games  Game collection.
 * @param idx    Index of the game in the collection. Valid indices
 *               are in the range 0 ... GameCollection_Count() - 1.
 *
 * @return  Game associated with unique index @a idx else @c NULL.
 */
Game* GameCollection_ByIndex(GameCollection* games, int idx);

/**
 * Finds the first playable game in the collection according to registration
 * order.
 *
 * @param games  Game collection.
 *
 * @return Game instance.
 */
Game* GameCollection_FirstPlayable(GameCollection* games);

/**
 * Try to locate all startup resources for all registered games.
 *
 * @param games  Game collection.
 */
void GameCollection_LocateAllResources(GameCollection* games);

D_CMD(ListGames);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GAMES_H */
