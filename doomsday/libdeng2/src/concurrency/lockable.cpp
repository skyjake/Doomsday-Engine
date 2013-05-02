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

#include "de/Lockable"
#include "de/error.h"

namespace de {

DENG2_PIMPL_NOREF(Lockable)
{
    mutable QMutex mutex;

    mutable int lockCount;
    mutable QMutex countMutex;

    Instance() : mutex(QMutex::Recursive), lockCount(0)
    {}
};

Lockable::Lockable() : d(new Instance)
{}

Lockable::~Lockable()
{    
    while(isLocked())
    {
        unlock();
    }
}

void Lockable::lock() const
{
    d->countMutex.lock();
    d->lockCount++;
    d->countMutex.unlock();

    d->mutex.lock();
}

void Lockable::unlock() const
{
    // Release the lock.
    d->mutex.unlock();

    d->countMutex.lock();
    d->lockCount--;
    d->countMutex.unlock();

    DENG2_ASSERT(d->lockCount >= 0);
}

bool Lockable::isLocked() const
{
    bool result;
    d->countMutex.lock();
    result = (d->lockCount > 0);
    d->countMutex.unlock();
    return result;
}

} // namespace de
