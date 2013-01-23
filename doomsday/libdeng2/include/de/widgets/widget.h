/** @file widget.h Base class for widgets.
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

#ifndef LIBDENG2_WIDGET_H
#define LIBDENG2_WIDGET_H

#include "../String"
#include "../Event"

namespace de {

class RootWidget;

class Widget
{
public:
    /// Widget that was expected to exist was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    Widget(String const &name = "");
    virtual ~Widget();

    String name() const;
    void setName(String const &name);
    bool hasRoot() const;
    RootWidget &root() const;

    // Tree organization.
    void clear();
    Widget &add(Widget *child);
    Widget *remove(Widget &child);
    Widget *find(String const &name);
    Widget const *find(String const &name) const;
    QList<Widget *> children() const;
    Widget *parent() const;

    // Utilities.
    void notifyTree(void (Widget::*notifyFunc)());
    void notifyTreeReversed(void (Widget::*notifyFunc)());
    bool dispatchEvent(Event const *event, bool (Widget::*memberFunc)(Event const *));

    // Events.
    virtual void initialize();
    virtual void viewResized();
    virtual void update();
    virtual void draw();
    virtual bool handleEvent(Event const *event);

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // LIBDENG2_WIDGET_H
