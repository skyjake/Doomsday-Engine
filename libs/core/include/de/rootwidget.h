/** @file rootwidget.h Widget for managing the root of the UI.
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

#ifndef LIBCORE_ROOTWIDGET_H
#define LIBCORE_ROOTWIDGET_H

#include "de/widget.h"
#include "de/vector.h"

namespace de {

class Rule;
class RuleRectangle;

/**
 * Widget that represents the root of the widget tree.
 *
 * Events passed to and draw requests on the root widget propagate to the
 * entire tree. Other widgets may query the size of the view from the root
 * widget.
 *
 * The view dimensions are available as Rule instances so that widgets'
 * position rules may be defined relative to them.
 *
 * @ingroup widgets
 */
class DE_PUBLIC RootWidget : public Widget, public Lockable
{
public:
    typedef Vec2ui Size;

    /// Notified when the focused widget changes.
    DE_AUDIENCE(FocusChange, void focusedWidgetChanged(Widget *))

public:
    RootWidget();

    Size viewSize() const;

    const RuleRectangle &viewRule() const;
    const Rule &viewLeft() const;
    const Rule &viewRight() const;
    const Rule &viewTop() const;
    const Rule &viewBottom() const;
    const Rule &viewWidth() const;
    const Rule &viewHeight() const;

    /**
     * Sets the size of the view. All widgets in the tree are notified.
     *
     * @param viewSize  View size.
     */
    virtual void setViewSize(const Size &viewSize);

    /**
     * Sets the focus widget. It is the first widget to be offered input
     * events. As focus changes from widget to widget, they will be notified of
     * this via the focusGained() and focusLost() methods.
     *
     * @param widget  Widget to have focus. Set to @c NULL to clear focus.
     */
    void setFocus(const Widget *widget);

    /**
     * Returns the current focus widget.
     */
    Widget *focus() const;

    /**
     * Propagates an event to the full tree of widgets (until it gets eaten).
     *
     * @param event  Event to handle.
     *
     * @return @c true, if event was eaten.
     */
    bool processEvent(const Event &event);

    void initialize();

    /**
     * Update the widget tree. Call this before drawing the widget tree so that
     * the widgets may update their internal state for the current time.
     */
    void update();

    /**
     * Draws the widget tree using the current time.
     */
    void draw();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_ROOTWIDGET_H
