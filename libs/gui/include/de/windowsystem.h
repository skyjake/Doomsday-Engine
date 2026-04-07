/** @file windowsystem.h  Window management subsystem.
 *
 * @todo Deferred window changes should be done using a queue-type solution
 * where it is possible to schedule multiple tasks into the future separately
 * for each window. Each window could have its own queue.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_WINDOWSYSTEM_H
#define LIBAPPFW_WINDOWSYSTEM_H

#include <de/system.h>
#include <de/string.h>
#include <de/vector.h>
#include <de/event.h>

#include "ui/style.h"
#include "basewindow.h"

namespace de {

#undef main

/**
 * Window management subsystem.
 *
 * The window system processes events produced by the input drivers. In practice,
 * the events are passed to the root widgets of the windows.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC WindowSystem : public System
{
public:
    /// Required/referenced Window instance is missing. @ingroup errors
    DE_ERROR(MissingWindowError);

public:
    WindowSystem();

    /**
     * Sets a new style for the application.
     *
     * @param style  Style instance. Ownership taken.
     */
    void setStyle(Style *style);

    template <typename WindowType>
    WindowType *newWindow(const String &id = "main") {
        DE_ASSERT(!findWindow(id));
        WindowType *win = new WindowType(id);
        addWindow(win);
        return win;
    }

    void addWindow(GLWindow *window);

    /**
     * Removes a window from the window system and returns the new main window.
     * The main window changes only when removing the main window itself.
     *
     * @param window  Window to remove.
     * @return New main window.
     */
    void removeWindow(GLWindow &window);

    /**
     * Sets or changes the main window. The first window that is added to the window
     * system automatically becomes the main window.
     *
     * @param id  Window ID.
     */
    void setMainWindow(const String &id);

    void setFocusedWindow(const String &id);

    /**
     * @return Number of windows.
     */
    dsize count() const;

    /**
     * Find a window.
     *
     * @param id  Window identifier. "main" for the main window.
     *
     * @return Window instance or @c NULL if not found.
     */
    GLWindow *findWindow(const String &id) const;

    template <typename WindowType>
    WindowType *find(const String &id) const
    {
        return maybeAs<WindowType>(findWindow(id));
    }

    LoopResult forAll(const std::function<LoopResult(GLWindow &)> &func);

    /**
     * Closes all windows, including the main window.
     */
    void closeAll();

    /**
     * Returns the window system's UI style.
     */
    Style &style();

    /**
     * Poll SDL events and dispatch them to the window(s) that should handle them.
     */
    void pollAndDispatchEvents();

    // System.
    void timeChanged(const Clock &);

    DE_AUDIENCE(AllClosing, void allWindowsClosing())

public:
//    static void setAppWindowSystem(WindowSystem &winSys);
    static WindowSystem &get();

    /**
     * Returns @c true if a main window is available.
     */
    static bool mainExists();

    /**
     * Returns the main window.
     */
    static GLWindow &getMain();

    static GLWindow *focusedWindow();

    /**
     * Returns a pointer to the @em main window.
     */
    inline static GLWindow *mainPtr() { return mainExists() ? &getMain() : nullptr; }

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_WINDOWSYSTEM_H
