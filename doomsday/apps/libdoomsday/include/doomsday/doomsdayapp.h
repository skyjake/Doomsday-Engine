/** @file doomsdayapp.h  Common application-level state and components.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "gameprofiles.h"

#include <de/Binder>
#include <de/NativePath>
#include <de/Info>
#include <de/shell/PackageDownloader>

#include <QFlags>
#include <string>

namespace res { class Bundles; }

class Game;
class Games;
class SaveGames;
class GameStateFolder;
class AbstractSession;

/**
 * Common application-level state and components.
 *
 * Both the server and client applications have an instance of DoomsdayApp
 * to manage the common state and subsystems.
 */
class LIBDOOMSDAY_PUBLIC DoomsdayApp
{
public:
    /// Notified before the current game is unloaded.
    DENG2_DEFINE_AUDIENCE2(GameUnload, void aboutToUnloadGame(Game const &gameBeingUnloaded))

    DENG2_DEFINE_AUDIENCE2(GameLoad, void aboutToLoadGame(Game const &gameBeingLoaded))

    /// Notified after the current game has been changed.
    DENG2_DEFINE_AUDIENCE2(GameChange, void currentGameChanged(Game const &newGame))

    /// Notified when console variables and commands should be registered.
    DENG2_DEFINE_AUDIENCE2(ConsoleRegistration, void consoleRegistration())

    DENG2_DEFINE_AUDIENCE2(PeriodicAutosave, void periodicAutosave())

    struct GameChangeParameters
    {
        /// @c true iff caller (i.e., App_ChangeGame) initiated busy mode.
        bool initiatedBusyMode;
    };

public:
    DoomsdayApp(Players::Constructor playerConstructor);

    virtual ~DoomsdayApp();

    void determineGlobalPaths();

    /**
     * Initialize application state.
     */
    void initialize();

    /**
     * Initializes the /local/wads folder that contains all the WAD files that
     * Doomsday will access.
     */
    void initWadFolders();

    /**
     * Initializes the /local/packs folder.
     */
    void initPackageFolders();

    /**
     * Lists all the files found on the command line "-file" option (and its aliases).
     * @return List of files.
     */
    QList<de::File *> filesFromCommandLine() const;

    /**
     * Release all cached uncompressed entries. If the contents of the compressed
     * files are needed, they will be decompressed and cached again.
     */
    void uncacheFilesFromMemory();

    /**
     * Deletes the contents of the /home/cache folder.
     */
    void clearCache();

    enum Behavior
    {
        AllowReload = 0x1,
        DefaultBehavior = 0,
    };
    Q_DECLARE_FLAGS(Behaviors, Behavior)

    /**
     * Switch to/activate the specified game.
     *
     * @param profile    Game to change to.
     * @param gameActivationFunc  Callback to call after the new game has
     *                   been made current.
     * @param behaviors  Change behavior flags.
     */
    bool changeGame(GameProfile const &profile,
                    std::function<int (void *)> gameActivationFunc,
                    Behaviors behaviors = DefaultBehavior);

    static bool isGameBeingChanged();

    bool isShuttingDown() const;
    void setShuttingDown(bool shuttingDown);

#ifdef WIN32
    void *moduleHandle() const;
#endif

    void setDoomsdayBasePath(de::NativePath const &path);
    std::string const &doomsdayBasePath() const;

    GameProfile &adhocProfile();

    /**
     * Checks if the currently loaded packages are compatible with the provided list.
     * If the user does not cancel, and the correct packages can be loaded, calls the
     * provided callback.
     *
     * The method may return immediately, if the user is shown an interactive message
     * explaning the situation and/or needed tasks.
     *
     * @param packageIds    List of package identifiers.
     * @param userMessageIfIncompatible  Message to show to the user explaining what is
     *                      using the incompatible packages.
     * @param finalizeFunc  Callback after the loaded packages have been checked to be
     *                      compatible. Will not be called if the operation is cancelled.
     */
    virtual void checkPackageCompatibility(
            de::StringList const &packageIds,
            de::String const &userMessageIfIncompatible,
            std::function<void ()> finalizeFunc) = 0;

    /**
     * Saves application state to a save folder.
     *
     * When saving a game session to disk, this method should be called so the
     * application gets a chance to include its state in the save as well.
     *
     * @param session   Game session that is being saved.
     * @param toFolder  Folder where the game state is being written.
     */
    virtual void gameSessionWasSaved(AbstractSession const &session,
                                     GameStateFolder &toFolder);

    /**
     * Loads application state from a save folder.
     *
     * When loading a game session from disk, this method should be called so that
     * the application can restore its state from the save.
     *
     * @param session     Game session that is being loaded.
     * @param fromFolder  Folder where the game state is being read.
     */
    virtual void gameSessionWasLoaded(AbstractSession const &session,
                                      GameStateFolder const &fromFolder);

public:
    static DoomsdayApp &    app();
    static de::shell::PackageDownloader &
                            packageDownloader();
    static res::Bundles &   bundles();
    static Plugins &        plugins();
    static Games &          games();
    static GameProfiles &   gameProfiles();
    static Players &        players();
    static BusyMode &       busyMode();
    static SaveGames &      saveGames();

    static de::NativePath   steamBasePath();
    static QList<de::NativePath> gogComPaths();

    /**
     * Sets the currently active game. DoomsdayApp does not take ownership of
     * the provided Game instance.
     *
     * @param game  Game instance. Must not be deleted until another Game is
     *              used as the current one.
     */
    static void setGame(Game const &game);

    /**
     * Returns the currently active game.
     */
    static Game const &game();

    static GameProfile const *currentGameProfile();

    static bool isGameLoaded();

    /**
     * Composes a list of all the packages that should be identified in savegame
     * metadata.
     *
     * @return List of package identifiers, in load order.
     */
    static de::StringList loadedPackagesAffectingGameplay();

protected:
    static void initBindings(de::Binder &binder);

    /**
     * Called just before a game change is about to begin. The GameUnload
     * audience has already been notified.
     *
     * @param upcomingGame  Upcoming game that we will be changing to.
     */
    virtual void unloadGame(GameProfile const &upcomingGame);

    virtual void makeGameCurrent(GameProfile const &game);

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

Q_DECLARE_OPERATORS_FOR_FLAGS(DoomsdayApp::Behaviors)

#endif // LIBDOOMSDAY_DOOMSDAYAPP_H
