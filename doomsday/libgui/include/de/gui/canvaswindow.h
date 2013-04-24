/** @file canvaswindow.h  Top-level window with a Canvas.
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

#ifndef LIBGUI_CANVASWINDOW_H
#define LIBGUI_CANVASWINDOW_H

#include <QMainWindow>
#include <de/Canvas>
#include <de/Vector>
#include <de/NativePath>

namespace de {

/**
 * Top-level window that contains an OpenGL drawing canvas. @ingroup base
 *
 * CanvasWindow is the window frame and Canvas is the drawing surface. These
 * two are in separate classes as the former is a top-level window and the
 * latter is a regular widget inside the window. Also, the canvas may need to
 * be replaced with one using a different OpenGL surface (for instance when
 * switching on FSAA). This @em recreation of the canvas is managed here in
 * CanvasWindow.
 *
 * @see Canvas
 */
class  LIBGUI_PUBLIC CanvasWindow : public QMainWindow,
                                    DENG2_OBSERVES(Canvas, GLReady),
                                    DENG2_OBSERVES(Canvas, GLDraw)
{
    Q_OBJECT

public:
    typedef Vector2ui Size;

public:
    CanvasWindow();

    /**
     * Determines if the canvas is ready for GL drawing.
     */
    bool isReady() const;

    float frameRate() const;

    /**
     * Determines the current top left corner (origin) of the window.
     */
    inline Vector2i pos() const { return Vector2i(x(), y()); }

    inline Size size() const { return Size(de::max(0, width()), de::max(0, height())); }

    /**
     * Determines the current width of window's Canvas in pixels.
     */
    inline int width() const { return canvas().width(); }

    /**
     * Determines the current height of window's Canvas in pixels.
     */
    inline int height() const { return canvas().height(); }

    /**
     * Recreates the contained Canvas with an updated GL format. The context is
     * shared with the old Canvas.
     */
    void recreateCanvas();

    Canvas& canvas() const;

    /**
     * Determines if a Canvas instance is owned by this window.
     *
     * @param c  Canvas instance.
     *
     * @return @c true or @c false.
     */
    bool ownsCanvas(Canvas *c) const;

    // Events.
#ifdef WIN32
    bool event(QEvent *); // Alt key kludge
#endif
    void hideEvent(QHideEvent *);

    /**
     * Called when the Canvas is ready for OpenGL drawing (and visible).
     * Overriding methods must call this.
     *
     * @param canvas  Canvas.
     */
    virtual void canvasGLReady(Canvas &canvas);

    /**
     * Called from Canvas when a GL draw is requested. Overriding methods
     * must call this as the last operation (updates frame rate statistics).
     */
    virtual void canvasGLDraw(Canvas &);

    enum GrabMode
    {
        GrabNormal,
        GrabHalfSized
    };

    /**
     * Grab the contents of the window into an OpenGL texture.
     *
     * @param grabMode  How to do the grabbing.
     *
     * @return OpenGL texture name. Caller is reponsible for deleting the texture.
     */
    duint grabAsTexture(GrabMode grabMode = GrabNormal) const;

    /**
     * Grabs the contents of the window and saves it into a native image file.
     *
     * @param fileName  Name of the file to save. May include a file extension
     *                  that indicates which format to use (e.g, "screenshot.jpg").
     *                  If omitted, defaults to PNG.
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool grabToFile(NativePath const &path) const;

    void swapBuffers() const;

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
    void glDone();

    /**
     * Returns a handle to the native window instance. (Platform-specific.)
     */
    void *nativeHandle() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_CANVASWINDOW_H
