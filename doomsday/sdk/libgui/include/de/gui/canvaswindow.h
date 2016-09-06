/** @file canvaswindow.h  Top-level window with a Canvas.
 *
 * @authors Copyright © 2012-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_CANVASWINDOW_H
#define LIBGUI_CANVASWINDOW_H

#include <QMainWindow>
#include <de/Canvas>
#include <de/Vector>
#include <de/Rectangle>
#include <de/NativePath>

#include "../GLTextureFramebuffer"

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

/**
 * Top-level window that contains an OpenGL drawing canvas. @ingroup gui
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
class LIBGUI_PUBLIC CanvasWindow : public QMainWindow, public Asset
{
    Q_OBJECT

public:
    typedef Vector2ui Size;

    DENG2_AS_IS_METHODS()

public:
    CanvasWindow();

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

    enum GrabMode
    {
        GrabNormal,
        GrabHalfSized
    };

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

    virtual void draw();

public:
    static bool mainExists();
    static CanvasWindow &main();
    static void setMain(CanvasWindow *window);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_CANVASWINDOW_H
