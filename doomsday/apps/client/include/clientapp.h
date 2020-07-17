/** @file clientapp.h  The client application.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENTAPP_H
#define CLIENTAPP_H

#include <doomsday/doomsdayapp.h>
#include <doomsday/world/world.h>
#include <de/baseguiapp.h>

class AudioSystem;
class ClientPlayer;
class ClientResources;
class ClientWorld;
class ClientWindow;
class ConfigProfiles;
class InFineSystem;
class InputSystem;
class IWorldRenderer;
class RenderSystem;
class ServerLink;
class Updater;

#if !defined (DE_MOBILE)
#  define DE_HAVE_BUSYRUNNER 1
class BusyRunner;
#endif

/**
 * The client application.
 */
class ClientApp : public de::BaseGuiApp, public DoomsdayApp
{
public:
    ClientApp(const de::StringList &args);

    /**
     * Sets up all the subsystems of the application. Must be called before the
     * event loop is started. At the end of this method, the bootstrap script is
     * executed.
     */
    void initialize();

    void preFrame();
    void postFrame();

    void checkPackageCompatibility(const de::StringList &       packageIds,
                                   const de::String &           userMessageIfIncompatible,
                                   const std::function<void()> &finalizeFunc) override;

    void openInBrowser(const de::String &url);
    void openHomepageInBrowser();
    void showLocalFile(const de::NativePath &path);

    IWorldRenderer *makeWorldRenderer() const;

public:
    /**
     * Reports a new alert to the user.
     *
     * @param msg    Message to show. May contain style escapes.
     * @param level  Importance of the message.
     */
    static void alert(const de::String &msg, de::LogEntry::Level level = de::LogEntry::Message);

    static ClientPlayer &player(int console);
    static de::LoopResult forLocalPlayers(const std::function<de::LoopResult (ClientPlayer &)> &func);

    static ClientApp &      app();
    static ConfigProfiles & logSettings();
    static ConfigProfiles & networkSettings();
    static ConfigProfiles & audioSettings(); ///< @todo Belongs in AudioSystem.
    static ConfigProfiles & windowSettings();
    static ConfigProfiles & uiSettings();
    static ServerLink &     serverLink();
    static InFineSystem &   infine();
    static InputSystem &    input();
    static AudioSystem &    audio();
    static RenderSystem &   render();
    static ClientResources &resources();
    static world::World &   world();
    static ClientWorld &    classicWorld();

#if defined (DE_HAVE_BUSYRUNNER)
    static BusyRunner &     busyRunner();
#endif

    static ClientWindow *mainWindow();

    static bool hasInput();
    static bool hasRender();
    static bool hasAudio();
    static bool hasClassicWorld();

#if defined (DE_HAVE_UPDATER)
    static Updater &updater();
#endif

protected:
    void reset() override;
    void makeGameCurrent(const GameProfile &newGame) override;
    void unloadGame(const GameProfile &upcomingGame) override;
    void gameSessionWasSaved(const AbstractSession &session, GameStateFolder &toFolder) override;
    void gameSessionWasLoaded(const AbstractSession &session, const GameStateFolder &fromFolder) override;
    void sendDataToPlayer(int player, const de::IByteArray &data) override;

private:
    DE_PRIVATE(d)
};

#endif  // CLIENTAPP_H
