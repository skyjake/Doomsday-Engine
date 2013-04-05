/** @file persistentcanvaswindow.h  Canvas window with persistent state.
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

#ifndef LIBGUI_PERSISTENTCANVASWINDOW_H
#define LIBGUI_PERSISTENTCANVASWINDOW_H

#include <de/Error>
#include <de/CanvasWindow>
#include <de/Rectangle>

namespace de {

/**
 * General-purpose top-level window with persistent state.
 *
 * Also supports fullscreen display modes (using DisplayMode).
 */
class PersistentCanvasWindow : public CanvasWindow
{
    Q_OBJECT

public:
    /// Provided window ID was not valid. @ingroup errors
    DENG2_ERROR(InvalidIdError);

    /// Absolute minimum width of a window (in fullscreen also).
    static int const MIN_WIDTH;

    /// Absolute minimum height of a window (in fullscreen also).
    static int const MIN_HEIGHT;

    /**
     * Window attributes.
     */
    enum Attribute
    {
        End = 0, ///< Marks the end of an attribute list (not a valid attribute in itself)

        // Windowed attributes
        Left,
        Top,
        Width,
        Height,
        Centered,
        Maximized,

        // Fullscreen attributes
        Fullscreen,
        FullscreenWidth,
        FullscreenHeight,
        ColorDepthBits,

        // Other
        FullSceneAntialias,
        VerticalSync
    };

public:    
    /**
     * Constructs a new window using the persistent configuration associated
     * with @a id. Note that the configuration is saved persistently when the
     * window is deleted.
     *
     * Command line options (e.g., -xpos) can be used to modify the main window
     * configuration directly.
     *
     * @param id  Identifier of the window.
     */
    PersistentCanvasWindow(String const &id);

    /**
     * Returns @c true iff the window is currently centered.
     */
    bool isCentered() const;

    /**
     * Returns the current window geometry (non-fullscreen).
     */
    Rectanglei windowRect() const;

    Size fullscreenSize() const;

    /**
     * Convenient accessor method for retrieving the x axis origin (in pixels)
     * for the current window geometry.
     */
    inline int x() const { return windowRect().topLeft.x; }

    /**
     * Convenient accessor method for retrieving the y axis origin (in pixels)
     * for the current window geometry.
     */
    inline int y() const { return windowRect().topLeft.y; }

    /**
     * Convenient accessor method for retrieving the width dimension (in pixels)
     * of the current window geometry.
     */
    inline int width() const { return windowRect().width(); }

    /**
     * Convenient accessor method for retrieving the height dimension (in pixels)
     * of the current window geometry.
     */
    inline int height() const { return windowRect().height(); }

    inline int fullscreenWidth() const { return fullscreenSize().x; }

    inline int fullscreenHeight() const { return fullscreenSize().y; }

    int colorDepthBits() const;

    void show(bool yes = true);

    /**
     * Sets or changes one or more window attributes.
     *
     * @param attribs  Array of values:
     *      <pre>[ attribId, value, attribId, value, ..., 0 ]</pre>
     *      The array must be zero-terminated, as that indicates where the
     *      array ends (see Attribute).
     *
     * @return @c true iff all attribute deltas were validated and the window
     * was then successfully updated accordingly. If any attribute failed to
     * validate, the window will remain unchanged and @a false is returned.
     */
    bool changeAttributes(int const *attribs);

    /**
     * Saves the window's state into a persistent storage so that it can be later
     * on restored. Used at shutdown time to save window geometry.
     */
    void saveToConfig();

    /**
     * Restores the window's state from persistent storage. Used when creating
     * a window to determine its persistent configuration.
     */
    void restoreFromConfig();

    static PersistentCanvasWindow &main();

protected slots:
    void performQueuedTasks();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_PERSISTENTCANVASWINDOW_H
