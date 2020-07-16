/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_GUARD_H
#define LIBCORE_GUARD_H

#include "de/libcore.h"
#include "de/lockable.h"
//#include "de/readwritelockable.h"

namespace de {

/**
 * Locks the variable @a varName until the end of the current scope.
 *
 * @param varName  Name of the variable to guard. Must be just a single
 *                 identifier with no operators or anything else.
 */
#define DE_GUARD(varName) \
    de::Guard _guarding_##varName(varName); \
    DE_UNUSED(_guarding_##varName);

#if 0
#define DE_GUARD_READ(varName) \
    de::Guard _guarding_##varName(varName, de::Guard::Reading); \
    DE_UNUSED(_guarding_##varName);

#define DE_GUARD_WRITE(varName) \
    de::Guard _guarding_##varName(varName, de::Guard::Writing); \
    DE_UNUSED(_guarding_##varName);
#endif

/**
 * Locks the target @a targetName until the end of the current scope.
 *
 * @param targetName  Target to be guarded.
 * @param varName     Name of the variable to guard. Must be just a single
 *                    identifier with no operators or anything else.
 */
#define DE_GUARD_FOR(targetName, varName) \
    de::Guard varName(targetName); \
    DE_UNUSED(varName);

#if 0
#define DE_GUARD_READ_FOR(targetName, varName) \
    de::Guard varName(targetName, de::Guard::Reading); \
    DE_UNUSED(varName);

#define DE_GUARD_WRITE_FOR(targetName, varName) \
    de::Guard varName(targetName, de::Guard::Writing); \
    DE_UNUSED(varName);
#endif

class Lockable;
//class ReadWriteLockable;

/**
 * Utility for locking a Lockable or ReadWriteLockable object for the lifetime
 * of the Guard. Note that using this is preferable to manual locking and
 * unlocking: if an exception occurs while the target is locked, unlocking will
 * be taken care of automatically when the Guard goes out of scope.
 *
 * @ingroup concurrency
 */
class DE_PUBLIC Guard
{
public:
//    enum LockMode { Reading, Writing };

public:
    /**
     * The target object is locked.
     */
    inline Guard(const Lockable &target)
        : _target(&target)
//        , _rwTarget(nullptr)
    {
        _target->lock();
    }

    /**
     * The target object is locked.
     */
    inline Guard(const Lockable *target)
        : _target(target)
//        , _rwTarget(nullptr)
    {
        DE_ASSERT(target != nullptr);
        _target->lock();
    }

#if 0
    inline Guard(const ReadWriteLockable &target, LockMode mode) : _target(nullptr), _rwTarget(&target) {
        if (mode == Reading) {
            _rwTarget->lockForRead();
        }
        else {
            _rwTarget->lockForWrite();
        }
    }

    inline Guard(const ReadWriteLockable *target, LockMode mode) : _target(nullptr), _rwTarget(target) {
        DE_ASSERT(_rwTarget != nullptr);
        if (mode == Reading) {
            _rwTarget->lockForRead();
        }
        else {
            _rwTarget->lockForWrite();
        }
    }
#endif

    /**
     * The target object is unlocked.
     */
    inline ~Guard() {
        if (_target) _target->unlock();
//        if (_rwTarget) _rwTarget->unlock();
    }

private:
    const Lockable *_target;
//    const ReadWriteLockable *_rwTarget;
};

} // namespace de

#endif // LIBCORE_GUARD_H
