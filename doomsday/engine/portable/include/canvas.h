/**
 * @file canvas.h
 * OpenGL drawing surface. @ingroup gl
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "image.h"

#include <QGLWidget>

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
    explicit Canvas(QWidget *parent = 0);
    ~Canvas();
    
    /**
     * Sets a callback function that will be called when the canvas is ready
     * for GL initialization. The OpenGL context and drawing surface are not
     * ready to be used before that. The callback will only be called once
     * during the lifetime of the Canvas.
     *
     * @param canvasInitializeFunc  Callback.
     */
    void setInitFunc(void (*canvasInitializeFunc)(Canvas&));

    /**
     * Sets a callback function that is responsible for drawing the canvas
     * contents when it gets painted. Setting a @c NULL callback will cause the
     * canvas to be filled with black.
     *
     * @param canvasDrawFunc  Callback.
     */
    void setDrawFunc(void (*canvasDrawFunc)(Canvas&));

    /**
     * Sets a callback function that gets called after the size of the canvas changes.
     *
     * @param canvasResizedFunc  Callback.
     */
    void setResizedFunc(void (*canvasResizedFunc)(Canvas&));

    /**
     * Forces immediate repainting of the canvas. The draw callback gets called.
     */
    void forcePaint();

    QImage grabImage(const QSize& outputSize = QSize());

    GLuint grabAsTexture(const QSize& outputSize = QSize());

    /**
     * Grabs the contents of the canvas framebuffer into a raw RGB image.
     *
     * @param img         Grabbed image data. Caller gets ownership.
     *                    Use GL_DestroyImage() to destroy the image_t contents.
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     */
    void grab(image_t* img, const QSize& outputSize = QSize());

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
    void wheelEvent(QWheelEvent* ev);
    void showEvent(QShowEvent* ev);

protected slots:
    void notifyInit();
    void trackMousePosition();

private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_CANVAS_H
