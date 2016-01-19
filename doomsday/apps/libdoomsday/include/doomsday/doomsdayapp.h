/** @file doomsdayapp.h  Common application-level state and components.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DOOMSDAYAPP_H
#define LIBDOOMSDAY_DOOMSDAYAPP_H

#include "plugins.h"
#include "busymode.h"
#include "gameapi.h"
#include "players.h"

#include <de/NativePath>
#include <de/Info>
#include <string>

namespace res { class Bundles; }

class Games;
class Game;

/**
 * Common application-level state and components.
 *
 * Both the server and client applications have an instance of DoomsdayApp
 * to manage the common state and subsystems.
 */
class LIBDOOMSDAY_PUBLIC DoomsdayApp
{
public:
    /**
     * Notified before the current game is unloaded.
     */
    DENG2_DEFINE_AUDIENCE2(GameUnload, void aboutToUnloadGame(Game const &gameBeingUnloaded))

    /**
     * Notified after the current game has been changed.
     */
    DENG2_DEFINE_AUDIENCE2(GameChange, void currentGameChanged(Game const &newGame))

public:
    DoomsdayApp(Players::Constructor playerConstructor);

    /**
     * Initialize application state.
     */
    void initialize();

    void identifyDataBundles();

    void determineGlobalPaths();

    bool isUsingUserDir() const;

#ifdef WIN32
    void *moduleHandle() const;
#endif

    void setDoomsdayBasePath(de::NativePath const &path);
    void setDoomsdayRuntimePath(de::NativePath const &path);
    std::string const &doomsdayBasePath() const;
    std::string const &doomsdayRuntimePath() const;

public:
    static DoomsdayApp &app();
    static res::Bundles &bundles();
    static Plugins &plugins();
    static Games &games();
    static Players &players();
    static Game &currentGame();
    static BusyMode &busyMode();
    static de::NativePath steamBasePath();

    /**
     * Sets the currently active game. DoomsdayApp does not take ownership of
     * the provided Game instance.
     *
     * @param game  Game instance. Must not be deleted until another Game is
     *              used as the current one.
     */
    static void setGame(Game &game);

    /**
     * Returns the currently active game.
     */
    static Game &game();

//protected:
    /**
     * Called just before a game change is about to begin. The GameUnload
     * audience has already been notified.
     *
     * @param upcomingGame  Upcoming game that we will be changing to.
     */
    virtual void aboutToChangeGame(Game const &upcomingGame);

    /**
     * Clears all allocated resources and subsystems. This is called when
     * a game is being unloaded.
     */
    virtual void reset();

private:
    DENG2_PRIVATE(d)
};

/**
 * Returns @c true if a game module is presently loaded.
 */
LIBDOOMSDAY_PUBLIC bool App_GameLoaded();

#endif // LIBDOOMSDAY_DOOMSDAYAPP_H
