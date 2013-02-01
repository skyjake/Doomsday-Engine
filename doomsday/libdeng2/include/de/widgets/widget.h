/** @file widget.h Base class for widgets.
 *
 * @defgroup widgets  Widget Framework
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

/**
 * Base class for widgets.
 * @ingroup widgets
 */
class DENG2_PUBLIC Widget
{
public:
    /// Widget that was expected to exist was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    enum Behavior
    {
        /// Widget is invisible: not drawn. Hidden widgets also receive no events.
        Hidden = 0x1,

        /// Widget will only receive events if it has focus.
        HandleEventsOnlyWhenFocused = 0x2,

        DefaultBehavior = 0
    };
    Q_DECLARE_FLAGS(Behaviors, Behavior)

    typedef QList<Widget *> Children;

public:
    Widget(String const &name = "");
    virtual ~Widget();

    /**
     * Returns the automatically generated, unique identifier of the widget.
     */
    duint32 id() const;

    String name() const;
    void setName(String const &name);
    bool hasRoot() const;
    RootWidget &root() const;
    bool hasFocus() const;
    bool isHidden() const;
    inline void hide() { show(false); }
    void show(bool doShow = true);

    /**
     * Sets or clears one or more behavior flags.
     *
     * @param behavior  Flags.
     * @param set       @c true to set, @c false to clear.
     */
    void setBehavior(Behaviors behavior, bool set = true);

    Behaviors behavior() const;

    /**
     * Sets the identifier of the widget that will receive focus when
     * a focus navigation is requested.
     *
     * @param name  Name of a widget.
     */
    void setFocusNext(String const &name);

    String focusNext() const;

    // Tree organization.
    void clear();
    Widget &add(Widget *child);
    Widget *remove(Widget &child);
    Widget *find(String const &name);
    Widget const *find(String const &name) const;
    Children children() const;
    Widget *parent() const;

    // Utilities.
    void notifyTree(void (Widget::*notifyFunc)());
    void notifyTreeReversed(void (Widget::*notifyFunc)());
    bool dispatchEvent(Event const *event, bool (Widget::*memberFunc)(Event const *));

    // Events.
    virtual void initialize();
    virtual void viewResized();
    virtual void update();
    virtual void drawIfVisible();
    virtual void draw();
    virtual bool handleEvent(Event const *event);

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // LIBDENG2_WIDGET_H
