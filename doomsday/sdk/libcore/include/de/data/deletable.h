/** @file deletable.h  Object whose deletion can be observed.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
        if (_ptr) _ptr->Deletable::audienceForDeletion -= this;
        _ptr = ptr;
        if (_ptr) _ptr->Deletable::audienceForDeletion += this;
    }
    SafePtr &operator = (SafePtr const &other) {
        reset(other._ptr);
        return *this;
    }
    Type *operator -> () const {
        return _ptr;
    }
    operator Type const * () const {
        return _ptr;
    }
    operator Type * () {
        return _ptr;
    }
    Type *get() const {
        return _ptr;
    }
    explicit operator bool() const {
        return _ptr != nullptr;
    }
    void objectWasDeleted(Deletable *obj) {
        if (obj == _ptr) {
            _ptr = nullptr;
        }
    }
private:
    Type *_ptr = nullptr;
};

} // namespace de

#endif // LIBDENG2_DELETABLE_H

