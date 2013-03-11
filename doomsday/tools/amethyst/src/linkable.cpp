/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "linkable.h"

Linkable::Linkable(bool makeRoot)
{
    // By default the node is linked to itself.
    _next = _prev = this;
    _isRootNode = makeRoot;
}

Linkable::~Linkable()
{
    // The root is responsible for destroying the ring.
    if(_isRootNode) destroy();
}

void Linkable::destroy()
{
    while(_next != this) delete _next->remove();
}

Linkable *Linkable::addAfter(Linkable *nodeToAdd)
{
    nodeToAdd->_prev = this;
    nodeToAdd->_next = _next;
    return _next = nodeToAdd->_next->_prev = nodeToAdd;
}

Linkable *Linkable::addBefore(Linkable *nodeToAdd)
{
    nodeToAdd->_next = this;
    nodeToAdd->_prev = _prev;
    return _prev = nodeToAdd->_prev->_next = nodeToAdd;
}

Linkable *Linkable::remove()
{
    _next->_prev = _prev;
    _prev->_next = _next;
    return this;
}

int Linkable::count()
{
    int count = 0;
    for(Linkable *it = _next; !it->_isRootNode; it = it->_next, count++) {}
    return count;
}
