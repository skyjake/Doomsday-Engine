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
#include <QDebug>
#include <stdlib.h>

#include <de/Log>
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

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

using namespace de;

static ServerApp *serverAppSingleton = 0;

static void handleAppTerminate(char const *msg)
{
    qFatal("Application terminated due to exception:\n%s\n", msg);
}

DENG2_PIMPL(ServerApp)
, DENG2_OBSERVES(Plugins, PublishAPI)
, DENG2_OBSERVES(DoomsdayApp, GameUnload)
, DENG2_OBSERVES(DoomsdayApp, ConsoleRegistration)
{
    QScopedPointer<ServerSystem> serverSystem;
    QScopedPointer<ResourceSystem> resourceSys;
    QScopedPointer<audio::System> audioSys;
    ClientServerWorld world;
    InFineSystem infineSys;

    Instance(Public *i)
        : Base(i)
    {
        serverAppSingleton = thisPublic;

        DoomsdayApp::plugins().audienceForPublishAPI() += this;
        self.audienceForGameUnload() += this;
        self.audienceForConsoleRegistration() += this;
    }

    ~Instance()
    {
        self.audienceForGameUnload() -= this;
        self.audienceForConsoleRegistration() -= this;

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
        if(netGame && isServer)
        {
            N_ServerClose();
        }

        infineSystem().reset();

        if(App_GameLoaded())
        {
            Con_SaveDefaults();
        }
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
        printf("Usage: %s [options]\n", self.commandLine().at(0).toLatin1().constData());
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
    , d(new Instance(this))
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

    d->resourceSys.reset(new ResourceSystem);
    addSystem(*d->resourceSys);

    d->audioSys.reset(new ::audio::System);
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
    if(commandLine().has("--version") || commandLine().has("-version"))
    {
        d->printVersionToStdOut();
        ::exit(0);
    }
    if(commandLine().has("--help") || commandLine().has("-h") || commandLine().has("-?"))
    {
        d->printHelpToStdOut();
        ::exit(0);
    }
#endif

    if(!CommandLine_Exists("-stdout"))
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
    if(!DD_Win32_Init())
    {
        throw Error("ServerApp::initialize", "DD_Win32_Init failed");
    }
#elif UNIX
    if(!DD_Unix_Init())
    {
        throw Error("ServerApp::initialize", "DD_Unix_Init failed");
    }
#endif

    plugins().loadAll();

    DD_FinishInitializationAfterWindowReady();
}

void ServerApp::unloadGame(Game const &upcomingGame)
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

::audio::System &ServerApp::audioSystem()
{
    return *app().d->audioSys;
}

ResourceSystem &ServerApp::resourceSystem()
{
    return *app().d->resourceSys;
}

ClientServerWorld &ServerApp::world()
{
    return app().d->world;
}

void ServerApp::reset()
{
    DoomsdayApp::reset();
}
