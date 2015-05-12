/** @file widget.cpp Base class for widgets.
 * @ingroup widget
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Widget"
#include "de/RootWidget"
#include <QList>
#include <QMap>

namespace de {

DENG2_PIMPL(Widget)
{
    Id id;
    String name;
    Widget *parent = nullptr;
    RootWidget *manualRoot = nullptr;
    Behaviors behavior;
    String focusNext;
    String focusPrev;

    typedef QMap<int, Widget *> Routing;
    Routing routing;

    typedef QList<Widget *> Children;
    typedef QMap<String, Widget *> NamedChildren;
    Children children;
    NamedChildren index;

    Instance(Public *i, String const &n) : Base(i), name(n)
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
            Widget *w = children.takeFirst();
            //qDebug() << "deleting" << w << w->name();
            delete w;
        }
        index.clear();
    }

    RootWidget *findRoot() const
    {
        if(manualRoot)
        {
            return manualRoot;
        }
        Widget const *w = thisPublic;
        while(w->parent())
        {
            w = w->parent();
            if(w->d->manualRoot) return w->d->manualRoot;
        }
        if(w->is<RootWidget>())
        {
            return const_cast<RootWidget *>(&w->as<RootWidget>());
        }
        return nullptr;
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(ParentChange)
    DENG2_PIMPL_AUDIENCE(ChildAddition)
    DENG2_PIMPL_AUDIENCE(ChildRemoval)
};

DENG2_AUDIENCE_METHOD(Widget, Deletion)
DENG2_AUDIENCE_METHOD(Widget, ParentChange)
DENG2_AUDIENCE_METHOD(Widget, ChildAddition)
DENG2_AUDIENCE_METHOD(Widget, ChildRemoval)

Widget::Widget(String const &name) : d(new Instance(this, name))
{}

Widget::~Widget()
{
    if(hasRoot() && root().focus() == this)
    {
        root().setFocus(0);
    }

    audienceForParentChange().clear();

    // Remove from parent automatically.
    if(d->parent)
    {
        d->parent->remove(*this);
    }

    // Notify everyone else.
    DENG2_FOR_AUDIENCE2(Deletion, i)
    {
        i->widgetBeingDeleted(*this);
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

DotPath Widget::path() const
{
    Widget const *w = this;
    String result;
    while(w)
    {
        if(!result.isEmpty()) result = "." + result;
        if(!w->d->name.isEmpty())
        {
            result = w->d->name + result;
        }
        else
        {
            result = QString("0x%1").arg(dintptr(w), 0, 16) + result;
        }
        w = w->parent();
    }
    return result;
}

bool Widget::hasRoot() const
{
    return d->findRoot() != nullptr;
}

RootWidget &Widget::root() const
{
    if(auto *rw = d->findRoot())
    {
        return *rw;
    }
    throw NotFoundError("Widget::root", "No root widget found");
}

void Widget::setRoot(RootWidget *root)
{
    d->manualRoot = root;
}

bool Widget::hasFocus() const
{
    return hasRoot() && root().focus() == this;
}

bool Widget::hasFamilyBehavior(Behavior const &flags) const
{
    for(Widget const *w = this; w != 0; w = w->d->parent)
    {
        if(w->d->behavior.testFlag(flags)) return true;
    }
    return false;
}

void Widget::show(bool doShow)
{
    setBehavior(Hidden, doShow? UnsetFlags : SetFlags);
}

void Widget::setBehavior(Behaviors behavior, FlagOp operation)
{
    applyFlagOperation(d->behavior, behavior, operation);
}

void Widget::unsetBehavior(Behaviors behavior)
{
    applyFlagOperation(d->behavior, behavior, UnsetFlags);
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

void Widget::clearTree()
{
    d->clear();
}

Widget &Widget::add(Widget *child)
{
    DENG2_ASSERT(child != 0);
    DENG2_ASSERT(child->d->parent == 0);

#ifdef _DEBUG
    // Can't have double ownership.
    if(parent())
    {
        if(parent()->hasRoot())
        {
            DENG2_ASSERT(!parent()->root().isInTree(*child));
        }
        else
        {
            DENG2_ASSERT(!parent()->isInTree(*child));
        }
    }
    else
    {
        DENG2_ASSERT(!isInTree(*child));
    }
#endif

    child->d->parent = this;
    d->children.append(child);

    // Update index.
    if(!child->name().isEmpty())
    {
        d->index.insert(child->name(), child);
    }

    // Notify.
    DENG2_FOR_AUDIENCE2(ChildAddition, i)
    {
        i->widgetChildAdded(*child);
    }
    DENG2_FOR_EACH_OBSERVER(ParentChangeAudience, i, child->audienceForParentChange())
    {
        i->widgetParentChanged(*child, 0, this);
    }

    return *child;
}

Widget &Widget::insertBefore(Widget *child, Widget const &otherChild)
{    
    DENG2_ASSERT(child != &otherChild);

    add(child);
    moveChildBefore(child, otherChild);
    return *child;
}

Widget *Widget::remove(Widget &child)
{
    DENG2_ASSERT(child.d->parent == this);
    DENG2_ASSERT(d->children.contains(&child));

    child.d->parent = 0;
    d->children.removeOne(&child);

    DENG2_ASSERT(!d->children.contains(&child));

    if(!child.name().isEmpty())
    {
        d->index.remove(child.name());
    }

    // Notify.
    DENG2_FOR_AUDIENCE2(ChildRemoval, i)
    {
        i->widgetChildRemoved(child);
    }
    DENG2_FOR_EACH_OBSERVER(ParentChangeAudience, i, child.audienceForParentChange())
    {
        i->widgetParentChanged(child, this, 0);
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
    DENG2_FOR_EACH_CONST(Instance::Children, i, d->children)
    {
        Widget *w = (*i)->find(name);
        if(w) return w;
    }

    return 0;
}

bool Widget::isInTree(Widget const &child) const
{
    if(this == &child) return true;

    DENG2_FOR_EACH_CONST(Instance::Children, i, d->children)
    {
        if((*i)->isInTree(child))
        {
            return true;
        }
    }
    return false;
}

Widget const *Widget::find(String const &name) const
{
    return const_cast<Widget *>(this)->find(name);
}

void Widget::moveChildBefore(Widget *child, Widget const &otherChild)
{
    if(child == &otherChild) return; // invalid

    int from = -1;
    int to = -1;

    // Note: O(n)
    for(int i = 0; i < d->children.size() && (from < 0 || to < 0); ++i)
    {
        if(d->children.at(i) == child)
        {
            from = i;
        }
        if(d->children.at(i) == &otherChild)
        {
            to = i;
        }
    }

    DENG2_ASSERT(from != -1);
    DENG2_ASSERT(to != -1);

    d->children.removeAt(from);
    if(to > from) to--;

    d->children.insert(to, child);
}

void Widget::moveChildToLast(Widget &child)
{
    DENG2_ASSERT(child.parent() == this);
    if(!child.isLastChild())
    {
        remove(child);
        add(&child);
    }
}

Widget *Widget::parent() const
{
    return d->parent;
}

bool Widget::isFirstChild() const
{
    if(!parent()) return false;
    return this == parent()->d->children.first();
}

bool Widget::isLastChild() const
{
    if(!parent()) return false;
    return this == parent()->d->children.last();
}

String Widget::uniqueName(String const &name) const
{
    return String("#%1.%2").arg(id().asInt64()).arg(name);
}

Widget::NotifyArgs::Result Widget::notifyTree(NotifyArgs const &args)
{
    NotifyArgs::Result result = NotifyArgs::Continue;
    bool preNotified = false;

    for(int idx = 0; idx < d->children.size(); ++idx)
    {
        Widget *i = d->children.at(idx);

        if(i == args.until)
        {
            result = NotifyArgs::Abort;
            break;
        }

        if(args.conditionFunc && !(i->*args.conditionFunc)())
            continue; // Skip this one.

        if(args.preNotifyFunc && !preNotified)
        {
            preNotified = true;
            (this->*args.preNotifyFunc)();
        }

        (i->*args.notifyFunc)();

        if(i != d->children.at(idx))
        {
            // The list of children was modified; let's update the current
            // index accordingly.
            int newIdx = d->children.indexOf(i);

            // The current widget remains in the tree.
            if(newIdx >= 0)
            {
                idx = newIdx;
                i = d->children.at(newIdx);
            }
            else
            {
                // The current widget is gone. Continue with the same index.
                idx--;
                continue;
            }
        }

        // Continue down the tree by notifying any children of this widget.
        if(i->childCount())
        {
            if(i->notifyTree(args) == NotifyArgs::Abort)
            {
                result = NotifyArgs::Abort;
                break;
            }
        }
    }

    if(args.postNotifyFunc && preNotified)
    {
        (this->*args.postNotifyFunc)();
    }

    return result;
}

Widget::NotifyArgs::Result Widget::notifySelfAndTree(NotifyArgs const &args)
{
    (this->*args.notifyFunc)();
    return notifyTree(args);
}

void Widget::notifyTreeReversed(NotifyArgs const &args)
{
    if(args.preNotifyFunc)
    {
        (this->*args.preNotifyFunc)();
    }

    for(int i = d->children.size() - 1; i >= 0; --i)
    {
        Widget *w = d->children.at(i);

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
    if(isHidden() || d->behavior.testFlag(DisableEventDispatch)) return false;

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

    if(!d->behavior.testFlag(DisableEventDispatchToChildren))
    {
        // Tree is traversed in reverse order so that the visibly topmost
        // widgets get events first.
        for(int i = d->children.size() - 1; i >= 0; --i)
        {
            Widget *w = d->children.at(i);
            bool eaten = w->dispatchEvent(event, memberFunc);
            if(eaten) return true;
        }
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

dsize de::Widget::childCount() const
{
    return d->children.size();
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
