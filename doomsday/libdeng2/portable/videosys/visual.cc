/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Visual"

using namespace de;

Visual::Visual() : parent_(0)
{}

Visual::~Visual()
{
    if(parent_)
    {
        parent_->remove(this);
    }
    clear();
}

void Visual::clear()
{
    for(Children::iterator i = children_.begin(); i != children_.end(); ++i)
    {
        (*i)->parent_ = 0;
        delete *i;
    }
    children_.clear();
}

Visual* Visual::add(Visual* visual)
{
    children_.push_back(visual);
    visual->parent_ = this;
    return visual;
}

Visual* Visual::remove(Visual* visual)
{
    visual->parent_ = 0;
    children_.remove(visual);
    return visual;
}

void Visual::update()
{}

void Visual::draw() const
{
    drawSelf(BEFORE_CHILDREN);
    for(Children::const_iterator i = children_.begin(); i != children_.end(); ++i)
    {
        (*i)->draw();
    }
    drawSelf(AFTER_CHILDREN);
}

void Visual::drawSelf(DrawingStage /*stage*/) const
{}
