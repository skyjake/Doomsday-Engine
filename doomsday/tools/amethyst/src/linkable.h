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

#ifndef __AMETHYST_LINKABLE_H__
#define __AMETHYST_LINKABLE_H__

class Linkable
{
public:
    Linkable(bool makeRoot = false);
    virtual ~Linkable();

    void destroy();

    Linkable *addAfter(Linkable *nodeToAdd);
    Linkable *addBefore(Linkable *nodeToAdd);
    Linkable *addFirst(Linkable *nodeToAdd) { setRoot(); return addAfter(nodeToAdd); }
    Linkable *addLast(Linkable *nodeToAdd) { setRoot(); return addBefore(nodeToAdd); }
    Linkable *remove();

    void setRoot(bool makeRoot = true) { _isRootNode = makeRoot; }
    bool isRoot() { return _isRootNode; }
    bool isListEmpty() { return _next == this; }
    int count();
    Linkable *next() { return _next; }
    Linkable *prev() { return _prev; }

protected:
    bool        _isRootNode;
    Linkable    *_next, *_prev;
};

#endif
