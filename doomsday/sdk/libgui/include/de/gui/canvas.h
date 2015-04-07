/** @file canvas.h  Top-level window with an OpenGL drawing surface.
 *
 * @authors Copyright © 2012-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <QOpenGLWindow>
#include <de/GLTexture>
#include <de/NativeFile>
#include <de/NativePath>
#include <de/Observers>
#include <de/Rectangle>
#include <de/Vector>
#include <de/libcore.h>

#include "../GLTarget"
#include "../GLFramebuffer"
#include "../QtInputSource"

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

/**
 * Top-level window that contains an OpenGL drawing surface. @ingroup base
 *
 * Canvas represents the entire native window. It (re)constructs the native surface,
 * handles incoming window events (passing them forward), and owns a GLFramebuffer
 * representing the default framebuffer (i.e., the window surface).
 *
 * @see Canvas
 */
class LIBGUI_PUBLIC Canvas : public QOpenGLWindow, public Asset
{
    Q_OBJECT

public:
    typedef Vector2ui Size;

    DENG2_AS_IS_METHODS()

    /**
     * Notified when GL state needs to be initialized. This is called immediately
     * before drawing the contents of the canvas for the first time (during a paint
     * event).
     */
    DENG2_DEFINE_AUDIENCE2(GLInit, void canvasGLInit(Canvas &))

    /**
     * Notified when a canvas's size has changed.
     */
    DENG2_DEFINE_AUDIENCE2(GLResize, void canvasGLResized(Canvas &))

public:
    Canvas();

    /**
     * Determines the current top left corner (origin) of the window.
     */
    inline Vector2i pos() const { return Vector2i(x(), y()); }

    float frameRate() const;

    QtInputSource &input();
    QtInputSource const &input() const;

    /**
     * Recreates the contained Canvas with an updated GL format. The context is
     * shared with the old Canvas.
     */
    void recreateCanvas();

    /*
    // Events.
#ifdef WIN32
    bool event(QEvent *); // Alt key kludge
#endif
    void hideEvent(QHideEvent *);
    */

    /**
     * Called when the Canvas is ready for OpenGL drawing (and visible).
     *
     * @param canvas  Canvas.
     */
    //virtual void canvasGLReady(Canvas &canvas);

    /**
     * Called from Canvas when a GL draw is requested. Overriding methods
     * must call this as the last operation (updates frame rate statistics).
     */
    //virtual void canvasGLDraw(Canvas &);

    /**
     * Activates the window's GL context so that OpenGL API calls can be made.
     * The GL context is automatically active during the drawing of the window's
     * contents; at other times it needs to be manually activated.
     */
    void glActivate();

    /**
     * Dectivates the window's GL context after OpenGL API calls have been done.
     * The GL context is automatically deactived after the drawing of the window's
     * contents; at other times it needs to be manually deactivated.
     */
    void glDeactivate();

    /**
     * Returns a handle to the native window instance. (Platform-specific.)
     */
    void *nativeHandle() const;

    /**
     * Grabs the contents of the canvas framebuffer.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(QSize const &outputSize = QSize()) const;

    /**
     * Grabs a portion of the contents of the canvas framebuffer.
     *
     * @param area        Portion to grab.
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(QRect const &area, QSize const &outputSize = QSize()) const;

    /**
     * Grabs the contents of the canvas framebuffer and creates an OpenGL
     * texture out of it.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  OpenGL texture name. Caller is responsible for deleting the texture.
     */
    void grab(GLTexture &dest, QSize const &outputSize = QSize()) const;

    void grab(GLTexture &dest, QRect const &area, QSize const &outputSize = QSize()) const;

    enum GrabMode { GrabNormal, GrabHalfSized };

    /**
     * Grab the contents of the window into an OpenGL texture.
     *
     * @param grabMode  How to do the grabbing.
     *
     * @return OpenGL texture name. Caller is reponsible for deleting the texture.
     */
    void grab(GLTexture &dest, GrabMode grabMode) const;

    void grab(GLTexture &dest, Rectanglei const &area, GrabMode grabMode) const;

    /**
     * Grabs the contents of the window and saves it into a native image file.
     *
     * @param path  Name of the file to save. May include a file extension
     *              that indicates which format to use (e.g, "screenshot.jpg").
     *              If omitted, defaults to PNG.
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool grabToFile(NativePath const &path) const;

    /**
     * Returns the size of the OpenGL drawing surface in device pixels.
     */
    Size glSize() const;

    /**
     * Returns the width of the canvas in device pixels.
     */
    inline int width() const { return glSize().x; }

    /**
     * Returns the height of the canvas in device pixels.
     */
    inline int height() const { return glSize().y; }

    GLFramebuffer &framebuffer() const;

    /**
     * Returns a render target that renders to this canvas.
     *
     * @return GL render target.
     */
    inline GLTarget &target() const { return framebuffer().target(); }

    /**
     * Copies or swaps the back buffer to the front, making it visible.
     */
    //void swapBuffers(gl::SwapBufferMode swapMode = gl::SwapMonoBuffer);

/*protected slots:
    void notifyReady();
    void updateSize();*/

    bool isFullScreen() const { return windowState() & Qt::WindowFullScreen; }
    bool isMaximized() const  { return windowState() & Qt::WindowMaximized; }
    bool isMinimized() const  { return windowState() & Qt::WindowMinimized; }
    bool isHidden() const { return !isVisible(); }

protected:
    void exposeEvent(QExposeEvent *);

    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    /**
     * Draw the contents of the window. The contents are drawn immediately and the method
     * does not return until everything has been drawn. The method should draw an entire
     * frame using the non-transformed logical size of the view.
     *
     * Overriding methods must call Canvas::drawCanvas().
     */
    virtual void drawCanvas();

public:
    static bool mainExists();
    static Canvas &main();
    static void setMain(Canvas *canvas);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_CANVAS_H
