/** @file gamesession.h  Logical game session and saved session marshalling.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAMESESSION_H
#define LIBCOMMON_GAMESESSION_H

#include <de/game/Session>
#include <de/String>
#include <doomsday/uri.h>
#include "doomsday.h"
#include "gamerules.h"

namespace common {

/**
 * Implements high level logic for the manipulation and configuration of the logical game session.
 *
 * An internal backing store is used to record player progress automatically, whenever the current
 * map changes while the session is in progress. This occurs irrespective of the user's savegame
 * preferences. Additionally, the user may configure the game so that the internal backing store
 * is periodically (e.g., when the map changes) copied to a new "autosave" automatically.
 *
 * The "scope" of a continous game session progression depends on the configuration of the World
 * and the Maps within it. Upon leaving one map and entering another, if both are attributed to
 * the same logical "hub" then the current state of the map is written to the backing store so
 * that it may be reloaded later if the player(s) decide to revisit. However, if the new map is
 * in another hub, or no hub is defined, then all saved map progress for current hub is discarded.
 *
 * Note that the use of hubs is not required and some games may not use them at all (e.g., DOOM).
 *
 * @ingroup libcommon
 */
class GameSession : public de::game::Session
{
public:
    GameSession();
    virtual ~GameSession();

    /// Returns the singleton instance.
    static GameSession &gameSession();

    /// Register the commands and variables of this module.
    static void consoleRegister();

    bool hasBegun();
    bool savingPossible();
    bool loadingPossible();

    /**
     * Returns the (current) Episode definition for the game session in progress. If the session
     * has not yet begun then @c NULL is returned.
     */
    de::Record *episodeDef();

    /**
     * Returns the (current) MapGraphNode definition for the game session in progress. If the
     * session has not yet begun then @c NULL is returned.
     */
    de::Record *mapGraphNodeDef();

    /**
     * Returns the (current) MapInfo definition for the game session in progress. If the session
     * has not yet begun, or no definition exists for the current map then @c NULL is returned.
     */
    de::Record *mapInfo();

    /**
     * Resolves a named exit according to the map progression.
     */
    de::Uri mapUriForNamedExit(de::String const &name);

    /**
     * Returns the current ruleset for the game session.
     */
    GameRuleset const &rules() const;

    /**
     * To be called when a new game begins to effect the game rules. Note that some of the rules
     * may be overridden here (e.g., in a networked game).
     *
     * @todo Prevent this outright if the game session is already in progress!
     */
    void applyNewRules(GameRuleset const &rules);

    /**
     * Determines whether saved game progress will be restored when the current map is reloaded,
     * according to the current game state and user configuration.
     */
    bool progressRestoredOnReload() const;

    /**
     * End the game session (if in progress).
     * @see hasBegun()
     */
    void end();

    /**
     * End the game session (if in progress) and begin the title sequence.
     * @see end(), hasBegun()
     */
    void endAndBeginTitle();

    /**
     * Configure and begin a new game session. Note that a @em new session cannot @em begin if
     * one already @ref hasBegun() (if so, the session must be ended first).
     *
     * @param mapUri       Map identifier.
     * @param mapEntrance  Logical map entry point number.
     * @param rules        Game rules to apply.
     *
     * @throws InProgressError if the session has already begun.
     */
    void begin(de::Uri const &mapUri, uint mapEntrance, GameRuleset const &rules);

    /**
     * Reload the @em current map, automatically loading any saved progress from the backing
     * store if @ref progressRestoredOnReload(). If no saved progress exists then the map will
     * be in the default state.
     */
    void reloadMap();

    /**
     * Leave the @em current map (automatically saving progress to the backing store) and then
     * load up the next map according to the defined game progression.
     */
    void leaveMap();

    /**
     * Convenient method of looking up the user description of the game session in progress.
     *
     * @return  User description of the session if it @ref hasBegun() or a zero-length string.
     */
    de::String userDescription();

public: // Saved session management ----------------------------------------------------------

    /**
     * Save the current game state to a new @em user saved session.
     *
     * @param saveName         Name of the new saved session.
     * @param userDescription  Textual description of the current game state provided either
     *                         by the user or possibly generated automatically.
     */
    void save(de::String const &saveName, de::String const &userDescription);

    /**
     * Load the game state from the @em user saved session specified.
     *
     * @param saveName  Name of the saved session to be loaded.
     */
    void load(de::String const &saveName);

    /**
     * Makes a copy of the @em user saved session specified in /home/savegames/<gameId>
     *
     * @param destName    Name of the new/replaced saved session.
     * @param sourceName  Name of the saved session to be copied.
     */
    void copySaved(de::String const &destName, de::String const &sourceName);

    /**
     * Removes the @em user saved session /home/savegames/<gameId>/<@a saveName>.save
     */
    void removeSaved(de::String const &saveName);

    /**
     * Convenient method of looking up the @em user description of an existing saved session.
     *
     * @param saveName  Name of the saved session to lookup.
     *
     * @return  User description of the named session or a zero-length string if not found.
     */
    de::String savedUserDescription(de::String const &saveName);

private:
    DENG2_PRIVATE(d)
};

} // namespace common

/**
 * Macro for conveniently accessing the common::GameSession singleton instance.
 */
#define COMMON_GAMESESSION  (&common::GameSession::gameSession())

#endif // LIBCOMMON_GAMESESSION_H
