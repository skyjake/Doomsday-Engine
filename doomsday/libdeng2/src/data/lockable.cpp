/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Lockable"
#include "de/error.h"

using namespace de;

#define LOCK_TIMEOUT_MS     60000

Lockable::Lockable() : _mutex(QMutex::Recursive), _lockCount(0)
{}
        
Lockable::~Lockable()
{
    while(_lockCount > 0)
    {
        unlock();
    }
}

void Lockable::lock() const
{
    // Acquire the lock.  Blocks until the operation succeeds.
    if(!_mutex.tryLock(LOCK_TIMEOUT_MS))
    {
        /// @throw Error Acquiring the mutex failed due to an error.
        throw Error("Lockable::lock", "Failed to lock");
    }

    _lockCount++;
}

void Lockable::unlock() const
{
    if(_lockCount > 0)
    {
        _lockCount--;

        // Release the lock.
        _mutex.unlock();
    }
}

bool Lockable::isLocked() const
{
    return _lockCount > 0;
}

void Lockable::assertLocked() const
{
    DENG2_ASSERT(isLocked());
}
