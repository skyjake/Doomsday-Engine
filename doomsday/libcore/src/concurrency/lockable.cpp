/*
 * The Doomsday Engine Project -- libcore
 *
 * @authors Copyright © 2004-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/Lockable"
#include "de/error.h"

namespace de {

Lockable::Lockable()
    : _mutex(QMutex::Recursive)
#ifdef DENG2_DEBUG
    , _lockCount(0)
#endif
{}

Lockable::~Lockable()
{    
    DENG2_ASSERT(!isLocked()); // You should unlock before deleting!
}

void Lockable::lock() const
{
    _mutex.lock();

#ifdef DENG2_DEBUG
    _countMutex.lock();
    _lockCount++;
    _countMutex.unlock();
#endif
}

void Lockable::unlock() const
{
#ifdef DENG2_DEBUG
    _countMutex.lock();
    _lockCount--;
#endif

    // Release the lock.
    _mutex.unlock();
    
#ifdef DENG2_DEBUG
    DENG2_ASSERT(_lockCount >= 0);
    _countMutex.unlock();
#endif
}

#ifdef DENG2_DEBUG
bool Lockable::isLocked() const
{
    _countMutex.lock();
    bool result = (_lockCount > 0);
    _countMutex.unlock();
    return result;
}
#endif

} // namespace de
