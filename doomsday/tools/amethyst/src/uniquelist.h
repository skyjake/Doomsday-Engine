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

#ifndef __AMETHYST_UNIQUE_LIST_H__
#define __AMETHYST_UNIQUE_LIST_H__

#include "list.h"

template<class T>
class UniqueList : public List<T>
{
public:
    UniqueList(T *ptr = 0) : List<T>(ptr) { }
    UniqueList(const List<T> &other) : List<T>(other) { }

    UniqueList *next() { return (UniqueList*) Linkable::next(); }
    UniqueList *prev() { return (UniqueList*) Linkable::prev(); }

    T *add(T *ptr);
};

/**
 * Call this for the root! The new node is added to the end of the list,
 * i.e. just before the root.
 */
template<class T>
T *UniqueList<T>::add(T *ptr)
{
    // Check if this pointer already exists.
    for(UniqueList *u = next(); !u->isRoot(); u = u->next())
        if(u->get() == ptr) return ptr; // Already there.
    return List<T>::add(ptr);
}

typedef UniqueList<void> UniquePtrList;

#endif
