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

Counted::Counted() : _refCount(1)
{
#ifdef DENG2_DEBUG
    totalCount++;
#endif
}

Counted::~Counted()
{
#ifdef DENG2_DEBUG
    totalCount--;
#endif
    DENG2_ASSERT(_refCount == 0);
}

void Counted::release() const
{
    //qDebug() << "Counted" << this << typeid(*this).name() << "ref dec'd to" << _refCount-1;

    DENG2_ASSERT(_refCount > 0);
    if(!--_refCount)
    {
        delete this;
    }
}

} // namespace de
