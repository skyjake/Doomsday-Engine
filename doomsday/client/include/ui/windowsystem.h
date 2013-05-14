/** @file windowsystem.h  Window management subsystem.
 *
 * @todo Deferred window changes should be done using a queue-type solution
 * where it is possible to schedule multiple tasks into the future separately
 * for each window. Each window could have its own queue.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef CLIENT_WINDOWSYSTEM_H
#define CLIENT_WINDOWSYSTEM_H

#include <de/System>
#include <de/String>

class ClientWindow;
class Style;

/**
 * Window management subsystem.
 *
 * The window system processes events produced by the input drivers. In
 * practice, the events are passed to the widgets in the windows.
 *
 * @ingroup gui
 */
class WindowSystem : public de::System
{
public:
    /// Required/referenced Window instance is missing. @ingroup errors
    DENG2_ERROR(MissingWindowError);

public:
    WindowSystem();

    /**
     * Constructs a new window using the default configuration. Note that the
     * default configuration is saved persistently when the engine shuts down
     * and is restored when the engine is restarted.
     *
     * Command line options (e.g., -xpos) can be used to modify the window
     * configuration.
     *
     * @param id     Identifier of the window ("main" for the main window).
     *
     * @return ClientWindow instance. Ownership is retained by the window system.
     */
    ClientWindow *createWindow(de::String const &id = "main");

    /**
     * Returns @c true iff a main window is available.
     */
    static bool haveMain();

    /**
     * Returns the main window.
     */
    static ClientWindow &main();

    /**
     * Returns a pointer to the @em main window.
     *
     * @see haveMain()
     */
    inline static ClientWindow *mainPtr() { return haveMain()? &main() : 0; }

    /**
     * Find a window.
     *
     * @param id  Window identifier. "main" for the main window.
     *
     * @return Window instance or @c NULL if not found.
     */
    ClientWindow *find(de::String const &id) const;

    /**
     * Closes all windows, including the main window.
     */
    void closeAll();

    /**
     * Returns the window system's UI style.
     */
    Style &style();

    // System.
    bool processEvent(de::Event const &);
    void timeChanged(de::Clock const &);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_WINDOWSYSTEM_H
