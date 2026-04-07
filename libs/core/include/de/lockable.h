/*
 * The Doomsday Engine Project -- libcore
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_LOCKABLE_H
#define LIBCORE_LOCKABLE_H

#include "de/libcore.h"

#include <mutex>

namespace de {

/**
 * A mutex that can be used to synchronize access to a resource.  All classes
 * of lockable resources should be derived from this class. The mutex works
 * in a recursive way: if lock() is called multiple times, unlock() must be
 * called as many times.
 *
 * @ingroup concurrency
 */
class DE_PUBLIC Lockable
{
public:
    /// Acquire the lock.  Blocks until the operation succeeds.
    /// @return @c true, if the lock succeeded. @c false, if there was an error.
    bool lock() const noexcept;

    /// Release the lock.
    inline void unlock() const {
        _mutex.unlock();
    }

private:
    mutable std::recursive_mutex _mutex;
};

template <typename Type>
struct LockableT : public Lockable
{
    typedef Type ValueType;
    Type value;

    LockableT() {}
    LockableT(const Type &initial) : value(initial) {}
    LockableT(Type &&initial) : value(initial) {}

    operator Type &() { return value; }
    operator const Type &() const { return value; }
};

} // namespace de

#endif // LIBCORE_LOCKABLE_H
