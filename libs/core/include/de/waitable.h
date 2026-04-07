/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_WAITABLE_H
#define LIBCORE_WAITABLE_H

#include "de/libcore.h"
#include "de/time.h"

namespace de {

/**
 * Semaphore that allows objects to be waited on.
 *
 * @ingroup concurrency
 */
class DE_PUBLIC Waitable
{
public:
    /// wait() failed due to timing out before the resource is secured. @ingroup errors
    DE_ERROR(TimeOutError);

public:
    Waitable(dint initialValue = 0);

    /// Resets the semaphore to zero.
    //void reset();

    /// Wait until the resource becomes available. Waits indefinitely.
    void wait() const;

    /// Wait for the specified period of time to secure the
    /// resource.  If timeout occurs, an exception is thrown.
    void wait(TimeSpan timeOut) const;

    /// Wait for the specified period of time to secure the
    /// resource.  If timeout occurs, returns @c false.
    bool tryWait(TimeSpan timeOut) const;

    /// Mark the resource as available by incrementing the
    /// semaphore value.
    void post() const;

private:
    DE_PRIVATE(d)
};

}

#endif /* LIBCORE_WAITABLE_H */
