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

#include "shard.h"
#include "block.h"
#include "source.h"

Shard::Shard(ShardType t, Source *src)
{ 
    _type = t;
    _parent = _first = _last = _next = _prev = 0;
    _source = src;
    if(src) _lineNumber = src->lineNumber();
}

Shard::~Shard()
{
    clear();    // Delete all children.
}

/**
 * Deletes all children.
 */
void Shard::clear()
{
    while(_first) delete remove(_first);
}

/**
 * Makes the shard the last child of this one.
 */
Shard *Shard::add(Shard *s)
{
    s->_parent = this;
    
    // Are there any previous children?
    if(!_first && !_last)
    {
        _first = _last = s;
        s->_next = s->_prev = 0;
    }
    else
    {
        _last->_next = s;
        s->_prev = _last;
        s->_next = 0;
        _last = s;
    }
    return s;
}

/**
 * Unlinks the child s.
 */
Shard *Shard::remove(Shard *s)
{
    if(s->_prev) s->_prev->_next = s->_next; else _first = s->_next;
    if(s->_next) s->_next->_prev = s->_prev; else _last = s->_prev;
    s->_parent = s->_next = s->_prev = 0;
    return s;
}

int Shard::count()
{
    int count = 0;
    for(Shard *it = _first; it; it = it->_next) count++;
    return count;
}

/**
 * Returns a zero-based order number for this shard.
 */
int Shard::order()
{
    int count = 0;
    if(!_parent) return 0;
    for(Shard *it = _parent->_first; it != this; it = it->_next, count++) {}
    return count;
}

bool Shard::isIdentical(Shard *other)
{
    if(_type != other->_type) return false;
    if(count() != other->count()) return false;
    for(Shard *mine = _first, *yours = other->_first; mine;
        mine = mine->_next, yours = yours->_next)
        if(!mine->isIdentical(yours)) return false;
    return true;
}

bool Shard::operator == (Shard &other)
{
    return isIdentical(&other);
}

/**
 * Negative index => reverse search.
 */
Shard *Shard::child(int oneBasedIndex)
{
    bool fwd = oneBasedIndex > 0;
    int count = 1;

    if(!oneBasedIndex) return NULL; // Zero is not defined.
    oneBasedIndex = qAbs(oneBasedIndex);
    Shard *it = fwd? _first : _last;
    for(; it && count != oneBasedIndex; it = fwd? it->_next : it->_prev, count++) {}
    return it;
}

void Shard::steal(Shard *whoseChildren)
{
    Shard *fol;
    for(Shard *it = whoseChildren->_first; it; it = fol)
    {
        fol = it->_next;
        add(whoseChildren->remove(it));
    }
}

Shard *Shard::top()
{
    Shard *top = this;
    while(top->parent()) top = top->parent();
    return top;
}

Shard *Shard::following()
{
    if(_first) return _first;
    if(_next) return _next;
    Shard *it = this;
    while(it->_parent)
    {
        it = it->_parent;
        if(it->_next) return it->_next;
    }
    return NULL;
}

Shard *Shard::preceding()
{
    if(!_prev) return _parent;
    // Go to the last possible child.
    return _prev->final();
}

Shard *Shard::final()
{
    Shard *it = this;
    while(it->_last) it = it->_last;
    return it;
}
