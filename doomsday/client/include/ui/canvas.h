/**
 * @file canvas.h
 * OpenGL drawing surface. @ingroup gl
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_CANVAS_H
#define LIBDENG_CANVAS_H

#ifndef __CLIENT__
#  error "canvas.h requires __CLIENT__"
#endif

#include <de/Observers>
#include <de/libdeng2.h>

#include <QGLWidget>

struct image_s; // see image.h
class CanvasWindow;

#ifdef Q_OS_LINUX
//#  define LIBDENG_CANVAS_TRACK_WITH_MOUSE_MOVE_EVENTS
//#  define LIBDENG_CANVAS_XWARPPOINTER
#endif

#ifdef MACOS_10_4
#  define LIBDENG_CANVAS_TRACK_WITH_MOUSE_MOVE_EVENTS
#endif

/**
 * Drawing canvas with an OpenGL context and window surface. Each CanvasWindow
 * creates one Canvas instance on which to draw. Buffer swapping must be done
 * manually when appropriate.
 *
 * @ingroup gl
 */
class Canvas : public QGLWidget
{
    Q_OBJECT

public:
    explicit Canvas(CanvasWindow *parent, QGLWidget* shared = 0);
    
    /**
     * Sets a callback function that will be called when the canvas is ready
     * for GL initialization. The OpenGL context and drawing surface are not
     * ready to be used before that. The callback will only be called once
     * during the lifetime of the Canvas.
     *
     * @param canvasInitializeFunc  Callback.
     */
    void setInitFunc(void (*canvasInitializeFunc)(Canvas&));

    void setParent(CanvasWindow *parent);

    /**
     * Sets the callback function that is called when the window's focus state changes.
     * The callback is given @c true or @c false as argument, with
     *  - @c true   Focus was gained.
     *  - @c false  Focus was lost.
     *
     * @param canvasFocusChanged  Callback function.
     */
    void setFocusFunc(void (*canvasFocusChanged)(Canvas&, bool));

    /**
     * Copies the callback functions of another Canvas to this one.
     *
     * @param other  Canvas to copy callbacks from.
     */
    void useCallbacksFrom(Canvas& other);

    /**
     * Grabs the contents of the canvas framebuffer.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(const QSize& outputSize = QSize());

    /**
     * Grabs the contents of the canvas framebuffer and creates an OpenGL
     * texture out of it.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  OpenGL texture name. Caller is responsible for deleting the texture.
     */
    GLuint grabAsTexture(const QSize& outputSize = QSize());

    /**
     * Grabs the contents of the canvas framebuffer into a raw RGB image.
     *
     * @param img         Grabbed image data. Caller gets ownership.
     *                    Use GL_DestroyImage() to destroy the image_t contents.
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     */
    void grab(struct image_s* img, const QSize& outputSize = QSize());

    /**
     * When the mouse is trapped, all mouse input is grabbed, the mouse cursor
     * is hidden, and mouse movement is submitted as deltas to the input
     * subsystem.
     *
     * @param trap  Enable or disable the mouse trap.
     */
    void trapMouse(bool trap = true);

    bool isMouseTrapped() const;

    /**
     * Determines if the mouse cursor is currently visible or not.
     */
    bool isCursorVisible() const;

    /**
     * Redraws the Canvas contents immediately. Does not return until the frame
     * has been swapped to the screen. This means if vsync is enabled, this
     * function will block for several milliseconds. The draw callback is
     * called during the execution of this function.
     */
    void forceImmediateRepaint();

protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    // Events.
    void focusInEvent(QFocusEvent* ev);
    void focusOutEvent(QFocusEvent* ev);
    void keyPressEvent(QKeyEvent* ev);
    void keyReleaseEvent(QKeyEvent* ev);
    void mousePressEvent(QMouseEvent* ev);
    void mouseReleaseEvent(QMouseEvent* ev);
#ifdef LIBDENG_CANVAS_TRACK_WITH_MOUSE_MOVE_EVENTS
    void mouseMoveEvent(QMouseEvent* ev);
#endif
    void wheelEvent(QWheelEvent* ev);
    void showEvent(QShowEvent* ev);

protected slots:
    void notifyInit();
#ifdef LIBDENG_CANVAS_TRACK_WITH_MOUSE_MOVE_EVENTS
    void recenterMouse();
#endif

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDENG_CANVAS_H
