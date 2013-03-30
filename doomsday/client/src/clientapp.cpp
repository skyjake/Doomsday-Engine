/** @file clientapp.cpp  The client application.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QMenuBar>
#include <QAction>
#include <QNetworkProxyFactory>
#include <QDebug>
#include <stdlib.h>

#include <de/Log>
#include <de/Error>
#include <de/c_wrapper.h>
#include <de/garbage.h>

#include "clientapp.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "con_main.h"
#include "sys_system.h"
#include "audio/s_main.h"
#include "gl/gl_main.h"
#include "ui/displaymode.h"
#include "ui/windowsystem.h"
#include "ui/window.h"
#include "updater.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

using namespace de;

static ClientApp *clientAppSingleton = 0;

static void handleLegacyCoreTerminate(char const *msg)
{
    Con_Error("Application terminated due to exception:\n%s\n", msg);
}

static void continueInitWithEventLoopRunning()
{
    // Show the main window. This causes initialization to finish (in busy mode)
    // as the canvas is visible and ready for initialization.
    Window *mainWindow = Window::main();
    DENG_ASSERT(mainWindow != 0);
    mainWindow->show();
}

DENG2_PIMPL(ClientApp)
{
    LegacyCore *legacyCore;
    QMenuBar *menuBar;
    WindowSystem winSys;
    ServerLink *svLink;

    Instance(Public *i)
        : Base(i),
          legacyCore(0),
          menuBar(0),
          svLink(0)
    {
        clientAppSingleton = thisPublic;
    }

    ~Instance()
    {
        Sys_Shutdown();
        DD_Shutdown();

        LegacyCore_Delete(legacyCore);

        delete svLink;
        delete menuBar;
        clientAppSingleton = 0;
    }

    /**
     * Set up an application-wide menu.
     */
    void setupAppMenu()
    {
#ifdef MACOSX
        menuBar = new QMenuBar;
        QMenu* gameMenu = menuBar->addMenu("&Game");
        QAction* checkForUpdates = gameMenu->addAction("Check For &Updates...", Updater_Instance(),
                                                       SLOT(checkNowShowingProgress()));
        checkForUpdates->setMenuRole(QAction::ApplicationSpecificRole);
#endif
    }
};

ClientApp::ClientApp(int &argc, char **argv)
    : GuiApp(argc, argv), d(new Instance(this))
{
    novideo = false;

    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    QCoreApplication::setOrganizationDomain ("dengine.net");
    QCoreApplication::setOrganizationName   ("Deng Team");
    QCoreApplication::setApplicationName    ("Doomsday Engine");
    QCoreApplication::setApplicationVersion (DOOMSDAY_VERSION_BASE);

    setTerminateFunc(handleLegacyCoreTerminate);

    // Subsystems.
    addSystem(d->winSys);
}

void ClientApp::initialize()
{
    d->legacyCore = LegacyCore_New();
    d->svLink = new ServerLink;

    // Config needs DisplayMode, so let's initialize it before the libdeng2
    // subsystems and Config.
    DisplayMode_Init();

    initSubsystems();

    Libdeng_Init();

    // Check for updates automatically.
    Updater_Init();

    d->setupAppMenu();

    // Initialize.
#if WIN32
    if(!DD_Win32_Init())
    {
        throw Error("ClientApp::initialize", "DD_Win32_Init failed");
    }
#elif UNIX
    if(!DD_Unix_Init())
    {
        throw Error("ClientApp::initialize", "DD_Unix_Init failed");
    }
#endif

    // Create the main window.
    char title[256];
    DD_ComposeMainWindowTitle(title);
    new Window(title);

    LegacyCore_Timer(1, continueInitWithEventLoopRunning);
}

void ClientApp::preFrame()
{
    // Frame syncronous I/O operations.
    S_StartFrame(); /// @todo Move to AudioSystem::timeChanged().

    if(gx.BeginFrame) /// @todo Move to GameSystem::timeChanged().
    {
        gx.BeginFrame();
    }
}

void ClientApp::postFrame()
{
    /// @todo Should these be here? Consider multiple windows, each having a postFrame?
    /// Or maybe the frames need to be synced? Or only one of them has a postFrame?

    if(gx.EndFrame)
    {
        gx.EndFrame();
    }

    S_EndFrame();

    Garbage_Recycle();
    loop().resume();
}

ClientApp &ClientApp::app()
{
    DENG2_ASSERT(clientAppSingleton != 0);
    return *clientAppSingleton;
}

ServerLink &ClientApp::serverLink()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->svLink != 0);
    return *a.d->svLink;
}
