/** @file glwindow.h  Top-level OpenGL window.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLWINDOW_H
#define LIBGUI_GLWINDOW_H

#include "de/windoweventhandler.h"
#include "de/gltextureframebuffer.h"

#include <de/vector.h>
#include <de/rectangle.h>
#include <de/nativepath.h>

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

class GLTimer;
class Image;

/**
 * Top-level window that contains an OpenGL drawing surface. @ingroup gui
 *
 * @see WindowEventHandler
 */
class LIBGUI_PUBLIC GLWindow : public Asset
{
public:
    using Size = Vec2ui;

    struct LIBGUI_PUBLIC DisplayMode // for fullscreen use
    {
        Vec2i        resolution;
        unsigned int bitDepth;
        int          refreshRate;

        bool operator==(const DisplayMode &other) const
        {
            return resolution == other.resolution &&
                   (!bitDepth || !other.bitDepth || bitDepth == other.bitDepth) &&
                   (!refreshRate || !other.refreshRate || refreshRate == other.refreshRate);
        }

        inline bool operator!=(const DisplayMode &other) const
        {
            return !(*this == other);
        }

        inline bool isDefault() const
        {
            return resolution == Vec2i();
        }

        Vec2i ratio() const
        {
            return de::ratio(resolution);
        }
    };

    /**
     * Notified when the window's GL state needs to be initialized. The OpenGL
     * context and drawing surface are not ready before this occurs. This gets
     * called immediately before drawing the contents of the WindowEventHandler for the
     * first time (during a paint event).
     */
    DE_AUDIENCE(Init, void windowInit(GLWindow &))

    /**
     * Notified when a window size has changed.
     */
    DE_AUDIENCE(Resize, void windowResized(GLWindow &))

    /**
     * Notified when the window's current display changes.
     */
    DE_AUDIENCE(Display, void windowDisplayChanged(GLWindow &))

    /**
     * Notified when the window pixel ratio has changed.
     */
    DE_AUDIENCE(PixelRatio, void windowPixelRatioChanged(GLWindow &))

    /**
     * Notified when the contents of the window have been swapped to the window front
     * buffer and are thus visible to the user.
     */
    DE_AUDIENCE(Swap, void windowSwapped(GLWindow &))

    DE_AUDIENCE(Move,       void windowMoved(GLWindow &, Vec2i))
    DE_AUDIENCE(Visibility, void windowVisibilityChanged(GLWindow &))

    DE_CAST_METHODS()

public:
    GLWindow(const String &id = "main");

    const String &id() const;

    void        setTitle(const String &title);
    void        setIcon(const Image &image);
    void        setMinimumSize(const Size &minSize);
    void        setGeometry(const Rectanglei &rect);
    inline void setGeometry(dint x, dint y, duint width, duint height)
    {
        setGeometry(Rectanglei{x, y, width, height});
    }

    void makeCurrent();
    void doneCurrent();
    void update();
    void show();
    void showNormal();
    void showMaximized();
    void showFullScreen();
    void hide();
    void raise();
    void close();

    bool isGLReady() const;
    bool isFullScreen() const;
    bool isMaximized() const;
    bool isMinimized() const;
    bool isVisible() const;
    bool isHidden() const;

    float frameRate() const;
    uint  frameCount() const;

    inline int x() const { return pos().x; }
    inline int y() const { return pos().y; }

    /**
     * Determines the current top left corner (origin) of the window.
     */
    Vec2i pos() const;

    Size  pointSize() const;
    Size  pixelSize() const;
    duint pointWidth() const;
    duint pointHeight() const;
    duint pixelWidth() const;
    duint pixelHeight() const;

    inline Vec2f pointSizef() const
    {
        const auto n = pointSize();
        return {float(n.x), float(n.y)};
    }
    inline Vec2f pixelSizef() const
    {
        const auto p = pixelSize();
        return {float(p.x), float(p.y)};
    }

    double pixelRatio() const;
    int    displayIndex() const;

    void        setFullscreenDisplayMode(const DisplayMode &mode);
    DisplayMode fullscreenDisplayMode() const;
    DisplayMode desktopDisplayMode() const;
    bool        isNotDesktopDisplayMode() const;

    inline Rectanglei geometry() const { return {x(), y(), pointSize().x, pointSize().y }; }

    Vec2i mapToGlobal(const Vec2i &coordInsideWindow) const;

    /**
     * Returns a render target that renders to this window.
     *
     * @return GL render target.
     */
    GLFramebuffer &framebuffer() const;

    /**
     * Provides access to the GL profiling timers.
     */
    GLTimer &timer() const;

    WindowEventHandler &eventHandler() const;

    /**
     * Determines if a WindowEventHandler instance is owned by this window.
     *
     * @param handler  WindowEventHandler instance.
     *
     * @return @c true or @c false.
     */
    bool ownsEventHandler(WindowEventHandler *handler) const;

//    void checkNativeEvents();

    enum GrabMode { GrabNormal, GrabHalfSized };

    /**
     * Grabs the contents of the window and saves it into a native image file.
     *
     * @param path  Name of the file to save. May include a file extension
     *              that indicates which format to use (e.g, "screenshot.jpg").
     *              If omitted, defaults to PNG.
     */
    void grabToFile(const NativePath &path) const;

    /**
     * Grabs the contents of the window framebuffer.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    Image grabImage(const Size &outputSize = Size()) const;

    /**
     * Grabs a portion of the contents of the window framebuffer.
     *
     * @param area        Portion to grab.
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    Image grabImage(const Rectanglei &area, const Size &outputSize = Size()) const;

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
     * Returns a handle to the SDL window instance.
     */
    void *sdlWindow() const;

    void handleWindowEvent(const void *ptr);

    virtual void draw() = 0;
    virtual void rootUpdate() = 0;

public:
    static void      setMain(GLWindow *window);
    static bool      mainExists();
    static GLWindow &getMain();
    static void      glActivateMain();
    static GLWindow &current(); // current glActivated window

    /**
     * Enumerates the available display modes of a display.
     * @param displayIndex  Which display.
     * @return List of supported modes.
     */
    static List<DisplayMode> displayModes(int displayIndex);

protected:
    virtual void initializeGL();
    virtual void paintGL();
    virtual void windowAboutToClose();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLWINDOW_H
