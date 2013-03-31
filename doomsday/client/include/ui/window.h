/** @file window.h Window management. @ingroup base
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef LIBDENG_BASE_WINDOW_H
#define LIBDENG_BASE_WINDOW_H

#ifndef __CLIENT__
#  error "window.h requires __CLIENT__"
#endif

#include <QRect>

#include "dd_types.h"
#include "resource/image.h"
#include "canvaswindow.h"
#include <de/Error>

/**
 * Macro for conveniently accessing the current active window. There is always
 * one active window, so no need to worry about NULLs. The easiest way to get
 * information about the window where drawing is done.
 */
#define DENG_WINDOW         (&Window::main())

/**
 * A helpful macro that changes the origin of the window space coordinate system.
 */
#define FLIP(y)             (DENG_WINDOW->height() - (y+1))

/**
 * Window and window management.
 *
 * @deprecated  Windows will be represented by CanvasWindow instances.
 */
class Window
{
public:
    /// Required/referenced Window instance is missing. @ingroup errors
    DENG2_ERROR(MissingWindowError);

    /// Minimum width of a window (in fullscreen also).
    static const int MIN_WIDTH = 320;

    /// Minimum height of a window (in fullscreen also).
    static const int MIN_HEIGHT = 240;

    /**
     * Logical window attribute identifiers.
     */
    enum
    {
        End, ///< Marks the end of an attribute list (not a valid attribute in itself)

        X,
        Y,
        Width,
        Height,
        Centered,
        Maximized,
        Fullscreen,
        Visible,
        ColorDepthBits
    };

public:
    /**
     * Initialize the window manager.
     * Tasks include; checking the system environment for feature enumeration.
     */
    static void initialize();

    /**
     * Shutdown the window manager.
     */
    static void shutdown();

    /**
     * Constructs a new window using the default configuration. Note that the
     * default configuration is saved persistently when the engine shuts down
     * and is restored when the engine is restarted.
     *
     * Command line options (e.g., -xpos) can be used to modify the window
     * configuration.
     *
     * @param title  Text for the window title.
     *
     * @note Ownership of the Window is @em not given to the caller.
     */
    static Window *create(char const *title);

    /**
     * Returns @c true iff a main window is available.
     */
    static bool haveMain();

    /**
     * Returns the main window.
     */
    static Window &main();

    /**
     * Returns a pointer to the @em main window.
     *
     * @see haveMain()
     */
    inline static Window *mainPtr() { return haveMain()? &main() : 0; }

    /**
     * Returns a pointer to the window associated with unique index @a idx.
     */
    static Window *byIndex(uint idx);

public:
    /**
     * Returns @c true iff the window is currently fullscreen.
     */
    bool isFullscreen() const;

    /**
     * Returns @c true iff the window is currently centered.
     */
    bool isCentered() const;

    /**
     * Returns @c true iff the window is currently maximized.
     */
    bool isMaximized() const;

    /**
     * Returns the current geometry of the window.
     */
    QRect rect() const;

    /**
     * Convenient accessor method for retrieving the x axis origin (in pixels)
     * for the current window geometry.
     */
    inline int x() const { return rect().x(); }

    /**
     * Convenient accessor method for retrieving the y axis origin (in pixels)
     * for the current window geometry.
     */
    inline int y() const { return rect().y(); }

    /**
     * Convenient accessor method for retrieving the width dimension (in pixels)
     * of the current window geometry.
     */
    inline int width() const { return rect().width(); }

    /**
     * Convenient accessor method for retrieving the height dimension (in pixels)
     * of the current window geometry.
     */
    inline int height() const { return rect().height(); }

    /**
     * Returns the windowed geometry of the window (used when not maximized or
     * fullscreen).
     */
    QRect normalRect() const;

    inline int normalX() const { return normalRect().x(); }

    inline int normalY() const { return normalRect().y(); }

    inline int normalWidth() const { return normalRect().width(); }

    inline int normalHeight() const { return normalRect().height(); }

    int colorDepthBits() const;

    /**
     * Sets the title of a window.
     *
     * @param title  New title for the window.
     */
    void setTitle(char const *title) const; /// @todo semantically !const

    void show(bool show = true);

    /**
     * Sets or changes one or more window attributes.
     *
     * @param attribs  Array of values:
     *      <pre>[ attribId, value, attribId, value, ..., 0 ]</pre>
     *      The array must be zero-terminated, as that indicates where the
     *      array ends (see windowattribute_e).
     *
     * @return @c true iff all attribute deltas were validated and the window
     * was then successfully updated accordingly. If any attribute failed to
     * validate, the window will remain unchanged and @a false is returned.
     */
    bool changeAttributes(int const *attribs);

    /**
     * Request drawing the contents of the window as soon as possible.
     */
    void draw();

    /**
     * Make the content of the framebuffer visible.
     */
    void swapBuffers() const;

    /**
     * Grab the contents of the window into an OpenGL texture.
     *
     * @param halfSized  If @c true, scales the image to half the full size.
     *
     * @return OpenGL texture name. Caller is reponsible for deleting the texture.
     */
    uint grabAsTexture(bool halfSized = false) const;

    /**
     * Grabs the contents of the window and saves it into an image file.
     *
     * @param fileName  Name of the file to save. May include a file extension
     *                  that indicates which format to use (e.g, "screenshot.jpg").
     *                  If omitted, defaults to PNG.
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool grabToFile(char const *fileName) const;

    /**
     * Grab the contents of the window into the supplied @a image. Ownership of
     * the image passes to the window for the duration of this call.
     *
     * @param image      Image to fill with the grabbed frame contents.
     * @param halfSized  If @c true, scales the image to half the full size.
     */
    void grab(image_t &image, bool halfSized = false) const;

    /**
     * Saves the window's state into a persistent storage so that it can be later
     * on restored. Used at shutdown time to save window geometry.
     */
    void saveState();

    /**
     * Restores the window's state from persistent storage. Used at engine startup
     * to determine the default window geometry.
     */
    void restoreState();

    /**
     * Activates or deactivates the window mouse trap. When trapped, the mouse
     * cursor is not visible and all mouse motions are interpreted as deltas.
     *
     * @param enable  @c true, if the mouse is to be trapped in the window.
     *
     * @note Effectively a wrapper for Canvas::trapMouse().
     */
    void trapMouse(bool enable = true) const; /// @todo semantically !const

    bool isMouseTrapped() const;

    /**
     * Determines whether the contents of a window should be drawn during the
     * execution of the main loop callback, or should we wait for an update event
     * from the windowing system.
     */
    bool shouldRepaintManually() const;

    void updateCanvasFormat();

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

    void *nativeHandle() const;

    /**
     * Returns a pointer to the native widget for the window (may be @c NULL).
     */
    QWidget *widgetPtr();

    /**
     * Returns the CanvasWindow for the window.
     */
    CanvasWindow &canvasWindow();

    /**
     * Utility to call after changing the size of a CanvasWindow. This will
     * update the Window state.
     *
     * @deprecated In the future, size management will be done internally in
     * CanvasWindow/WindowSystem.
     */
    void updateAfterResize();

private:
    /**
     * Constructs a new window using the default configuration. Note that the
     * default configuration is saved persistently when the engine shuts down
     * and is restored when the engine is restarted.
     *
     * Command line options (e.g., -xpos) can be used to modify the window
     * configuration.
     *
     * @param title  Text for the window title.
     */
    Window(char const *title = "");

    /**
     * Close and destroy the window. Its state is saved persistently and used
     * as the default configuration the next time the same window is created.
     */
    ~Window();

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDENG_BASE_WINDOW_H
