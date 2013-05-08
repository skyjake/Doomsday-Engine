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
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_LOCKABLE_H
