/**
 * @file canvaswindow.h
 * Canvas window. @ingroup base
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

#ifndef LIBDENG_CANVASWINDOW_H
#define LIBDENG_CANVASWINDOW_H

#ifndef __CLIENT__
#  error "canvaswindow.h is only for __CLIENT__"
#endif

#include <QMainWindow>
#include <de/RootWidget>
#include "canvas.h"

/**
 * Top-level window that contains an OpenGL drawing canvas. @ingroup base
 * @see Canvas
 */
class CanvasWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum Mode
    {
        Normal,
        Busy
    };

public:
    explicit CanvasWindow(QWidget *parent = 0);

    de::RootWidget &root();

    /**
     * Sets the operating mode of the window. In Busy mode, the normal
     * widgets of the window will be replaced with a single BusyWidget.
     *
     * @param mode  Mode.
     */
    void setMode(Mode const &mode);

    float frameRate() const;

    /**
     * Recreates the contained Canvas with an updated GL format. The context is
     * shared with the old Canvas. The Canvas will not be recreated if the
     * GL format does not change.
     *
     * @return @c true, if a new Canvas was created. @c false, if recreating
     * the canvas was not needed (format not changed).
     */
    bool recreateCanvas();

    Canvas& canvas();
    bool ownsCanvas(Canvas *c) const;

    /**
     * Sets a callback function for notifying about window movement.
     */
    void setMoveFunc(void (*func)(CanvasWindow &));

    /**
     * Sets a callback function for notifying about window closing.
     * The window closing is cancelled if the callback is defined
     * and returned false.
     */
    void setCloseFunc(bool (*func)(CanvasWindow &));

    // Events.
    bool event(QEvent *);
    void closeEvent(QCloseEvent *);
    void moveEvent(QMoveEvent *);
    void resizeEvent(QResizeEvent *);
    void hideEvent(QHideEvent *);

    /**
     * Called from Canvas when it is ready for OpenGL drawing (visible).
     *
     * @param canvas  Canvas.
     */
    void canvasReady(Canvas &canvas);

    /**
     * Called from Canvas when a GL draw is requested. The UI widgets will be
     * rendered onto the canvas.
     *
     * @param canvas  Canvas to paint.
     */
    void paintCanvas(Canvas &canvas);

    /**
     * Must be called before any canvas windows are created. Defines the
     * default OpenGL format settings for the contained canvases.
     *
     * @return @c true, if the new format was applied. @c false, if the new
     * format remains the same because none of the settings have changed.
     */
    static bool setDefaultGLFormat();

signals:

public slots:

protected:
    static void initCanvasAfterRecreation(Canvas &canvas);

private:
    DENG2_PRIVATE(d)
};

#endif // CANVASWINDOW_H
