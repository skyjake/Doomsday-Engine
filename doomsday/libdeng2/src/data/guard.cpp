/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

Guard::Guard(de::Lockable const &target) : _target(&target)
{
    _target->lock();
}

Guard::Guard(de::Lockable const *target) : _target(target)
{
    DENG2_ASSERT(target != 0);
    _target->lock();
}

Guard::~Guard()
{
    _target->unlock();
}

void Guard::assertLocked() const
{
    _target->assertLocked();
}
