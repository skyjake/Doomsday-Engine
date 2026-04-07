/** @file widget.cpp Base class for widgets.
 * @ingroup widget
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

#include "de/widget.h"
#include "de/rootwidget.h"
#include "de/asset.h"
#include "de/logbuffer.h"
#include "de/list.h"
#include "de/keymap.h"

namespace de {

DE_PIMPL(Widget)
{
    Id id;
    String name;
    Widget *parent = nullptr;
    std::unique_ptr<Record> names; // IObject
    RootWidget *manualRoot = nullptr;
    Behaviors behavior;
    String focusNext;
    String focusPrev;

    typedef KeyMap<int, Widget *> Routing;
    Routing routing;

    typedef WidgetList Children;
    typedef KeyMap<String, Widget *> NamedChildren;
    Children children;
    NamedChildren index;

    Impl(Public *i, const String &n) : Base(i), name(n)
    {}

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        while (!children.isEmpty())
        {
            children.first()->d->parent = nullptr;
            Widget *w = children.takeFirst();
            //qDebug() << "deleting" << w << w->name();
            delete w;
        }
        index.clear();
    }

    RootWidget *findRoot() const
    {
        if (manualRoot)
        {
            return manualRoot;
        }
        const Widget *w = thisPublic;
        while (w->parent())
        {
            w = w->parent();
            if (w->d->manualRoot) return w->d->manualRoot;
        }
        if (is<RootWidget>(w))
        {
            return const_cast<RootWidget *>(&w->as<RootWidget>());
        }
        return nullptr;
    }

    enum AddBehavior { Append, Prepend, InsertBefore };

    void add(Widget *child, AddBehavior behavior, const Widget *ref = nullptr)
    {
        DE_ASSERT(child != nullptr);
        DE_ASSERT(child->d->parent == nullptr);

#ifdef _DEBUG
        // Can't have double ownership.
        if (self().parent())
        {
            if (self().parent()->hasRoot())
            {
                DE_ASSERT(!self().parent()->root().isInTree(*child));
            }
            else
            {
                DE_ASSERT(!self().parent()->isInTree(*child));
            }
        }
        else
        {
            DE_ASSERT(!self().isInTree(*child));
        }
#endif

        child->d->parent = thisPublic;

        switch (behavior)
        {
        case Append:
            children.append(child);
            break;

        case Prepend:
            children.push_front(child);
            break;

        case InsertBefore: {
            auto ip = std::find(children.begin(), children.end(), ref);
            children.insert(ip, child);
//            children.insert(children.indexOf(const_cast<Widget *>(ref)), child);
            break; }
        }

        // Update index.
        if (!child->name().isEmpty())
        {
            index.insert(child->name(), child);
        }

        // Notify.
        DE_NOTIFY_PUBLIC(ChildAddition, i)
        {
            i->widgetChildAdded(*child);
        }
        DE_FOR_OBSERVERS(i, child->audienceForParentChange())
        {
            i->widgetParentChanged(*child, nullptr, thisPublic);
        }
    }

    Widget *walkChildren(Widget *beginFrom,
                         WalkDirection dir,
                         const std::function<LoopResult (Widget &)>& func,
                         int verticalDir = 0)
    {
        bool first = true;

        // This gets a little complicated. The method handles both forward and backward
        // directions, and is called recursively when either descending into subtrees
        // or ascending to the parent level.

        for (int pos = children.indexOf(beginFrom); ;
             pos += (dir == Forward? +1 : -1), first = false)
        {
            // Skip the first widget when walking back up the tree.
            if (first && (verticalDir < 0 || (verticalDir == 0 && dir == Backward)))
            {
                continue;
            }

            // During the first round, we will skip the starting point widget but allow
            // descending into its children.
            const bool onlyDescend = (verticalDir == 0 && first);

            // When we run out of siblings, try to ascend to parent and continue from
            // there without handling the parent again.
            if (pos < 0 || pos >= children.sizei())
            {
                if (verticalDir > 0)
                {
                    // We were descending: the recursion should fall back to the
                    // previous parent.
                    return nullptr;
                }
                if (dir == Backward && func(self()))
                {
                    return thisPublic;
                }
                if (parent)
                {
                    return parent->d->walkChildren(thisPublic, dir, func, -1 /*up*/);
                }
                return nullptr;
            }

            Widget *i = children.at(pos);

            if (dir == Forward && !onlyDescend && func(*i))
            {
                return i;
            }

            // Descend into subtree.
            if (i->childCount() > 0)
            {
                Widget *starting = (dir == Forward? i->d->children.first()
                                                  : i->d->children.last());
                if (Widget *found = i->d->walkChildren(starting, dir, func, +1 /*down*/))
                {
                    return found;
                }
            }

            if (dir == Backward && !onlyDescend && func(*i))
            {
                return i;
            }
        }
    }

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(ParentChange)
    DE_PIMPL_AUDIENCE(ChildAddition)
    DE_PIMPL_AUDIENCE(ChildRemoval)
};

