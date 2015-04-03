/** @file qtinputsource.h  Input event source based on Qt events.
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

#ifndef LIBGUI_QTINPUTSOURCE_H
#define LIBGUI_QTINPUTSOURCE_H

#include <de/Observers>
#include <de/libcore.h>
#include <de/Vector>
#include <QObject>

#include "../KeyEventSource"
#include "../MouseEventSource"

namespace de {

/**
 * Listens to incoming input events and produces suitable notifications. Works as
 * a Qt event filter. This is intended to be installed on a CanvasWindow.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC QtInputSource : public QObject, public KeyEventSource, public MouseEventSource
{
    Q_OBJECT

public:
    typedef Vector2ui Size;

    /**
     * Notified when the focus has been gained or lost.
     */
    DENG2_DEFINE_AUDIENCE2(FocusChange, void inputFocusChanged(bool hasFocus))

public:
    /**
     * @param fallbackHandler  Events are passed to this object if QtInputSource
     *                         does not handle them.
     */
    explicit QtInputSource(QObject &fallbackHandler);

    /**
     * Notifies mouse event audience that the mouse should be trapped.
     *
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

    /**
     * Handles/converts incoming events and passes them to appropriate audiences.
     * @param watched  Target object.
     * @param event    Event.
     */
    bool eventFilter(QObject *watched, QEvent *event);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_QTINPUTSOURCE_H
