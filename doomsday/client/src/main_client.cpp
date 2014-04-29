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
 * The main Qt application instance is ClientApp, based on de::GuiApp, a slightly
 * modified version of the normal QApplication: it catches stray exceptions and forces a
 * clean shutdown of the application.
 *
 * The application's event loop is started as soon as the main window has been created
 * (but not shown yet). After the window appears with a fully functional OpenGL drawing
 * surface, the rest of the engine initialization is completed. This is done via a
 * callback in the Canvas class that gets called when the window actually appears on
 * screen (with empty contents).
 *
 * The application's refresh loop is controlled by de::Loop. Before each frame, clock
 * time advances and de::Loop's iteration audience is notified. This is observed by
 * de::WindowSystem, which updates all widgets. When the GameWidget is updated, it runs
 * game tics and requests a redraw of the window contents.
 *
 * During startup the engine goes through a series of busy mode tasks. While a busy task
 * is running, the application's primary event loop is blocked. However, BusyTask starts
 * another loop that continues handling events received by the application.
 */

#include "clientapp.h"
#include "dd_loop.h"

#include <QDebug>
#include <QMessageBox>
#include <QTranslator>

/**
 * Application entry point.
 */
int main(int argc, char** argv)
{
    ClientApp clientApp(argc, argv);

    /**
     * @todo Translations are presently disabled because lupdate can't seem to
     * parse tr strings from inside private implementation classes. Workaround
     * or fix is needed?
     */
#if 0
    // Load the current locale's translation.
    QTranslator translator;
    translator.load(QString("client_") + QLocale::system().name());
    clientApp.installTranslator(&translator);
#endif

    try
    {
        clientApp.initialize();
        return clientApp.execLoop();
    }
    catch(de::Error const &er)
    {
        qWarning() << "App init failed:\n" << er.asText();
        QMessageBox::critical(0, DOOMSDAY_NICENAME, "App init failed:\n" + er.asText());
        return -1;
    }

#ifdef DENG2_DEBUG
    // Check that all reference-counted objects have been deleted.
    DENG2_ASSERT(de::Counted::totalCount == 0);
#endif
}
