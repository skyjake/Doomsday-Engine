/** @file widget.cpp Base class for widgets.
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

#include "de/Widget"
#include "de/RootWidget"
#include <QList>
#include <QMap>

namespace de {

DENG2_PIMPL(Widget)
{
    Id id;
    String name;
    Widget *parent;
    Behaviors behavior;
    String focusNext;
    String focusPrev;

    typedef QMap<int, Widget *> Routing;
    Routing routing;

    typedef QList<Widget *> Children;
    typedef QMap<String, Widget *> NamedChildren;
    Children children;
    NamedChildren index;

    Instance(Public *i, String const &n) : Base(i), name(n), parent(0)
    {}

    ~Instance()
    {
        clear();
    }

    void clear()
    {
        while(!children.isEmpty())
        {
            children.first()->d->parent = 0;
            delete children.takeFirst();
        }
        index.clear();
    }
};

Widget::Widget(String const &name) : d(new Instance(this, name))
{}

Widget::~Widget()
{
    if(hasRoot() && root().focus() == this)
    {
        root().setFocus(0);
    }

    // Remove from parent automatically.
    if(d->parent)
    {
        d->parent->remove(*this);
    }
}

Id Widget::id() const
{
    return d->id;
}

String Widget::name() const
{
    return d->name;
}

void Widget::setName(String const &name)
{
    // Remove old name from parent's index.
    if(d->parent && !d->name.isEmpty())
    {
        d->parent->d->index.remove(d->name);
    }

    d->name = name;

    // Update parent's index with new name.
    if(d->parent && !name.isEmpty())
    {
        d->parent->d->index.insert(name, this);
    }
}

bool Widget::hasRoot() const
{
    Widget const *w = this;
    while(w)
    {
        if(dynamic_cast<RootWidget const *>(w)) return true;
        w = w->parent();
    }
    return false;
}

RootWidget &Widget::root() const
{
    Widget const *w = this;
    while(w)
    {
        RootWidget const *rw = dynamic_cast<RootWidget const *>(w);
        if(rw) return *const_cast<RootWidget *>(rw);
        w = w->parent();
    }
    throw NotFoundError("Widget::root", "No root widget found");
}

bool Widget::hasFocus() const
{
    return hasRoot() && root().focus() == this;
}

bool Widget::isHidden() const
{
    for(Widget const *w = this; w != 0; w = w->d->parent)
    {
        if(w->d->behavior.testFlag(Hidden)) return true;
    }
    return false;
}

bool Widget::isVisible() const
{
    return !isHidden();
}

void Widget::show(bool doShow)
{
    setBehavior(Hidden, doShow? UnsetFlags : SetFlags);
}

void Widget::setBehavior(Behaviors behavior, FlagOp operation)
{
    applyFlagOperation(d->behavior, behavior, operation);
}

Widget::Behaviors Widget::behavior() const
{
    return d->behavior;
}

void Widget::setFocusNext(String const &name)
{
    d->focusNext = name;
}

void Widget::setFocusPrev(String const &name)
{
    d->focusPrev = name;
}

String Widget::focusNext() const
{
    return d->focusNext;
}

String Widget::focusPrev() const
{
    return d->focusPrev;
}

void Widget::setEventRouting(QList<int> const &types, Widget *routeTo)
{
    foreach(int type, types)
    {
        if(routeTo)
        {
            d->routing.insert(type, routeTo);
        }
        else
        {
            d->routing.remove(type);
        }
    }
}

void Widget::clearEventRouting()
{
    d->routing.clear();
}

bool Widget::isEventRouted(int type, Widget *to) const
{
    return d->routing.contains(type) && d->routing[type] == to;
}

void Widget::clear()
{
    d->clear();
}

Widget &Widget::add(Widget *child)
{
    DENG2_ASSERT(child != 0);
    DENG2_ASSERT(child->d->parent == 0);

    child->d->parent = this;
    d->children.append(child);

    // Update index.
    if(!child->name().isEmpty())
    {
        d->index.insert(child->name(), child);
    }
    return *child;
}

Widget *Widget::remove(Widget &child)
{
    DENG2_ASSERT(child.d->parent == this);
    child.d->parent = 0;

    d->children.removeOne(&child);
    if(!child.name().isEmpty())
    {
        d->index.remove(child.name());
    }
    return &child;
}

Widget *Widget::find(String const &name)
{
    if(d->name == name) return this;

    Instance::NamedChildren::const_iterator found = d->index.constFind(name);
    if(found != d->index.constEnd())
    {
        return found.value();
    }

    // Descend recursively to child widgets.
    DENG2_FOR_EACH(Instance::Children, i, d->children)
    {
        Widget *w = (*i)->find(name);
        if(w) return w;
    }

    return 0;
}

Widget const *Widget::find(String const &name) const
{
    return const_cast<Widget *>(this)->find(name);
}

Widget *Widget::parent() const
{
    return d->parent;
}

String Widget::uniqueName(String const &name) const
{
    return String("#%1.%2").arg(id().asInt64()).arg(name);
}

Widget::NotifyArgs::Result Widget::notifyTree(NotifyArgs const &args)
{
    if(args.preNotifyFunc)
    {
        (this->*args.preNotifyFunc)();
    }

    DENG2_FOR_EACH(Instance::Children, i, d->children)
    {
        if(*i == args.until)
        {
            return NotifyArgs::Abort;
        }

        if(args.conditionFunc && !((*i)->*args.conditionFunc)())
            continue; // Skip this one.

        ((*i)->*args.notifyFunc)();

        if((*i)->notifyTree(args) == NotifyArgs::Abort)
        {
            return NotifyArgs::Abort;
        }
    }

    if(args.postNotifyFunc)
    {
        (this->*args.postNotifyFunc)();
    }

    return NotifyArgs::Continue;
}

void Widget::notifyTreeReversed(NotifyArgs const &args)
{
    if(args.preNotifyFunc)
    {
        (this->*args.preNotifyFunc)();
    }

    for(int i = d->children.size() - 1; i >= 0; --i)
    {
        Widget *w = d->children[i];

        if(args.conditionFunc && !(w->*args.conditionFunc)())
            continue; // Skip this one.

        w->notifyTreeReversed(args);
        (w->*args.notifyFunc)();
    }

    if(args.postNotifyFunc)
    {
        (this->*args.postNotifyFunc)();
    }
}

bool Widget::dispatchEvent(Event const &event, bool (Widget::*memberFunc)(Event const &))
{
    // Hidden widgets do not get events.
    if(isHidden()) return false;

    // Routing has priority.
    if(d->routing.contains(event.type()))
    {
        return d->routing[event.type()]->dispatchEvent(event, memberFunc);
    }

    bool const thisHasFocus = (hasRoot() && root().focus() == this);

    if(d->behavior.testFlag(HandleEventsOnlyWhenFocused) && !thisHasFocus)
    {
        return false;
    }
    if(thisHasFocus)
    {
        // The focused widget is offered events before dispatching to the tree.
        return false;
    }

    // Tree is traversed in reverse order.
    for(int i = d->children.size() - 1; i >= 0; --i)
    {
        Widget *w = d->children[i];
        bool eaten = w->dispatchEvent(event, memberFunc);
        if(eaten) return true;
    }

    if((this->*memberFunc)(event))
    {
        // Eaten.
        return true;
    }

    // Not eaten by anyone.
    return false;
}

Widget::Children Widget::children() const
{
    return d->children;
}

void Widget::initialize()
{}

void Widget::deinitialize()
{}

void Widget::viewResized()
{}

void Widget::focusGained()
{}

void Widget::focusLost()
{}

void Widget::update()
{}

void Widget::draw()
{}

void Widget::preDrawChildren()
{}

void Widget::postDrawChildren()
{}

bool Widget::handleEvent(Event const &)
{
    // Event is not handled.
    return false;
}

void Widget::setFocusCycle(WidgetList const &order)
{
    for(int i = 0; i < order.size(); ++i)
    {
        Widget *a = order[i];
        Widget *b = order[(i + 1) % order.size()];

        a->setFocusNext(b->name());
        b->setFocusPrev(a->name());
    }
}

} // namespace de
