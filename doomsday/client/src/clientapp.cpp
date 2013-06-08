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
#include <de/DisplayMode>
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
#include "ui/inputsystem.h"
#include "ui/windowsystem.h"
#include "ui/clientwindow.h"
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
    WindowSystem::main().show();
}

DENG2_PIMPL(ClientApp)
{
    QMenuBar *menuBar;
    InputSystem *inputSys;
    std::auto_ptr<WidgetActions> widgetActions;
    WindowSystem *winSys;
    ServerLink *svLink;
    GLShaderBank shaderBank;
    Games games;
    World world;

    Instance(Public *i)
        : Base(i),
          menuBar(0),
          inputSys(0),
          winSys(0),
          svLink(0)
    {
        clientAppSingleton = thisPublic;
    }

    ~Instance()
    {
        Sys_Shutdown();
        DD_Shutdown();

        delete svLink;
        delete winSys;
        delete inputSys;
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
}

void ClientApp::initialize()
{
    Libdeng_Init();

    d->svLink = new ServerLink;

    // Config needs DisplayMode, so let's initialize it before the libdeng2
    // subsystems and Config.
    DisplayMode_Init();

    initSubsystems();

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

    // Load all the shader program definitions.
    FS::FoundFiles found;
    fileSystem().findAll("shaders.dei", found);
    DENG2_FOR_EACH(FS::FoundFiles, i, found)
    {
        LOG_MSG("Loading shader definitions from %s") << (*i)->description();
        d->shaderBank.addFromInfo(**i);
    }

    // Create the window system.
    d->winSys = new WindowSystem;
    addSystem(*d->winSys);

    Plug_LoadAll();

    // Create the main window.
    char title[256];
    DD_ComposeMainWindowTitle(title);
    d->winSys->createWindow()->setWindowTitle(title);

    // Create the input system.
    d->inputSys = new InputSystem;
    addSystem(*d->inputSys);
    d->widgetActions.reset(new WidgetActions);

    App_Timer(1, continueInitWithEventLoopRunning);
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

bool ClientApp::haveApp()
{
    return clientAppSingleton != 0;
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

InputSystem &ClientApp::inputSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->inputSys != 0);
    return *a.d->inputSys;
}

WindowSystem &ClientApp::windowSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->winSys != 0);
    return *a.d->winSys;
}

WidgetActions &ClientApp::widgetActions()
{
    return *app().d->widgetActions.get();
}

GLShaderBank &ClientApp::glShaderBank()
{
    return app().d->shaderBank;
}

Games &ClientApp::games()
{
    return app().d->games;
}

World &ClientApp::world()
{
    return app().d->world;
}
