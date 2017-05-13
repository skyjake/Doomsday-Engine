/** @file guiapp.h  Application with GUI support.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGUI_GUIAPP_H
#define LIBGUI_GUIAPP_H

#include "libgui.h"
#include <de/App>
#include <de/GuiLoop>

/**
 * Macro for conveniently accessing the de::GuiApp singleton instance.
 */
#define DENG2_GUI_APP   (static_cast<de::GuiApp *>(qApp))

#define DENG2_ASSERT_IN_RENDER_THREAD()   DENG2_ASSERT(de::GuiApp::inRenderThread())

#if defined (DENG_MOBILE)
#include <QGuiApplication>
#  define LIBGUI_GUIAPP_BASECLASS  QGuiApplication
#else
#include <QApplication>
#  define LIBGUI_GUIAPP_BASECLASS  QApplication
#endif

namespace de {

/**
 * Application with GUI support.
 *
 * The event loop is protected against uncaught exceptions. Catches the
 * exception and shuts down the app cleanly.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC GuiApp : public LIBGUI_GUIAPP_BASECLASS
                           , public App
                           , DENG2_OBSERVES(Loop, Iteration)
{
    Q_OBJECT

public:
    static void setDefaultOpenGLFormat(); // call before constructing GuiApp

    GuiApp(int &argc, char **argv);

    void setMetadata(String const &orgName, String const &orgDomain,
                     String const &appName, String const &appVersion);

    bool notify(QObject *receiver, QEvent *event);

    /**
     * Emits the displayModeChanged() signal.
     *
     * @todo In the future when de::GuiApp (or a sub-object owned by it) is
     * responsible for display modes, this should be handled internally and
     * not via this public interface where anybody can call it.
     */
    void notifyDisplayModeChanged();

    int execLoop();
    void stopLoop(int code);

    GuiLoop &loop();

    /**
     * Determines if the currently executing thread is the rendering thread.
     * This may be the same thread as the main thread.
     */
    static bool inRenderThread();
    
    static void setRenderThread(QThread *thread);    
    
protected:
    NativePath appDataPath() const;

    // Observes Loop iteration.
    void loopIteration();

signals:
    /// Emitted when the display mode has changed.
    void displayModeChanged();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GUIAPP_H
