/** @file serverapp.cpp  The server application.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "serverapp.h"
#include "serverplayer.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "def_main.h"
#include "con_config.h"
#include "network/net_main.h"
#include "world/p_players.h"

#include <doomsday/console/var.h>
#include <doomsday/net.h>
#include <doomsday/world/map.h>
#include <doomsday/world/thinkers.h>

#include <de/commandline.h>
#include <de/config.h>
#include <de/error.h>
#include <de/filesystem.h>
#include <de/garbage.h>
#include <de/log.h>
#include <de/logbuffer.h>
#include <de/packagefeed.h>
#include <de/packageloader.h>
#include <de/dscript.h>
#include <de/c_wrapper.h>

#include <stdlib.h>

using namespace de;

static ServerApp *serverAppSingleton = nullptr;

DE_STATIC_STRING(PATH_SERVER_FILES, "/sys/server/public");

static void handleAppTerminate(const char *msg)
{
    LogBuffer::get().flush();
    warning("Application terminated due to exception:\n%s\n", msg);
    exit(1);
}

DE_PIMPL(ServerApp)
, DE_OBSERVES(Plugins, PublishAPI)
, DE_OBSERVES(DoomsdayApp, GameUnload)
, DE_OBSERVES(DoomsdayApp, ConsoleRegistration)
, DE_OBSERVES(DoomsdayApp, PeriodicAutosave)
, DE_OBSERVES(PackageLoader, Activity)
{
    std::unique_ptr<ServerSystem> serverSystem;
    std::unique_ptr<Resources>    resources;
    std::unique_ptr<AudioSystem>  audioSys;
    ServerWorld                   world;
    InFineSystem                  infineSys;
    duint32                       serverId;

    Impl(Public *i)
        : Base(i)
    {
        serverAppSingleton = thisPublic;

        DoomsdayApp::plugins().audienceForPublishAPI() += this;
        self().audienceForGameUnload() += this;
        self().audienceForConsoleRegistration() += this;
        self().audienceForPeriodicAutosave() += this;
        PackageLoader::get().audienceForActivity() += this;

        // Each running server instance is given a random identifier.
        serverId = randui32() & 0x7fffffff;

        LOG_NET_NOTE("Server instance ID: %08x") << serverId;
    }

    ~Impl() override
    {
        Sys_Shutdown();
        DD_Shutdown();
    }

    void publishAPIToPlugin(const char *plugName) override
    {
        DD_PublishAPIs(plugName);
    }

    void consoleRegistration() override
    {
        DD_ConsoleRegister();
    }

    void aboutToUnloadGame(const Game &/*gameBeingUnloaded*/) override
    {
        if (netState.netGame && netState.isServer)
        {
            N_ServerClose();
        }
        infine().reset();
        if (App_GameLoaded())
        {
            Con_SaveDefaults();
        }
    }

    void periodicAutosave() override
    {
        if (Config::exists())
        {
            Config::get().writeIfModified();
        }
        Con_SaveDefaultsIfChanged();
    }

    void initServerFiles()
    {
        // Packages available to clients via RemoteFeed use versioned identifiers because
        // a client may already have a different version of the package.

        Folder &files = self().fileSystem().makeFolder(PATH_SERVER_FILES());
        auto *feed = new PackageFeed(PackageLoader::get(),
                                     PackageFeed::LinkVersionedIdentifier);
        feed->setFilter(
            [](const Package &pkg) { return !pkg.matchTags(pkg.file(), "\\b(vanilla|core)\\b"); });
        files.attach(feed);
    }

    void setOfLoadedPackagesChanged() override
    {
        if (auto *files = FS::tryLocate<Folder>(PATH_SERVER_FILES()))
        {
            files->populate();
        }
    }

#ifdef UNIX
    void printVersionToStdOut()
    {
        printf("%s %s\n", DOOMSDAY_NICENAME, DOOMSDAY_VERSION_FULLTEXT);
    }

    void printHelpToStdOut()
    {
        printVersionToStdOut();
        printf("Usage: %s [options]\n", self().commandLine().at(0).c_str());
        printf(" -iwad (dir)  Set directory containing IWAD files.\n");
        printf(" -file (f)    Load one or more PWAD files at startup.\n");
        printf(" -game (id)   Set game to load at startup.\n");
        printf(" --version    Print current version.\n");
        printf("For more options and information, see \"man doomsday-server\".\n");
    }
