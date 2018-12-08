/** @file main_client.cpp Client application entrypoint.
 * @ingroup base
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * The main Qt application instance is ClientApp, based on de::BaseGuiApp.
 *
 * The event loop is started after the application has been initialized. Initialization
 * comprises the creation of subsystems and the main window. As a final step during
 * initialization, the "bootstrap" script is executed. At this point, the main window is
 * not visible yet. After the window appears with a fully functional OpenGL drawing
 * surface, the rest of the engine initialization is completed. This is done via an
 * observer audience in the de::GLWindow class.
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
#include "dd_version.h"

//#include <QDebug>
//#include <QMessageBox>
//#include <QTranslator>
#include <de/EscapeParser>

#include <SDL_messagebox.h>

#if defined (DE_STATIC_LINK)

#include <QtPlugin>
#include <de/Library>

Q_IMPORT_PLUGIN(QIOSIntegrationPlugin)
Q_IMPORT_PLUGIN(QGifPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QTgaPlugin)
Q_IMPORT_PLUGIN(QtQuick2Plugin)
Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)

DE_IMPORT_LIBRARY(importidtech1)
DE_IMPORT_LIBRARY(importudmf)
DE_IMPORT_LIBRARY(importdeh)
DE_IMPORT_LIBRARY(audio_fmod)
DE_IMPORT_LIBRARY(doom)
//DE_IMPORT_LIBRARY(heretic)
//DE_IMPORT_LIBRARY(hexen)
//DE_IMPORT_LIBRARY(doom64)

#endif

#if defined (DE_MOBILE)
#  include <QQuickView>
#  include "ui/clientwindow.h"
#endif

using namespace de;

/**
 * Application entry point.
 */
int main(int argc, char **argv)
{
    init_Foundation();
    int exitCode = 0;
    {
//        ClientApp::setDefaultOpenGLFormat();

        ClientApp clientApp(makeList(argc, argv));

        /**
         * @todo Translations are presently disabled because lupdate can't seem to
         * parse tr strings from inside private implementation classes. Workaround
         * or fix is needed?
         */
/*#if 0
        // Load the current locale's translation.
        QTranslator translator;
        translator.load(QString("client_") + QLocale::system().name());
        clientApp.installTranslator(&translator);
#endif*/

        try
        {
/*
 #if defined (DE_MOBILE)
            // On mobile, Qt Quick is actually in charge of drawing the screen.
            // GLWindow is just an item that draws the UI background.
            qmlRegisterType<de::GLQuickItemT<ClientWindow>>("Doomsday", 1, 0, "ClientWindow");
            QQuickView view;
            view.setResizeMode(QQuickView::SizeRootObjectToView);
            view.setSource(QUrl("qrc:///qml/main.qml"));
            view.show();
#endif
*/
            exitCode = clientApp.exec([&]() { clientApp.initialize(); });
        }
        catch (const de::Error &er)
        {
            de::EscapeParser msg;
            msg.parse(er.asText());
            de::warning("App init failed:\n%s", msg.plainText().c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, DOOMSDAY_NICENAME,
                                     "App init failed:\n" + msg.plainText(), nullptr);
            return -1;
        }
    }

    // Check that all reference-counted objects have been deleted.
    #if defined (DE_DEBUG)
    {
        #if defined (DE_USE_COUNTED_TRACING)
        {
            if(de::Counted::totalCount > 0)
            {
                de::Counted::printAllocs();
            }
        }
        #else
        {
            DE_ASSERT(de::Counted::totalCount == 0);
        }
        #endif
    }
    #endif

    return exitCode;
}
