/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_WINDOW_H
#define LIBDENG2_WINDOW_H

#include <de/Rectangle>
#include <QWidget>
#include <QFlags>

#include "glwindowsurface.h"
#include "visual.h"
#include "rules.h"

class Video;

/**
 * The abstract base class for windows in the operating system's windowing system.
 *
 * @ingroup video
 */
class Window : public GLWindowSurface
{
public:
    /// Window is in fullscreen mode.
    enum Flag
    {
        Fullscreen = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag);
        
public:
    Window(const QGLFormat& format, QWidget* parent = 0, const QGLWidget* shareWidget = 0);

    virtual ~Window();
        
    /**
     * Returns the root visual of the window.
     */
    const Visual& root() const { return _root; }

    /**
     * Returns the root visual of the window.
     */
    Visual& root() { return _root; }

    /**
     * Returns the mode of the window.
     */
    const Flags& flags() const { return _flags; }

    /**
     * Changes the value of the mode flags. Subclass should override to
     * update the state of the concrete window.
     *
     * @param modeFlags  Mode flag(s).
     * @param yes  @c true, if the flag(s) should be set. @c false to unset.
     */
    void setSelectedFlags(Flags selectedFlags, bool yes = true);

    /**
     * Sets the flags of the window.
     */
    virtual void setFlags(Flags allFlags);

    void surfaceResized(const QSize& size);

    /**
     * Draws the contents of the window.
     */
    virtual void draw();

private:
    /// Window mode.
    Flags _flags;

    /// Root visual of the window.
    Visual _root;

    ConstantRule* _widthRule;
    ConstantRule* _heightRule;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Window::Flags);

#endif /* LIBDENG2_WINDOW_H */