#endif
};

ServerApp::ServerApp(const StringList &args)
    : TextApp(args)
    , DoomsdayApp([] () -> Player * { return new ServerPlayer; })
    , d(new Impl(this))
{
    novideo = true;

    // Metadata.
    setMetadata("Deng Team", "dengine.net", "Doomsday Server", DOOMSDAY_VERSION_BASE);
    setUnixHomeFolderName(".doomsday-server");

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

    net().setTransmitterLookup([](int player) -> Transmitter * {
        auto &plr = players().at(player).as<ServerPlayer>();
        if (plr.isConnected())
        {
            return &serverSystem().user(plr.remoteUserId);
        }
        return nullptr;
    });
}

ServerApp::~ServerApp()
{
    d.reset();

    // Now that everything is shut down we can forget about the singleton instance.
    serverAppSingleton = nullptr;
}

duint32 ServerApp::instanceId() const
{
    return d->serverId;
}

void ServerApp::initialize()
{
    Libdeng_Init();
    DD_InitCommandLine();

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

    d->initServerFiles();

    // Initialize.
#if defined (WIN32)
    if (!DD_Win32_Init())
    {
        throw Error("ServerApp::initialize", "DD_Win32_Init failed");
    }
#elif defined (UNIX)
    if (!DD_Unix_Init())
    {
        throw Error("ServerApp::initialize", "DD_Unix_Init failed");
    }
#endif

    plugins().loadAll();

    scriptSystem().importModule("commonlib"); // from net.dengine.base

    DD_FinishInitializationAfterWindowReady();
}

void ServerApp::checkPackageCompatibility(const StringList &           packageIds,
                                          const String &               userMessageIfIncompatible,
                                          const std::function<void()> &finalizeFunc)
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

ServerInfo ServerApp::currentServerInfo() // static
{
    ServerInfo info;

    // Let's figure out what we want to tell about ourselves.
    info.setServerId(ServerApp::app().d->serverId);
    info.setCompatibilityVersion(DOOMSDAY_VERSION);
    info.setPluginDescription(
        Stringf("%s %s",
                reinterpret_cast<const char *>(gx.GetPointer(DD_PLUGIN_NAME)),
                reinterpret_cast<const char *>(gx.GetPointer(DD_PLUGIN_VERSION_SHORT))));

    info.setGameId(game().id());
    info.setGameConfig(reinterpret_cast<const char *>(gx.GetPointer(DD_GAME_CONFIG)));
    info.setName(serverName);
    info.setDescription(serverInfo);

    // The server player is there, it's just hidden.
    info.setMaxPlayers(de::min(svMaxPlayers, DDMAXPLAYERS - (isDedicated ? 1 : 0)));

    Flags flags = 0;
    if (CVar_Byte(Con_FindVariable("server-allowjoin"))
            && netState.isServer != 0
            && Sv_GetNumPlayers() < svMaxPlayers)
    {
        flags |= ServerInfo::AllowJoin;
    }
    info.setFlags(flags);

    // Identifier of the current map.
    if (world().hasMap())
    {
        auto &map = world().map();
        const String mapPath = (map.hasManifest() ? map.manifest().composeUri().path() : "(unknown map)");
        info.setMap(mapPath);
    }

    // Check the IP address of the server.
    info.setAddress(Address::localNetworkInterface(duint16(nptIPPort)));

    if (const String publicHostName = nptIPAddress)
    {
        info.setDomainName(Stringf(
            "%s:%i", publicHostName.c_str(), nptIPPort ? nptIPPort : DEFAULT_PORT));
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

void ServerApp::unloadGame(const GameProfile &upcomingGame)
{
    DoomsdayApp::unloadGame(upcomingGame);

    world::Map::initDummyElements();
}

ServerApp &ServerApp::app()
{
    DE_ASSERT(serverAppSingleton != nullptr);
    return *serverAppSingleton;
}

ServerSystem &ServerApp::serverSystem()
{
    return *app().d->serverSystem;
}

InFineSystem &ServerApp::infine()
{
    return app().d->infineSys;
}

AudioSystem &ServerApp::audio()
{
    return *app().d->audioSys;
}

Resources &ServerApp::resources()
{
    return *app().d->resources;
}

ServerWorld &ServerApp::world()
{
    return app().d->world;
}

void ServerApp::reset()
{
    DoomsdayApp::reset();
}
