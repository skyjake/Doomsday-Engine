/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Counted"

namespace de {

#ifdef DENG2_DEBUG
int Counted::totalCount = 0; ///< Should return back to zero when program ends.
#endif

Counted::Counted() : _refCount(1), _delegate(0)
{
#ifdef DENG2_DEBUG
    totalCount++;
#endif
}

void Counted::setDelegate(Counted const *delegate)
{
    DENG2_ASSERT(_delegate == 0);
    DENG2_ASSERT(_refCount == 1);

    _delegate = delegate;
    _refCount = 0; // won't be released any more
}

Counted::~Counted()
{
#ifdef DENG2_DEBUG
    totalCount--;
#endif
    //qDebug() << "~Counted" << this << typeid(*this).name() << "refCount:" << _refCount;

    DENG2_ASSERT(!_delegate || _delegate->_refCount == 0);
    DENG2_ASSERT(_refCount == 0);
}

void Counted::release() const
{
    Counted const *c = (!_delegate? this : _delegate);

    //qDebug() << "Counted" << c << typeid(*c).name() << "ref dec'd to" << c->_refCount - 1;

    DENG2_ASSERT(c->_refCount > 0);
    if(!--c->_refCount)
    {
        delete c;
    }
}

void Counted::addRef(dint count) const
{
    Counted const *c = (!_delegate? this : _delegate);

    DENG2_ASSERT(c->_refCount >= 0);
    c->_refCount += count;
    DENG2_ASSERT(c->_refCount >= 0);
}

} // namespace de
