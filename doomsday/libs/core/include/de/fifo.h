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

#ifndef LIBCORE_FIFO_H
#define LIBCORE_FIFO_H

#include "de/lockable.h"
#include "de/guard.h"

#include <algorithm>
#include <functional>
#include <list>

namespace de {

/**
 * A template FIFO buffer that maintains pointers to objects. This is a
 * thread-safe implementation: lock() and unlock() are automatically called
 * when necessary.
 *
 * @ingroup data
 */
template <typename Type>
class FIFO : public Lockable
{
public:
    enum PutMode { PutHead, PutTail };

public:
    FIFO()
        : Lockable()
    {}

    virtual ~FIFO()
    {
        DE_GUARD(this);
        for (typename Objects::iterator i = _objects.begin(); i != _objects.end(); ++i)
        {
            delete *i;
        }
    }

    /**
     * Insert a new object to the buffer.
     *
     * @param object  Object to add to the buffer. FIFO gets ownership.
     * @param mode    Where to insert the object:
     *  - PutHead: (default) object is put to the head of the buffer.
     *  - PutTail: object is put to the tail, meaning it will be the
     *    next one to come out.
     */
    void put(Type *object, PutMode mode = PutHead)
    {
        DE_GUARD(this);
        if (mode == PutHead)
        {
            _objects.push_front(object);
        }
        else
        {
            _objects.push_back(object);
        }
    }

    /**
     * Takes the oldest object in the buffer.
     *
     * @return The oldest object in the buffer, or nullptr if the buffer is empty.
     * Caller gets ownership of the returned object.
     */
    Type *take()
    {
        DE_GUARD(this);
        if (_objects.empty()) return nullptr;
        Type *last = _objects.back();
        _objects.pop_back();
        return last;
    }

    /**
     * Returns the oldest object in the buffer.
     *
     * @return The oldest object in the buffer, or nullptr if the buffer is empty.
     * The object is not removed from the buffer.
     */
    Type *tail() const
    {
        DE_GUARD(this);
        if (_objects.empty()) return nullptr;
        return _objects.back();
    }

    /**
     * Determines whether the buffer is empty.
     */
    bool isEmpty() const
    {
        DE_GUARD(this);
        return _objects.empty();
    }

    void clear()
    {
        DE_GUARD(this);
        while (!isEmpty())
        {
            delete take();
        }
    }

    void filter(const std::function<bool(Type *)> &cond)
    {
        DE_GUARD(this);
        _objects.remove_if(cond);
    }

private:
    typedef std::list<Type *> Objects;
    Objects _objects;
};

} // namespace de

#endif // LIBCORE_FIFO_H
