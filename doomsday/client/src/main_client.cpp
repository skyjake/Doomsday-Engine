/** @file main_client.cpp Client application entrypoint.
 * @ingroup base
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

/**
 * @page mainFlow Engine Control Flow
 *
 * The main Qt application instance is ClientApp, based on de::GuiApp, a
 * slightly modified version of the normal QApplication: it catches stray
 * exceptions and forces a clean shutdown of the application.
 *
 * LegacyCore is a thin wrapper around de::App that manages the event loop in a
 * way that is compatible with the legacy C implementation. The LegacyCore
 * instance is created in ClientApp and is globally available.
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
 * During startup the engine goes through a series of busy mode tasks. While a
 * busy task is running, the event loop started in LegacyCore is blocked.
 * However, BusyTask starts another loop that continues to handle events
 * received by the application, including making calls to the loop callback
 * function. Busy mode uses its own loop callback function that monitors the
 * progress of the busy worker and keeps updating the busy mode progress
 * indicator on screen. After busy mode ends, the main loop callback is
 * restored.
 *
 * The rate at which the main loop calls the loop callback can be configured
 * via LegacyCore.
 */

#include "clientapp.h"
#include "dd_loop.h"

#include <QDebug>

/**
 * Application entry point.
 */
int main(int argc, char** argv)
{
    ClientApp clientApp(argc, argv);
    try
    {
        clientApp.initialize();
        return clientApp.execLoop();
    }
    catch(de::Error const &er)
    {
        qFatal("App init failed: %s", er.asText().toLatin1().constData());
        return -1;
    }
}
