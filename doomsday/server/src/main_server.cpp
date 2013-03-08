/** @file main_server.cpp Server application entrypoint.
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

#include <de/TextApp>
#include <QNetworkProxyFactory>
#include <QDebug>
#include <stdlib.h>
#include <de/Log>
#include <de/Error>
#include <de/c_wrapper.h>
#include <de/garbage.h>
#include "de_platform.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "con_main.h"
#include "ui/displaymode.h"
#include "sys_system.h"
#include "serversystem.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

/**
 * libdeng2 application core.
 */
static LegacyCore* de2LegacyCore;

static void handleAppTerminate(char const *msg)
{
    qFatal("Application terminated due to exception:\n%s\n", msg);
}

/**
 * Application entry point.
 */
int main(int argc, char** argv)
{
    // Application core.
    de::TextApp textApp(argc, argv);
    de::App *dengApp = &textApp;
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

    dengApp->setTerminateFunc(handleAppTerminate);

    ServerSystem serverSystem;
    dengApp->addSystem(serverSystem);

    // Initialization.
    try
    {
        // C interface to the app.
        de2LegacyCore = LegacyCore_New();

        if(!CommandLine_Exists("-stdout"))
        {
            // In server mode, stay quiet on the standard outputs.
            LogBuffer_EnableStandardOutput(false);
        }

        dengApp->initSubsystems();

        Libdeng_Init();

        // Initialize.
#if WIN32
        if(!DD_Win32_Init()) return 1;
#elif UNIX
        if(!DD_Unix_Init()) return 1;
#endif

        DD_FinishInitializationAfterWindowReady();
    }
    catch(de::Error const &er)
    {
        qFatal("App init failed: %s", er.asText().toLatin1().constData());
    }

    // Run the main loop.
    int result = dengApp->execLoop();

    // Cleanup.
    Sys_Shutdown();
    DD_Shutdown();
    LegacyCore_Delete(de2LegacyCore);

    return result;
}

