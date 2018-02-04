/** @file deletable.h  Object whose deletion can be observed.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_DELETABLE_H
#define LIBDENG2_DELETABLE_H

#include "../Observers"

namespace de {

/**
 * Object whose deletion can be observed.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Deletable
{
public:
    virtual ~Deletable();

    DENG2_DEFINE_AUDIENCE(Deletion, void objectWasDeleted(Deletable *))
};

/**
 * Auto-nulled pointer to a Deletable object. Does not own the target object.
 * The pointer value is guarded by a mutex.
 */
template <typename Type>
class SafePtr : DENG2_OBSERVES(Deletable, Deletion)
{
public:
    SafePtr(Type *ptr = nullptr) {
        reset(ptr);
    }
    SafePtr(SafePtr const &other) {
        reset(other._ptr);
    }
    ~SafePtr() {
        reset(nullptr);
    }
    void reset(Type *ptr = nullptr) {
        DENG2_GUARD(_ptr)
        if (_ptr.value) _ptr.value->Deletable::audienceForDeletion -= this;
        _ptr.value = ptr;
        if (_ptr.value) _ptr.value->Deletable::audienceForDeletion += this;
    }
    SafePtr &operator = (SafePtr const &other) {
        reset(other._ptr);
        return *this;
    }
    Type *operator -> () const {
        DENG2_GUARD(_ptr);
        if (!_ptr) throw Error("SafePtr::operator ->", "Object has been deleted");
        return _ptr;
    }
    operator Type const * () const {
        DENG2_GUARD(_ptr);
        return _ptr;
    }
    operator Type * () {
        DENG2_GUARD(_ptr);
        return _ptr;
    }
    Type *get() const {
        DENG2_GUARD(_ptr);
        return _ptr;
    }
    explicit operator bool() const {
        DENG2_GUARD(_ptr);
        return _ptr.value != nullptr;
    }
    void objectWasDeleted(Deletable *obj) {
        DENG2_GUARD(_ptr);
        if (obj == _ptr.value) {
            _ptr.value = nullptr;
        }
    }
    void lock() {
        _ptr.lock();
    }
    void unlock() {
        _ptr.unlock();
    }
private:
    LockableT<Type *> _ptr { nullptr };
};

} // namespace de

#endif // LIBDENG2_DELETABLE_H

