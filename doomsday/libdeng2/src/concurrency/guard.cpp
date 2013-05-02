/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Guard"
#include "de/Lockable"
#include "de/ReadWriteLockable"

namespace de {

Guard::Guard(de::Lockable const &target) : _target(&target), _rwTarget(0)
{
    _target->lock();
}

Guard::Guard(de::Lockable const *target) : _target(target), _rwTarget(0)
{
    DENG2_ASSERT(target != 0);

    _target->lock();
}

Guard::Guard(ReadWriteLockable const &target, LockMode mode) : _target(0), _rwTarget(&target)
{
    if(mode == Reading)
        _rwTarget->lockForRead();
    else
        _rwTarget->lockForWrite();
}

Guard::Guard(ReadWriteLockable const *target, LockMode mode) : _target(0), _rwTarget(target)
{
    DENG2_ASSERT(_rwTarget != 0);

    if(mode == Reading)
        _rwTarget->lockForRead();
    else
        _rwTarget->lockForWrite();
}

Guard::~Guard()
{
    if(_target)   _target->unlock();
    if(_rwTarget) _rwTarget->unlock();
}

} // namespace de
