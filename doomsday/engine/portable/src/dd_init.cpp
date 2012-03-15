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

#include <QApplication>
#include <QSettings>
#include <stdlib.h>
#include <de/c_wrapper.h>
#include "de_platform.h"
#include "dd_loop.h"
#include "window.h"
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

    // Application core.
    QApplication dengApp(argc, argv, useGUI);

    // Metadata.
    QApplication::setOrganizationDomain("dengine.net");
    QApplication::setOrganizationName("Deng Team");
    QApplication::setApplicationName("Doomsday Engine");
    QApplication::setApplicationVersion(DOOMSDAY_VERSION_BASE);

    // C interface to the app.
    de2LegacyCore = LegacyCore_New(&dengApp);

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

    return result;
}
