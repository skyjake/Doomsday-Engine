/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_FIFO_H
#define LIBDENG2_FIFO_H

#include "../Lockable"
#include "../Guard"

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
    enum PutMode {
        PutHead,
        PutTail
    };

public:
    FIFO() : Lockable() {}

    virtual ~FIFO() {
        DENG2_GUARD(this);
        for(typename Objects::iterator i = _objects.begin(); i != _objects.end(); ++i) {
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
    void put(Type *object, PutMode mode = PutHead) {
        DENG2_GUARD(this);
        if(mode == PutHead) {
            _objects.push_front(object);
        }
        else {
            _objects.push_back(object);
        }
    }

    /**
     * Takes the oldest object in the buffer.
     *
     * @return The oldest object in the buffer, or NULL if the buffer is empty.
     * Caller gets ownership of the returned object.
     */
    Type *take() {
        DENG2_GUARD(this);
        if(_objects.empty()) return NULL;
        Type *last = _objects.back();
        _objects.pop_back();
        return last;
    }

    /**
     * Returns the oldest object in the buffer.
     *
     * @return The oldest object in the buffer, or NULL if the buffer is empty.
     * The object is not removed from the buffer.
     */
    Type* tail() const {
        DENG2_GUARD(this);
        if(_objects.empty()) return NULL;
        return _objects.back();
    }

    /**
     * Determines whether the buffer is empty.
     */
    bool isEmpty() const {
        DENG2_GUARD(this);
        return _objects.empty();
    }

    void clear() {
        DENG2_GUARD(this);
        while(!isEmpty()) delete take();
    }

private:
    typedef std::list<Type *> Objects;
    Objects _objects;
};

} // namespace de

#endif // LIBDENG2_FIFO_H
