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

#ifndef LIBCORE_WAITABLEFIFO_H
#define LIBCORE_WAITABLEFIFO_H

#include "de/fifo.h"
#include "de/waitable.h"

namespace de {

/**
 * FIFO with a semaphore that allows threads to wait until there are objects in
 * the buffer.
 *
 * @ingroup data
 */
template <typename Type>
class WaitableFIFO : public FIFO<Type>, public Waitable
{
public:
    WaitableFIFO() {}

    void put(Type *object, typename FIFO<Type>::PutMode mode = FIFO<Type>::PutHead) {
        FIFO<Type>::put(object, mode);
        post();
    }

    Type *take(TimeSpan timeOut = 0.0) {
        wait(timeOut);
        return FIFO<Type>::take();
    }

    Type *tryTake(TimeSpan timeOut = 0.0) {
        if (tryWait(timeOut)) {
            return FIFO<Type>::take();
        }
        return nullptr;
    }
};

} // namespace de

#endif // LIBCORE_WAITABLEFIFO_H
