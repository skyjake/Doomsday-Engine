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

#ifndef LIBCORE_DELETABLE_H
#define LIBCORE_DELETABLE_H

#include "de/observers.h"

namespace de {

/**
 * Object whose deletion can be observed.
 *
 * @ingroup data
 */
class DE_PUBLIC Deletable
{
public:
    virtual ~Deletable();

    DE_DEFINE_AUDIENCE(Deletion, void objectWasDeleted(Deletable *))
};

/**
 * Auto-nulled pointer to a Deletable object. Does not own the target object.
 * The pointer value is guarded by a mutex.
 */
template <typename Type>
class SafePtr : DE_OBSERVES(Deletable, Deletion)
{
public:
    SafePtr(Type *ptr = nullptr) {
        reset(ptr);
    }
    SafePtr(const SafePtr &other) {
        reset(other._ptr);
    }
    ~SafePtr() override {
        reset(nullptr);
    }
    void reset(Type *ptr = nullptr) {
        DE_GUARD(_ptr)
        if (_ptr.value) _ptr.value->Deletable::audienceForDeletion -= this;
        _ptr.value = ptr;
        if (_ptr.value) _ptr.value->Deletable::audienceForDeletion += this;
    }
    SafePtr &operator = (const SafePtr &other) {
        reset(other._ptr);
        return *this;
    }
    Type *operator -> () const {
        DE_GUARD(_ptr);
        if (!_ptr) throw Error("SafePtr::operator ->", "Object has been deleted");
        return _ptr;
    }
    operator const Type * () const {
        DE_GUARD(_ptr);
        return _ptr;
    }
    operator Type * () {
        DE_GUARD(_ptr);
        return _ptr;
    }
    Type *get() const {
        DE_GUARD(_ptr);
        return _ptr;
    }
    explicit operator bool() const {
        DE_GUARD(_ptr);
        return _ptr.value != nullptr;
    }
    void objectWasDeleted(Deletable *obj) override {
        DE_GUARD(_ptr);
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

#endif // LIBCORE_DELETABLE_H

