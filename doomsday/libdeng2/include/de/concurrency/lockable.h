/*
 * The Doomsday Engine Project -- libdeng2
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

#ifndef LIBDENG2_LOCKABLE_H
#define LIBDENG2_LOCKABLE_H

#include "../libdeng2.h"

#include <QMutex>

namespace de {

/**
 * A mutex that can be used to synchronize access to a resource.  All classes
 * of lockable resources should be derived from this class. The mutex works
 * in a recursive way: if lock() is called multiple times, unlock() must be
 * called as many times.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Lockable
{
public:
    Lockable();
    virtual ~Lockable();

    /// Acquire the lock.  Blocks until the operation succeeds.
    void lock() const;

    /// Release the lock.
    void unlock() const;

    /// Returns true, if the lock is currently locked.
    bool isLocked() const;

private:
    mutable QMutex _mutex;

    mutable int _lockCount;
    mutable QMutex _countMutex;
};

} // namespace de

#endif // LIBDENG2_LOCKABLE_H
