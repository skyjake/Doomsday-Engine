/** @file canvas.h  OpenGL drawing surface (QWidget).
 *
 * @authors Copyright (c) 2012-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_CANVAS_H
#define LIBGUI_CANVAS_H

#include <de/Observers>
#include <de/libcore.h>
#include <de/Vector>
#include <QOpenGLWidget>

#include "../KeyEventSource"
#include "../MouseEventSource"
#include "../GLFramebuffer"
#include "../GLTextureFramebuffer"

namespace de {

class CanvasWindow;

/**
 * Drawing canvas with an OpenGL context and window surface. Each CanvasWindow
 * creates one Canvas instance on which to draw. Buffer swapping occurs automatically
 * after the canvas has been painted. The GLSwapped audience is notified whenever a
 * swap is completed.
 *
 * As Canvas is derived from KeyEventSource and MouseEventSource so that it
 * can submit user input to interested parties.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC Canvas : public QOpenGLWidget, public KeyEventSource, public MouseEventSource
{
    Q_OBJECT

public:
    typedef Vector2ui Size;

    /**
     * Notified when the canvas's GL state needs to be initialized. The OpenGL
     * context and drawing surface are not ready before this occurs. This gets
     * called immediately before drawing the contents of the Canvas for the
     * first time (during a paint event).
     */
    DENG2_DEFINE_AUDIENCE2(GLInit, void canvasGLInit(Canvas &))

    /**
     * Notified when a canvas's size has changed.
     */
    DENG2_DEFINE_AUDIENCE2(GLResize, void canvasGLResized(Canvas &))

    /**
     * Notified when the contents of the canvas have been swapped to the window front
     * buffer and are thus visible to the user.
     */
    DENG2_DEFINE_AUDIENCE2(GLSwapped, void canvasGLSwapped(Canvas &))

    /**
     * Notified when the canvas gains or loses input focus.
     */
    DENG2_DEFINE_AUDIENCE2(FocusChange, void canvasFocusChanged(Canvas &, bool hasFocus))

public:
    explicit Canvas(CanvasWindow *parent);

    /**
     * Grabs the contents of the canvas framebuffer.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(QSize const &outputSize = QSize());

    /**
     * Grabs a portion of the contents of the canvas framebuffer.
     *
     * @param area        Portion to grab.
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(QRect const &area, QSize const &outputSize = QSize());

    /**
     * Returns the size of the canvas in device pixels.
     */
    Size size() const;

    /**
     * Returns the width of the canvas in device pixels.
     */
    inline int width() const { return size().x; }

    /**
     * Returns the height of the canvas in device pixels.
     */
    inline int height() const { return size().y; }

    /**
     * When the mouse is trapped, all mouse input is grabbed, the mouse cursor
     * is hidden, and mouse movement is submitted as deltas via EventSource.
     *
     * @param trap  Enable or disable the mouse trap.
     */
    void trapMouse(bool trap = true);

    /**
     * Determines if the mouse is presently trapped by the canvas.
     */
    bool isMouseTrapped() const;

    bool isGLReady() const;

    /**
     * Returns a render target that renders to this canvas.
     *
     * @return GL render target.
     */
    GLTextureFramebuffer &framebuffer() const;

protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    // Native events.
    void focusInEvent(QFocusEvent *ev);
    void focusOutEvent(QFocusEvent *ev);
    void keyPressEvent(QKeyEvent *ev);
    void keyReleaseEvent(QKeyEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void mouseDoubleClickEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void wheelEvent(QWheelEvent *ev);

protected slots:
    void frameWasSwapped();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_CANVAS_H
