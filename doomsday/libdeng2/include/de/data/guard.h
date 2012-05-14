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

#ifndef LIBDENG2_GUARD_H
#define LIBDENG2_GUARD_H

#include "../libdeng2.h"

namespace de {

/**
 * Locks the variable @a varName until the end of the current scope.
 * assertLocked() is called so the compiler does not complain about the unused
 * variable.
 *
 * @param varName  Name of the variable to guard. Must be just a single
 *                 identifier with no operators or anything else.
 */
#define DENG2_GUARD(varName)    Guard _guarding_##varName(varName); _guarding_##varName.assertLocked();

class Lockable;

/**
 * Utility for locking a Lockable object for the lifetime of the Guard.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Guard
{
public:
    /**
     * The target object is locked.
     */
    Guard(const Lockable& target);

    /**
     * The target object is locked.
     */
    Guard(const Lockable* target);

    /**
     * The target object is unlocked.
     */
    ~Guard();

    void assertLocked() const;

private:
    const Lockable* _target;
};

} // namespace de

#endif // LIBDENG2_GUARD_H
