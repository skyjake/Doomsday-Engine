/** @file windoweventhandler.h  Window event handling.
 *
 * @authors Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_WINDOWEVENTHANDLER_H
#define LIBGUI_WINDOWEVENTHANDLER_H

#include <de/Observers>
#include <de/libcore.h>
#include <de/Vector>

#include "../KeyEventSource"
#include "../MouseEventSource"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace de {

class GLWindow;

/**
 * Window event handler. Receives native window events (via Qt) and either handles
 * them here or passes them onward.
 *
 * WindowEventHandler derived from KeyEventSource and MouseEventSource so that it
 * can submit user input to interested parties.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC WindowEventHandler : public QObject, public KeyEventSource, public MouseEventSource
{
    Q_OBJECT

public:
    /**
     * Notified when the canvas gains or loses input focus.
     */
    DE_DEFINE_AUDIENCE2(FocusChange, void windowFocusChanged(GLWindow &, bool hasFocus))

public:
    explicit WindowEventHandler(GLWindow *parent);

    GLWindow *window() const;

    /**
     * When the mouse is trapped, all mouse input is grabbed, the mouse cursor
     * is hidden, and mouse movement is submitted as deltas via EventSource.
     *
     * @param trap  Enable or disable the mouse trap.
     */
    void trapMouse(bool trap = true);

    /**
     * Determines if the mouse is presently trapped by the canvas.
     */
    bool isMouseTrapped() const;

    // Native events.
    void focusInEvent(QFocusEvent *ev);
    void focusOutEvent(QFocusEvent *ev);
    void keyPressEvent(QKeyEvent *ev);
    void keyReleaseEvent(QKeyEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void mouseDoubleClickEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void wheelEvent(QWheelEvent *ev);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_WINDOWEVENTHANDLER_H
