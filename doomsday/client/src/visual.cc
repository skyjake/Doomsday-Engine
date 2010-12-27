/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

Visual::Visual() : _parent(0)
{}

Visual::~Visual()
{
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
    _children.push_back(visual);
    visual->_parent = this;
    return visual;
}

Visual* Visual::remove(Visual* visual)
{
    visual->_parent = 0;
    _children.remove(visual);
    return visual;
}

void Visual::update()
{}

void Visual::draw() const
{
    drawSelf(BEFORE_CHILDREN);
    for(Children::const_iterator i = _children.begin(); i != _children.end(); ++i)
    {
        (*i)->draw();
    }
    drawSelf(AFTER_CHILDREN);
}

void Visual::drawSelf(DrawingStage /*stage*/) const
{}
