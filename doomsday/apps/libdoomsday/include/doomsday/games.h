/** @file games.h  Specialized collection for a set of logical Games.
 *
 * @authors Copyright © 2012-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_GAMES_H
#define LIBDOOMSDAY_GAMES_H

#include "game.h"

#include <de/types.h>
#include <de/str.h>
#include <de/Observers>
#include <QList>

/**
 * Encapsulates a collection of Game instances and the logical operations
 * which are performed upon it (such as searches and various index printing
 * algorithms).
 *
 * @ingroup core
 */
class LIBDOOMSDAY_PUBLIC Games
{
public:
    /// The requested game does not exist in the collection. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// Used for returning the result of game searches. @see findAll()
    struct GameListItem
    {
        Game *game;

        GameListItem(Game *game = 0) : game(game)
        {}

        /// @return  @c true= this game's title is lexically less than that of @a other.
        bool operator < (GameListItem const &other) const {
            return game->title().compareWithoutCase(other.game->title()) < 0;
        }
    };

    typedef QList<GameListItem> GameList;
    typedef QList<Game *> All;

    /// Notified when a new game is added.
    DENG2_DEFINE_AUDIENCE2(Addition, void gameAdded(Game &game))

    /// Notified after game resources have been located.
    DENG2_DEFINE_AUDIENCE2(Readiness, void gameReadinessUpdated())

    /// Notified when a worker task is progressing.
    DENG2_DEFINE_AUDIENCE2(Progress, void gameWorkerProgress(int progress))

    static Games &get();

public:
    Games();

    /// @return  The special "null" Game instance.
    static Game &nullGame();

    /// @return  Total number of registered games.
    inline int count() const { return all().count(); }

    /// @return  Number of games marked as currently playable.
    int numPlayable() const;

    /**
     * @return  Game associated with @a identityKey.
     *
     * @throws NotFoundError if no game is associated with @a identityKey.
     */
    Game &operator [] (de::String const &id) const;

    bool contains(de::String const &id) const;

    /**
     * @return  Game associated with unique index @a idx.
     *
     * @deprecated Iterate games() instead.
     */
    Game &byIndex(int idx) const;

    void clear();

    /**
     * Register a new game.
     *
     * @note Game registration order defines the order of the automatic game
     * identification/selection logic.
     *
     * @param id          Game identifiet.
     * @param parameters  Game parameters.
     *
     * @return The created Game instance.
     */
    Game &defineGame(de::String const &id, de::Record const &parameters);

    /**
     * Returns a list of all the Game instances in the collection.
     */
    All const &all() const;

    de::LoopResult forAll(std::function<de::LoopResult (Game &)> callback) const;

    /**
     * Notify observers to update the readiness of games.
     */
    void checkReadiness();

    /**
     * Forgets the previously located resources of all registered games.
     */
    //void forgetAllResources();

    /**
     * Collects all games.
     *
     * @param collected  List to be populated with the collected games.
     * @return  Number of collected games.
     */
    int collectAll(GameList &collected);

    /**
     * Find the first playable game in this collection (in registration order).
     * @return  The found game else @c NULL.
     */
    GameProfile const *firstPlayable() const;

    /**
     * Try to locate all startup resources for @a game.
     */
    //void locateStartupResources(Game &game);

public:
    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_GAMES_H
