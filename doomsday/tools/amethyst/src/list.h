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

#ifndef __AMETHYST_LIST_H__
#define __AMETHYST_LIST_H__

#include "linkable.h"

template< class T >
class List : public Linkable
{
public:
    List(T *initializer = 0);
    List(const List &otherRoot); // Copy constructor.

    List *next() { return (List*) Linkable::next(); }
    List *prev() { return (List*) Linkable::prev(); }

    virtual T *add(T *ptr)
    {   
        addLast(new List(ptr)); // Also makes this a root node.
        return ptr;
    }
    T *get() { return _pointer; };

protected:
    T* _pointer;
};

template< class T >
List<T>::List(T *initializer)
{
    _pointer = initializer;
}

template< class T >
List<T>::List(const List &otherRoot)
{
    _pointer = otherRoot._pointer;
    // We must make a copy of all the nodes in the other's list.
    for(List *n = otherRoot.next(); !n->IsRoot(); n = n->next())
        add(n->get());
}

#endif
