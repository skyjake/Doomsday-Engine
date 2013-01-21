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

struct Widget::Instance
{
    Widget &self;
    String name;
    Widget *parent;

    typedef QList<Widget *> Children;
    typedef QMap<String, Widget *> NamedChildren;
    Children children;
    NamedChildren index;

    Instance(Widget &w, String const &n) : self(w), name(n), parent(0)
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

Widget::Widget(String const &name) : d(new Instance(*this, name))
{}

Widget::~Widget()
{
    // Remove from parent automatically.
    if(d->parent)
    {
        d->parent->remove(*this);
    }

    delete d;
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

void Widget::notifyTree(void (Widget::*notifyFunc)())
{
    DENG2_FOR_EACH(Instance::Children, i, d->children)
    {
        ((*i)->*notifyFunc)();
        (*i)->notifyTree(notifyFunc);
    }
}

void Widget::notifyTreeReversed(void (Widget::*notifyFunc)())
{
    for(int i = d->children.size() - 1; i >= 0; --i)
    {
        Widget *w = d->children[i];
        w->notifyTreeReversed(notifyFunc);
        (w->*notifyFunc)();
    }
}

bool Widget::dispatchEvent(Event const *event, bool (Widget::*memberFunc)(Event const *))
{
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

QList<Widget *> Widget::children() const
{
    return d->children;
}

void Widget::initialize()
{}

void Widget::viewResized()
{}

void Widget::update()
{}

void Widget::draw()
{}

bool Widget::handleEvent(const Event *)
{
    // Event is not handled.
    return false;
}

} // namespace de
