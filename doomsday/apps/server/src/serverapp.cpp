/** @file serverapp.cpp  The server application.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"

#include <QNetworkProxyFactory>
#include <QHostInfo>
#include <QDebug>
#include <stdlib.h>

#include <de/CommandLine>
#include <de/Log>
#include <de/LogBuffer>
#include <de/Error>
#include <de/c_wrapper.h>
#include <de/Garbage>

#include "serverapp.h"
#include "serverplayer.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "def_main.h"
#include "con_config.h"
#include "network/net_main.h"
#include "world/map.h"
#include "world/p_players.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

using namespace de;

static ServerApp *serverAppSingleton = 0;

static void handleAppTerminate(char const *msg)
{
    LogBuffer::get().flush();
    qFatal("Application terminated due to exception:\n%s\n", msg);
}

DENG2_PIMPL(ServerApp)
, DENG2_OBSERVES(Plugins, PublishAPI)
, DENG2_OBSERVES(DoomsdayApp, GameUnload)
, DENG2_OBSERVES(DoomsdayApp, ConsoleRegistration)
, DENG2_OBSERVES(DoomsdayApp, PeriodicAutosave)
{
    QScopedPointer<ServerSystem> serverSystem;
    QScopedPointer<Resources> resources;
    QScopedPointer<AudioSystem> audioSys;
    ClientServerWorld world;
    InFineSystem infineSys;

    Impl(Public *i)
        : Base(i)
    {
        serverAppSingleton = thisPublic;

        DoomsdayApp::plugins().audienceForPublishAPI() += this;
        self().audienceForGameUnload() += this;
        self().audienceForConsoleRegistration() += this;
        self().audienceForPeriodicAutosave() += this;
    }

    ~Impl()
    {
        Sys_Shutdown();
        DD_Shutdown();
    }

    void publishAPIToPlugin(::Library *plugin)
    {
        DD_PublishAPIs(plugin);
    }

    void consoleRegistration()
    {
        DD_ConsoleRegister();
    }

    void aboutToUnloadGame(Game const &/*gameBeingUnloaded*/)
    {
        if (netGame && isServer)
        {
            N_ServerClose();
        }

        infineSystem().reset();

        if (App_GameLoaded())
        {
            Con_SaveDefaults();
        }
    }

    void periodicAutosave()
    {
        Con_SaveDefaultsIfChanged();
    }

#ifdef UNIX
    void printVersionToStdOut()
    {
        printf("%s\n", String("%1 %2")
               .arg(DOOMSDAY_NICENAME)
               .arg(DOOMSDAY_VERSION_FULLTEXT)
               .toLatin1().constData());
    }

    void printHelpToStdOut()
    {
        printVersionToStdOut();
        printf("Usage: %s [options]\n", self().commandLine().at(0).toLatin1().constData());
        printf(" -iwad (dir)  Set directory containing IWAD files.\n");
        printf(" -file (f)    Load one or more PWAD files at startup.\n");
        printf(" -game (id)   Set game to load at startup.\n");
        printf(" --version    Print current version.\n");
        printf("For more options and information, see \"man doomsday-server\".\n");
    }
#endif
};

ServerApp::ServerApp(int &argc, char **argv)
    : TextApp(argc, argv)
    , DoomsdayApp([] () -> Player * { return new ServerPlayer; })
    , d(new Impl(this))
{
    novideo = true;

    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    setMetadata("Deng Team", "dengine.net", "Doomsday Server", DOOMSDAY_VERSION_BASE);
    setUnixHomeFolderName(".doomsday");

    setTerminateFunc(handleAppTerminate);

    d->serverSystem.reset(new ServerSystem);
    addSystem(*d->serverSystem);

    d->resources.reset(new Resources);
    addSystem(*d->resources);

    d->audioSys.reset(new AudioSystem);
    addSystem(*d->audioSys);

    addSystem(d->world);
    //addSystem(d->infineSys);

    // We must presently set the current game manually (the collection is global).
    setGame(games().nullGame());
}

ServerApp::~ServerApp()
{
    d.reset();

    // Now that everything is shut down we can forget about the singleton instance.
    serverAppSingleton = 0;
}

