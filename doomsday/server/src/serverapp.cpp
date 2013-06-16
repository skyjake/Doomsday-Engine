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
#include <de/garbage.h>

#include "serverapp.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "con_main.h"
#include "sys_system.h"

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
{
    ServerSystem serverSystem;
    Games games;
    World world;

    Instance(Public *i)
        : Base(i)
    {
        serverAppSingleton = thisPublic;
    }

    ~Instance()
    {
        Sys_Shutdown();
        DD_Shutdown();

        serverAppSingleton = 0;
    }
};

ServerApp::ServerApp(int &argc, char **argv)
    : TextApp(argc, argv), d(new Instance(this))
{
    novideo = true;

    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    QCoreApplication::setOrganizationDomain ("dengine.net");
    QCoreApplication::setOrganizationName   ("Deng Team");
    QCoreApplication::setApplicationName    ("Doomsday Server");
    QCoreApplication::setApplicationVersion (DOOMSDAY_VERSION_BASE);

    setTerminateFunc(handleAppTerminate);

    addSystem(d->serverSystem);
}

void ServerApp::initialize()
{
    Libdeng_Init();

    if(!CommandLine_Exists("-stdout"))
    {
        // In server mode, stay quiet on the standard outputs.
        LogBuffer::appBuffer().enableStandardOutput(false);
    }

    initSubsystems();

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

    Plug_LoadAll();

    DD_FinishInitializationAfterWindowReady();
}

bool ServerApp::haveApp()
{
    return serverAppSingleton != 0;
}

ServerApp &ServerApp::app()
{
    DENG2_ASSERT(serverAppSingleton != 0);
    return *serverAppSingleton;
}

ServerSystem &ServerApp::serverSystem()
{
    return app().d->serverSystem;
}

Games &ServerApp::games()
{
    return app().d->games;
}

World &ServerApp::world()
{
    return app().d->world;
}
