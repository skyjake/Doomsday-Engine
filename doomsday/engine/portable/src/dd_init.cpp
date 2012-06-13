/**
 * @file dd_init.cpp
 * Application entrypoint. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

/**
 * @page mainFlow Engine Control Flow
 *
 * The main Qt application instance is de::App, which is a slightly modified
 * version of the normal QApplication: it catches stray exceptions and forces a
 * clean shutdown of the application.
 *
 * LegacyCore is a thin wrapper around de::App that manages the event loop in a
 * way that is compatible with the legacy C implementation. The LegacyCore
 * instance is created in the main() function and is globally available
 * throughout the libdeng implementation as de2LegacyCore.
 *
 * The application's event loop is started as soon as the main window has been
 * created (but not shown yet). After the window appears with a fully
 * functional OpenGL drawing surface, the rest of the engine initialization is
 * completed. This is done via a callback in the Canvas class that gets called
 * when the window actually appears on screen (with empty contents).
 *
 * While the event loop is running, it periodically calls the loop callback
 * function that has been set via LegacyCore. Initially it is used for showing
 * the main window while the loop is already running
 * (continueInitWithEventLoopRunning()) after which it switches to the engine's
 * main loop callback (DD_GameLoopCallback()).
 *
 * During startup the engine goes through a series of busy tasks. While a busy
 * task is running, the event loop started in LegacyCore is blocked. However,
 * BusyTask starts another loop that continues to handle events received by the
 * application, including making calls to the loop callback function. Busy mode
 * uses its own loop callback function that monitors the progress of the busy
 * worker and keeps updating the busy mode progress indicator on screen. After
 * busy mode ends, the main loop callback is restored.
 *
 * The rate at which the main loop calls the loop callback can be configured
 * via LegacyCore.
 */

#include <QApplication>
#include <QSettings>
#include <QAction>
#include <QMenuBar>
#include <QNetworkProxyFactory>
#include <QDebug>
#include <stdlib.h>
#include <de/App>
#include <de/Log>
#include <de/c_wrapper.h>
#include "de_platform.h"
#include "dd_loop.h"
#include "con_main.h"
#include "window.h"
#include "garbage.h"
#include "displaymode.h"
#include "updater.h"
#include "sys_system.h"

extern "C" {

/// @todo  Refactor this away.
uint mainWindowIdx;   // Main window index.

boolean DD_Init(void);

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

/**
 * libdeng2 application core.
 */
LegacyCore* de2LegacyCore;

}

static void continueInitWithEventLoopRunning(void)
{
    // This function only needs to be called once, so clear the callback.
    LegacyCore_SetLoopFunc(de2LegacyCore, 0);

    // Show the main window. This causes initialization to finish (in busy mode)
    // as the canvas is visible and ready for initialization.
    Window_Show(Window_Main(), true);
}

static void handleLegacyCoreTerminate(const char* msg)
{
    Con_Error("Application terminated due to exception:\n%s\n", msg);
}

/**
 * Application entry point.
 */
int main(int argc, char** argv)
{
#ifdef Q_WS_X11
    // Autodetect non-graphical mode on X11.
    bool useGUI = (getenv("DISPLAY") != 0);
#else
    bool useGUI = true;
#endif

    // Are we running in novideo mode?
    for(int i = 1; i < argc; ++i)
    {
        if(!stricmp(argv[i], "-novideo") || !stricmp(argv[i], "-dedicated"))
        {
            // Console mode.
            useGUI = false;
        }
    }

    Garbage_Init();

    // Application core.
    de::App dengApp(argc, argv, useGUI);

    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    QApplication::setOrganizationDomain("dengine.net");
    QApplication::setOrganizationName("Deng Team");
    QApplication::setApplicationName("Doomsday Engine");
    QApplication::setApplicationVersion(DOOMSDAY_VERSION_BASE);

    // C interface to the app.
    de2LegacyCore = LegacyCore_New(&dengApp);
    LegacyCore_SetTerminateFunc(de2LegacyCore, handleLegacyCoreTerminate);

    QMenuBar* menuBar = 0;
    if(useGUI)
    {
        DisplayMode_Init();

        // Check for updates automatically.
        Updater_Init();

#ifdef MACOSX
        // Set up the application-wide menu.
        menuBar = new QMenuBar;
        QMenu* gameMenu = menuBar->addMenu("&Game");
        QAction* checkForUpdates = gameMenu->addAction(
                    "Check For &Updates...", Updater_Instance(), SLOT(checkNowShowingProgress()));
        checkForUpdates->setMenuRole(QAction::ApplicationSpecificRole);
#endif
    }

    // Initialize.
#if WIN32
    if(!DD_Win32_Init()) return 1;
#elif UNIX
    if(!DD_Unix_Init(argc, argv)) return 1;
#endif

    // Create the main window.
    char title[256];
    DD_ComposeMainWindowTitle(title);
    Window_New(novideo? WT_CONSOLE : WT_NORMAL, title);

    LegacyCore_SetLoopFunc(de2LegacyCore, continueInitWithEventLoopRunning);

    // Run the main loop.
    int result = DD_GameLoop();

    // Cleanup.
    Sys_Shutdown();
    DD_Shutdown();
    LegacyCore_Delete(de2LegacyCore);

    delete menuBar;

    return result;
}