void ServerApp::initialize()
{
    Libdeng_Init();

#ifdef UNIX
    // Some common Unix command line options.
    if (commandLine().has("--version") || commandLine().has("-version"))
    {
        d->printVersionToStdOut();
        ::exit(0);
    }
    if (commandLine().has("--help") || commandLine().has("-h") || commandLine().has("-?"))
    {
        d->printHelpToStdOut();
        ::exit(0);
    }
#endif

    if (!CommandLine_Exists("-stdout"))
    {
        // In server mode, stay quiet on the standard outputs.
        LogBuffer::get().enableStandardOutput(false);
    }

    Def_Init();

    // Load the server's packages.
    initSubsystems();
    DoomsdayApp::initialize();

    // Initialize.
#if WIN32
    if (!DD_Win32_Init())
    {
        throw Error("ServerApp::initialize", "DD_Win32_Init failed");
    }
#elif UNIX
    if (!DD_Unix_Init())
    {
        throw Error("ServerApp::initialize", "DD_Unix_Init failed");
    }
#endif

    plugins().loadAll();

    DD_FinishInitializationAfterWindowReady();
}

void ServerApp::checkPackageCompatibility(StringList const &packageIds,
                                          String const &userMessageIfIncompatible,
                                          std::function<void ()> finalizeFunc)
{
    if (GameProfiles::arePackageListsCompatible(packageIds, loadedPackagesAffectingGameplay()))
    {
        finalizeFunc();
    }
    else
    {
        LOG_RES_ERROR("%s") << userMessageIfIncompatible;
    }
}

shell::ServerInfo ServerApp::currentServerInfo()
{
    shell::ServerInfo info;

    // Let's figure out what we want to tell about ourselves.
    info.setCompatibilityVersion(DOOMSDAY_VERSION);
    info.setPluginDescription(String::format("%s %s",
                                             reinterpret_cast<char const *>(gx.GetVariable(DD_PLUGIN_NAME)),
                                             reinterpret_cast<char const *>(gx.GetVariable(DD_PLUGIN_VERSION_SHORT))));

    info.setGameId(game().id());
    info.setGameConfig(reinterpret_cast<char const *>(gx.GetVariable(DD_GAME_CONFIG)));
    info.setName(serverName);
    info.setDescription(serverInfo);

    // The server player is there, it's just hidden.
    info.setMaxPlayers(de::min(svMaxPlayers, DDMAXPLAYERS - (isDedicated ? 1 : 0)));

    //info->canJoin = ;
    shell::ServerInfo::Flags flags(0);
    if (isServer != 0 && Sv_GetNumPlayers() < svMaxPlayers)
    {
        flags |= shell::ServerInfo::AllowJoin;
    }
    info.setFlags(flags);

    // Identifier of the current map.
    if (world().hasMap())
    {
        auto &map = world().map();
        String const mapPath = (map.hasManifest() ? map.manifest().composeUri().path() : "(unknown map)");
        info.setMap(mapPath);
    }

    QHostInfo const host = QHostInfo::fromName(QHostInfo::localHostName());
    if (!host.addresses().isEmpty())
    {
        /// @todo Maybe check that it's not a loopback address?
        info.setAddress(Address(host.addresses().at(0), duint16(nptIPPort)));
    }

    // Let's compile a list of client names.
    for (dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (DD_Player(i)->isConnected())
        {
            info.addPlayer(DD_Player(i)->name);
        }
    }

    info.setPackages(loadedPackagesAffectingGameplay());

    // Some WAD names.
    //composePWADFileList(info->pwads, sizeof(info->pwads), ";");

    // This should be a CRC number that describes all the loaded data.
    //info->loadedFilesCRC = App_FileSystem().loadedFilesCRC();;

    return info;
}

void ServerApp::unloadGame(GameProfile const &upcomingGame)
{
    DoomsdayApp::unloadGame(upcomingGame);

    world::Map::initDummies();
}

ServerApp &ServerApp::app()
{
    DENG2_ASSERT(serverAppSingleton != 0);
    return *serverAppSingleton;
}

ServerSystem &ServerApp::serverSystem()
{
    return *app().d->serverSystem;
}

InFineSystem &ServerApp::infineSystem()
{
    return app().d->infineSys;
}

AudioSystem &ServerApp::audioSystem()
{
    return *app().d->audioSys;
}

Resources &ServerApp::resources()
{
    return *app().d->resources;
}

ClientServerWorld &ServerApp::world()
{
    return app().d->world;
}

void ServerApp::reset()
{
    DoomsdayApp::reset();
}
