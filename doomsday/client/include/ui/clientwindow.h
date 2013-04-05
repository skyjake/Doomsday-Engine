/** @file clientwindow.h  Top-level window with UI widgets.
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

#ifndef CLIENT_CLIENTWINDOW_H
#define CLIENT_CLIENTWINDOW_H

#include <de/PersistentCanvasWindow>
#include "ui/guirootwidget.h"
#include "resource/image.h"

/**
 * Macro for conveniently accessing the current active window. There is always
 * one active window, so no need to worry about NULLs. The easiest way to get
 * information about the window where drawing is done.
 */
#define DENG_WINDOW         (&ClientWindow::main())

/**
 * A helpful macro that changes the origin of the window space coordinate system.
 */
#define FLIP(y)             (DENG_WINDOW->height() - (y+1))

/**
 * Top-level window that contains a libdeng2 UI widgets. @ingroup gui
 */
class ClientWindow : public de::PersistentCanvasWindow,
                     DENG2_OBSERVES(de::Canvas, GLInit),
                     DENG2_OBSERVES(de::Canvas, GLResize),
                     DENG2_OBSERVES(de::Canvas, FocusChange)
{
    Q_OBJECT

public:
    enum Mode
    {
        Normal,
        Busy
    };

public:
    ClientWindow(de::String const &id = "main");

    GuiRootWidget &root();

    /**
     * Sets the operating mode of the window. In Busy mode, the normal
     * widgets of the window will be replaced with a single BusyWidget.
     *
     * @param mode  Mode.
     */
    void setMode(Mode const &mode);

    /**
     * Must be called before any canvas windows are created. Defines the
     * default OpenGL format settings for the contained canvases.
     *
     * @return @c true, if the new format was applied. @c false, if the new
     * format remains the same because none of the settings have changed.
     */
    static bool setDefaultGLFormat();

    /**
     * Request drawing the contents of the window as soon as possible.
     */
    void draw();

    /**
     * Determines whether the contents of a window should be drawn during the
     * execution of the main loop callback, or should we wait for an update event
     * from the windowing system.
     */
    bool shouldRepaintManually() const;

    /**
     * Grab the contents of the window into the supplied @a image. Ownership of
     * the image passes to the window for the duration of this call.
     *
     * @param image      Image to fill with the grabbed frame contents.
     * @param halfSized  If @c true, scales the image to half the full size.
     */
    void grab(image_t &image, bool halfSized = false) const;

    void updateCanvasFormat();

    // Events.
    void closeEvent(QCloseEvent *);
    void canvasGLReady(de::Canvas &);
    void canvasGLInit(de::Canvas &);
    void canvasGLDraw(de::Canvas &);
    void canvasGLResized(de::Canvas &);
    void canvasFocusChanged(de::Canvas &, bool hasFocus);

    static ClientWindow &main();

private:
    DENG2_PRIVATE(d)
};

#endif // CANVASWINDOW_H
