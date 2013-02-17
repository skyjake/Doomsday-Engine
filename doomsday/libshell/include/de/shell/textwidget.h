/** @file textwidget.h  Generic widget with a text-based visual.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_TEXTWIDGET_H
#define LIBSHELL_TEXTWIDGET_H

#include "libshell.h"
#include <de/Widget>
#include <de/RuleRectangle>
#include <QObject>
#include <QFlags>
#include "TextCanvas"

namespace de {
namespace shell {

class TextRootWidget;
class Action;

/**
 * Generic widget with a text-based visual.
 *
 * TextWidget is the base class for all widgets in libshell, because they are
 * intended to be device-independent and compatible with all character-based
 * UIs, regardless of whether the underlying device is text-only or graphical.
 *
 * It is assumed that the root widget under which text widgets are used is
 * derived from TextRootWidget.
 *
 * QObject is a base class for signals and slots.
 */
class LIBSHELL_PUBLIC TextWidget : public QObject, public Widget
{
    Q_OBJECT

public:
    TextWidget(String const &name = "");
    virtual ~TextWidget();

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
    RuleRectangle const &rule() const;

    /**
     * Returns the position of the cursor for the widget. If the widget
     * has focus, this is where the cursor will be positioned.
     *
     * @return Cursor position.
     */
    virtual Vector2i cursorPosition() const;

    /**
     * Adds a new action for the widget. During event processing actions are
     * checked in the order they have been added to the widget.
     *
     * @param action  Action instance. Ownership taken.
     */
    void addAction(Action *action);

    /**
     * Removes an action from the widget.
     *
     * @param action  Action instance.
     */
    void removeAction(Action &action);

    /**
     * Checks actions and triggers them when suitable event is received.
     */
    bool handleEvent(Event const &event);

private:
    struct Instance;
    Instance *d;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_TEXTWIDGET_H