DE_AUDIENCE_METHOD(Widget, Deletion)
DE_AUDIENCE_METHOD(Widget, ParentChange)
DE_AUDIENCE_METHOD(Widget, ChildAddition)
DE_AUDIENCE_METHOD(Widget, ChildRemoval)

Widget::Widget(const String &name) : d(new Impl(this, name))
{}

Widget::~Widget()
{
    if (hasRoot() && root().focus() == this)
    {
        root().setFocus(nullptr);
    }

    audienceForParentChange().clear();

    // Remove from parent automatically.
    if (d->parent)
    {
        d->parent->remove(*this);
    }

    // Notify everyone else.
    DE_NOTIFY(Deletion, i)
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

void Widget::setName(const String &name)
{
    // Remove old name from parent's index.
    if (d->parent && !d->name.isEmpty())
    {
        d->parent->d->index.remove(d->name);
    }

    d->name = name;

    // Update parent's index with new name.
    if (d->parent && !name.isEmpty())
    {
        d->parent->d->index.insert(name, this);
    }
}

DotPath Widget::path() const
{
    const Widget *w = this;
    String result;
    while (w)
    {
        if (!result.isEmpty()) result = "." + result;
        if (!w->d->name.isEmpty())
        {
            result = w->d->name + result;
        }
        else
        {
            result = Stringf("0x%x", w) + result;
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
    if (auto *rw = d->findRoot())
    {
        return *rw;
    }
    throw NotFoundError("Widget::root", "No root widget found");
}

RootWidget *Widget::findRoot() const
{
    return d->findRoot();
}

void Widget::setRoot(RootWidget *root)
{
    d->manualRoot = root;
}

bool Widget::hasFocus() const
{
    return hasRoot() && root().focus() == this;
}

bool Widget::canBeFocused() const
{
    return behavior().testFlag(Focusable) && isVisible() && isEnabled();
}

bool Widget::hasFamilyBehavior(const Behavior &flags) const
{
    for (const Widget *w = this; w; w = w->d->parent)
    {
        if (w->d->behavior.testFlag(flags)) return true;
    }
    return false;
}

void Widget::show(bool doShow)
{
    setBehavior(Hidden, !doShow);
}

void Widget::setBehavior(const Behaviors& behavior, FlagOpArg operation)
{
    applyFlagOperation(d->behavior, behavior, operation);
}

void Widget::unsetBehavior(const Behaviors& behavior)
{
    applyFlagOperation(d->behavior, behavior, UnsetFlags);
}

Widget::Behaviors Widget::behavior() const
{
    return d->behavior;
}

void Widget::setFocusNext(const String &name)
{
    d->focusNext = name;
}

void Widget::setFocusPrev(const String &name)
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

void Widget::setEventRouting(const List<int> &types, Widget *routeTo)
{
    for (int type : types)
    {
        if (routeTo)
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
    return addLast(child);
}

Widget &Widget::addLast(Widget *child)
{
    d->add(child, Impl::Append);
    return *child;
}

Widget &Widget::addFirst(Widget *child)
{
    d->add(child, Impl::Prepend);
    return *child;
}

Widget &Widget::insertBefore(Widget *child, const Widget &otherChild)
{
    DE_ASSERT(child != &otherChild);
    DE_ASSERT(otherChild.parent() == this);

    d->add(child, Impl::InsertBefore, &otherChild);
    return *child;
}

Widget *Widget::remove(Widget &child)
{
    DE_ASSERT(child.d->parent == this);
    DE_ASSERT(d->children.contains(&child));

    child.d->parent = nullptr;
    d->children.removeOne(&child);

    DE_ASSERT(!d->children.contains(&child));

    if (!child.name().isEmpty())
    {
        d->index.remove(child.name());
    }

    // Notify.
    DE_NOTIFY(ChildRemoval, i)
    {
        i->widgetChildRemoved(child);
    }
    DE_FOR_OBSERVERS(i, child.audienceForParentChange())
    {
        i->widgetParentChanged(child, this, nullptr);
    }

    return &child;
}

void Widget::orphan()
{
    if (d->parent)
    {
        d->parent->remove(*this);
    }
    DE_ASSERT(d->parent == nullptr);
}

Widget *Widget::find(const String &name)
{
    if (d->name == name) return this;

    Impl::NamedChildren::const_iterator found = d->index.constFind(name);
    if (found != d->index.end())
    {
        return found->second;
    }

    // Descend recursively to child widgets.
    for (const auto &i : d->children)
    {
        Widget *w = i->find(name);
        if (w) return w;
    }

    return nullptr;
}

bool Widget::isInTree(const Widget &child) const
{
    if (this == &child) return true;

    DE_FOR_EACH_CONST(Impl::Children, i, d->children)
    {
        if ((*i)->isInTree(child))
        {
            return true;
        }
    }
    return false;
}

bool Widget::hasAncestor(const Widget &ancestorOrParent) const
{
    for (const Widget *iter = parent(); iter; iter = iter->parent())
    {
        if (iter == &ancestorOrParent) return true;
    }
    return false;
}

const Widget *Widget::find(const String &name) const
{
    return const_cast<Widget *>(this)->find(name);
}

void Widget::moveChildBefore(Widget *child, const Widget &otherChild)
{
    if (child == &otherChild) return; // invalid

    int from = -1;
    int to = -1;

    // Note: O(n)
    for (int i = 0; i < d->children.sizei() && (from < 0 || to < 0); ++i)
    {
        if (d->children.at(i) == child)
        {
            from = i;
        }
        if (d->children.at(i) == &otherChild)
        {
            to = i;
        }
    }

    DE_ASSERT(from != -1);
    DE_ASSERT(to != -1);

    d->children.removeAt(from);
    if (to > from) to--;

    d->children.insert(to, child);
}

void Widget::moveChildToLast(Widget &child)
{
    DE_ASSERT(child.parent() == this);
    if (!child.isLastChild())
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
    if (!parent()) return false;
    return this == parent()->d->children.first();
}

bool Widget::isLastChild() const
{
    if (!parent()) return false;
    return this == parent()->d->children.last();
}

Widget *Widget::walkInOrder(WalkDirection dir, const std::function<LoopResult (Widget &)>& callback)
{
    if (!d->parent)
    {
        // This is the root.
        if (d->children.isEmpty()) return nullptr;
        if (dir == Forward)
        {
            return d->walkChildren(d->children.first(), Forward, callback, +1);
        }
        else
        {
            // There is no going back from the root.
            return nullptr;
        }
    }
    return d->parent->d->walkChildren(this, dir, callback); // allows ascending
}

Widget *Widget::walkChildren(WalkDirection dir, const std::function<LoopResult (Widget &)>& callback)
{
    if (d->children.isEmpty()) return nullptr;
    return d->walkChildren(dir == Forward? d->children.first() : d->children.last(),
                           dir, callback, +1);
}

String Widget::uniqueName(const String &name) const
{
    return Stringf("%s.%s", id().asText().c_str(), name.c_str());
}

Widget::NotifyArgs Widget::notifyArgsForDraw() const
{
    NotifyArgs args(&Widget::draw);
    args.conditionFunc  = &Widget::isVisible;
    args.preNotifyFunc  = &Widget::preDrawChildren;
    args.postNotifyFunc = &Widget::postDrawChildren;
    return args;
}

Widget::NotifyArgs::Result Widget::notifyTree(const NotifyArgs &args)
{
    NotifyArgs::Result result = NotifyArgs::Continue;
    bool preNotified = false;

    for (int idx = 0; idx < d->children.sizei(); ++idx)
    {
        Widget *i = d->children.at(idx);

        if (i == args.until)
        {
            result = NotifyArgs::Abort;
            break;
        }

        if (args.conditionFunc && !(i->*args.conditionFunc)())
            continue; // Skip this one.

        if (args.preNotifyFunc && !preNotified)
        {
            preNotified = true;
            (this->*args.preNotifyFunc)();
        }

        (i->*args.notifyFunc)();

        if (i != d->children.at(idx))
        {
            // The list of children was modified; let's update the current
            // index accordingly.
            int newIdx = d->children.indexOf(i);

            // The current widget remains in the tree.
            if (newIdx >= 0)
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
        if (i->childCount())
        {
            if (i->notifyTree(args) == NotifyArgs::Abort)
            {
                result = NotifyArgs::Abort;
                break;
            }
        }
    }

    if (args.postNotifyFunc && preNotified)
    {
        (this->*args.postNotifyFunc)();
    }

    return result;
}

Widget::NotifyArgs::Result Widget::notifySelfAndTree(const NotifyArgs &args)
{
    (this->*args.notifyFunc)();
    return notifyTree(args);
}

void Widget::notifyTreeReversed(const NotifyArgs &args)
{
    if (args.preNotifyFunc)
    {
        (this->*args.preNotifyFunc)();
    }

    for (int i = d->children.size() - 1; i >= 0; --i)
    {
        Widget *w = d->children.at(i);

        if (args.conditionFunc && !(w->*args.conditionFunc)())
            continue; // Skip this one.

        w->notifyTreeReversed(args);
        (w->*args.notifyFunc)();
    }

    if (args.postNotifyFunc)
    {
        (this->*args.postNotifyFunc)();
    }
}

bool Widget::dispatchEvent(const Event &event, bool (Widget::*memberFunc)(const Event &))
{
    // Hidden widgets do not get events.
    if (isHidden() || d->behavior.testFlag(DisableEventDispatch)) return false;

    // Routing has priority.
    if (d->routing.contains(event.type()))
    {
        return d->routing[event.type()]->dispatchEvent(event, memberFunc);
    }

    // Focus only affects key events.
    const bool thisHasFocus = (hasRoot() && root().focus() == this && event.isKey());

    if (d->behavior.testFlag(HandleEventsOnlyWhenFocused) && !thisHasFocus)
    {
        return false;
    }
    if (thisHasFocus)
    {
        // The focused widget is offered events before dispatching to the tree.
        return false;
    }

    if (!d->behavior.testFlag(DisableEventDispatchToChildren))
    {
        // Tree is traversed in reverse order so that the visibly topmost
        // widgets get events first.
        for (int i = d->children.size() - 1; i >= 0; --i)
        {
            Widget *w = d->children.at(i);
            bool eaten = w->dispatchEvent(event, memberFunc);
            if (eaten) return true;
        }
    }

    if ((this->*memberFunc)(event))
    {
        // Eaten.
        return true;
    }

    // Not eaten by anyone.
    return false;
}

void Widget::waitForAssetsReady()
{
    AssetGroup assets;
    collectUnreadyAssets(assets, CollectMode::OnlyVisible);
    if (!assets.isEmpty())
    {
        assets.waitForState(Asset::Ready);
    }
}

void Widget::collectUnreadyAssets(AssetGroup &collected, CollectMode collectMode)
{
    if (collectMode == CollectMode::OnlyVisible && behavior().testFlag(Hidden))
    {
        return;
    }
    if (auto *assetGroup = maybeAs<IAssetGroup>(this))
    {
        if (!assetGroup->assets().isReady())
        {
            collected += *assetGroup;
            LOGDEV_XVERBOSE("Found " _E(m) "NotReady" _E(.) " asset %s (%p)", path() << this);
        }
    }
    else
    {
        for (Widget *child : children())
        {
            child->collectUnreadyAssets(collected, collectMode);
        }
    }
}

Widget::Children Widget::children() const
{
    return d->children;
}

dsize de::Widget::childCount() const
{
    return dsize(d->children.size());
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

void Widget::offerFocus()
{}

void Widget::update()
{}

void Widget::draw()
{}

void Widget::preDrawChildren()
{}

void Widget::postDrawChildren()
{}

bool Widget::handleEvent(const Event &)
{
    // Event is not handled.
    return false;
}

Record &Widget::objectNamespace()
{
    if (!d->names)
    {
        d->names.reset(new Record);
    }
    return *d->names;
}

const Record &Widget::objectNamespace() const
{
    return const_cast<Widget *>(this)->objectNamespace();
}

void Widget::setFocusCycle(const WidgetList &order)
{
    for (dsize i = 0; i < order.size(); ++i)
    {
        Widget *a = order[i];
        Widget *b = order[(i + 1) % order.size()];

        a->setFocusNext(b->name());
        b->setFocusPrev(a->name());
    }
}

} // namespace de
