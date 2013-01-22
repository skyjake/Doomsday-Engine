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

#ifndef LIBDENG2_COUNTED_H
#define LIBDENG2_COUNTED_H

#include "../libdeng2.h"

namespace de {
/**
 * Reference-counted object. Gets destroyed when its reference counter
 * hits zero.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Counted
{
public:
    /**
     * New counted objects have a reference count of 1 -- it is assumed
     * that whoever constructs the object holds on to one reference.
     */
    Counted();

    /**
     * Acquires a reference to the reference-counted object. Use the
     * template to get the correct type of pointer from a derived class.
     */
    template <typename Type>
    Type *ref() {
        addRef();
        return static_cast<Type *>(this);
    }

    /**
     * Releases a reference that was acquired earlier with ref(). The
     * object destroys itself when the reference counter hits zero.
     */
    void release();

protected:
    /**
     * Modifies the reference counter. If the reference count is reduced to
     * zero, the caller is responsible for making sure the object gets
     * deleted.
     *
     * @param count  Added to the reference counter.
     */
    void addRef(dint count = 1) {
        _refCount += count;
        DENG2_ASSERT(_refCount >= 0);
    }

    /**
     * When a counted object is destroyed, its reference counter must be
     * zero. Note that this is only called from release() when all
     * references have been released.
     */
    virtual ~Counted();

private:
    /// Number of other things that refer to this object, i.e. have
    /// a pointer to it.
    dint _refCount;

    template <typename Type>
    friend Type *refless(Type *counted);
};

/**
 * Reduces a reference count by one without deleting the object. This should be
 * used when allocating reference-counted objects (with new) in a situation
 * where a reference will immediately be taken by someone else. It avoids one
 * having to call release() on the newly allocated object.
 */
template <typename Type>
inline Type *refless(Type *counted) {
    counted->addRef(-1);
    return counted;
}

} // namespace de

#endif /* LIBDENG2_COUNTED_H */
