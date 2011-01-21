/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "visual.h"
#include "rule.h"
#include <QDebug>

using namespace de;

Visual::Visual(Visual* p) : QObject(), _parent(0), _rect(0)
{
    if(p)
    {
        p->add(this);
    }
}

Visual::~Visual()
{
    //qDebug() << this << "destroyed.";

    if(_parent)
    {
        _parent->remove(this);
    }
    clear();
}

void Visual::clear()
{
    for(Children::iterator i = _children.begin(); i != _children.end(); ++i)
    {
        (*i)->_parent = 0;
        delete *i;
    }
    _children.clear();
}

Visual* Visual::add(Visual* visual)
{
    _children.append(visual);
    visual->_parent = this;
    return visual;
}

Visual* Visual::remove(Visual* visual)
{
    visual->_parent = 0;
    _children.removeOne(visual);
    return visual;
}

void Visual::setRect(RectangleRule* rule)
{
    Q_ASSERT(_rect == 0 /* Changing the rule is not supported. */);

    _rect = rule;
}

void Visual::checkRect()
{
    if(!_rect)
    {
        // Must have a parent.
        Q_ASSERT(_parent != 0);

        // No rect has been defined yet, so make a default one.
        // Just use the parent's rectangle.
        _rect = new RectangleRule(_parent->rule(), this);
    }
}

RectangleRule* Visual::rule()
{
    checkRect();
    return _rect;
}

const RectangleRule* Visual::rule() const
{
    const_cast<Visual*>(this)->checkRect();
    return _rect;
}

de::Rectanglef Visual::rect() const
{
    const_cast<Visual*>(this)->checkRect();
    return _rect->rect();
}

void Visual::draw() const
{
    drawSelf(BeforeChildren);
    for(Children::const_iterator i = _children.begin(); i != _children.end(); ++i)
    {
        (*i)->draw();
    }
    drawSelf(AfterChildren);
}

void Visual::drawSelf(DrawingStage /*stage*/) const
{}
