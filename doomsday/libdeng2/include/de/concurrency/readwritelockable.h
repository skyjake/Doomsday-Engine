/** @file readwritelockable.h  Read-write lock.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_READWRITELOCKABLE_H
#define LIBDENG2_READWRITELOCKABLE_H

#include "../libdeng2.h"

namespace de {

/**
 * Read-write lock.
 */
class DENG2_PUBLIC ReadWriteLockable
{
public:
    ReadWriteLockable();
    virtual ~ReadWriteLockable();

    /// Acquire the lock for reading.  Blocks until the operation succeeds.
    void lockForRead() const;

    /// Acquire the lock for writing.  Blocks until the operation succeeds.
    void lockForWrite() const;

    /// Release the lock.
    void unlock() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_READWRITELOCKABLE_H
