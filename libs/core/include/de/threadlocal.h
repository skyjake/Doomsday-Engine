/** @file threadlocal.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_THREADLOCAL_H
#define LIBCORE_THREADLOCAL_H

#include <the_Foundation/stdthreads.h>
#include "de/libcore.h"

namespace de {

/**
 * Thread-local object.
 *
 * An object of type T gets created for each thread on which ThreadLocal::get() is called.
 * A thread-local object is deleted when the thread exits.
 */
template <typename T>
class ThreadLocal
{
public:
    ThreadLocal()
    {
        zap(_tsKey);
    }

    T &get()
    {
        if (!_tsKey)
        {
            tss_create(&_tsKey, deleter);
        }
        T *obj = static_cast<T *>(tss_get(_tsKey));
        if (!obj)
        {
            tss_set(_tsKey, obj = new T);
        }
        return *obj;
    }

    static void deleter(void *obj)
    {
        if (obj) delete static_cast<T *>(obj);
    }

private:
    tss_t _tsKey;
};

} // namespace de

#endif // LIBCORE_THREADLOCAL_H
