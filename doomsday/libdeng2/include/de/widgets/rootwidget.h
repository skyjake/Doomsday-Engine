/** @file rootwidget.h Widget for managing the root of the UI.
 * @ingroup widget
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

#ifndef LIBDENG2_ROOTWIDGET_H
#define LIBDENG2_ROOTWIDGET_H

#include "../Widget"
#include "../Vector"

namespace de {

class Rule;

/**
 * Widget that represents the root of the widget tree.
 *
 * Events passed to and draw requests on the root widget propagate to the
 * entire tree. Other widgets may query the size of the view from the root
 * widget.
 *
 * The view dimensions are available as Rule instances so that widgets'
 * position rules may be defined relative to them.
 */
class RootWidget : public Widget
{
public:
    RootWidget();
    ~RootWidget();

    Vector2i viewSize() const;

    Rule const *viewLeft() const;
    Rule const *viewRight() const;
    Rule const *viewTop() const;
    Rule const *viewBottom() const;
    Rule const *viewWidth() const;
    Rule const *viewHeight() const;

    /**
     * Sets the size of the view. All widgets in the tree are notified.
     *
     * @param viewSize  View size.
     */
    virtual void setViewSize(Vector2i const &viewSize);

    /**
     * Sets the focus widget. It is the first widget to be offered input
     * events.
     *
     * @param widget  Widget to have focus. Set to @c NULL to clear focus.
     */
    void setFocus(Widget *widget);

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
    bool processEvent(Event const *event);

    void initialize();
    void draw();

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // LIBDENG2_ROOTWIDGET_H
