/** @file textwidget.h  Generic widget with a text-based visual.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "../widget.h"
#include "../rulerectangle.h"
#include "textcanvas.h"

namespace de { namespace term {

class Action;
class TextRootWidget;

/**
 * Generic text-based UI control.
 *
 * Widget is the base class for all text-based widgets in libshell, because they are
 * intended to be device-independent and compatible with all character-based
 * UIs, regardless of whether the underlying device is text-only or graphical.
 *
 * It is assumed that the root widget under which text widgets are used is
 * derived from TextRootWidget.
 *
 * @ingroup textUi
 */
class DE_PUBLIC Widget : public de::Widget
{
public:
    Widget(const String &name = String());

    TextRootWidget &root() const;

    /**
     * Sets the text canvas on which this widget is to be drawn. Calling this
     * is optional; by default all widgets use the root widget's canvas.
     *
     * @param canvas  Canvas for drawing. Ownership not taken.
     */
    void setTargetCanvas(TextCanvas *canvas);

    /**
     * Returns the text canvas on which this widget is to be drawn. Derived
     * classes can use this method to find out where to draw themselves.
     *
     * @return Destination canvas for drawing.
     */
    TextCanvas &targetCanvas() const;

    /**
     * Requests the root widget to redraw all the user interface.
     */
    void redraw();

    /**
     * Draw this widget and all its children, and show the target canvas
     * afterwards. Use this in special cases for faster redrawing of portions
     * of the screen when only one widget's contents have changed and need
     * updating.
     */
    void drawAndShow();

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    RuleRectangle &rule();

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    const RuleRectangle &rule() const;

    /**
     * Returns the position of the cursor for the widget. If the widget
     * has focus, this is where the cursor will be positioned.
     *
     * @return Cursor position.
     */
    virtual Vec2i cursorPosition() const;

    /**
     * Adds a new action for the widget. During event processing actions are
     * checked in the order they have been added to the widget.
     *
     * @param action  Action instance. Ownership taken.
     */
    void addAction(RefArg<Action> action);

    /**
     * Removes an action from the widget.
     *
     * @param action  Action instance.
     */
    void removeAction(Action &action);

    /**
     * Checks actions and triggers them when suitable event is received.
     */
    bool handleEvent(const Event &event);

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

