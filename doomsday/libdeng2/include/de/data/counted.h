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
     * Constructs a new reference-counted object. New counted objects have a
     * reference count of 1 -- it is assumed that whoever constructs the object
     * holds on to one reference.
     */
    Counted();

    /**
     * Converts the reference-counted object to a delegated one. Delegated
     * reference counting means that references held to and released from the
     * object actually are held to/released from the delegate target.
     *
     * @note The reference count of this object is ignored (set to zero). This
     * object must then be deleted directly rathen than via releasing (as
     * releasing would actually attempt to release the delegate target).
     *
     * @param delegate  Delegate target.
     */
    void setDelegate(Counted const *delegate);

    /**
     * Acquires a reference to the reference-counted object. Use the
     * template to get the correct type of pointer from a derived class.
     *
     * @see de::holdRef() for a slightly more convenient way to hold a reference.
     */
    template <typename Type>
    inline Type *ref() {
        addRef();
        return static_cast<Type *>(this);
    }

    template <typename Type>
    inline Type const *ref() const {
        addRef();
        return static_cast<Type const *>(this);
    }

    /**
     * Releases a reference that was acquired earlier with ref(). The
     * object destroys itself when the reference counter hits zero.
     *
     * @see de::releaseRef() for a more convenient way to release a reference.
     */
    void release() const;

protected:
    /**
     * Modifies the reference counter. If the reference count is reduced to
     * zero, the caller is responsible for making sure the object gets
     * deleted.
     *
     * @param count  Added to the reference counter.
     */
    void addRef(dint count = 1) const;

    /**
     * Non-public destructor. When a counted object is destroyed, its reference
     * counter must be zero. Note that this is only called from release() when
     * all references have been released.
     */
    virtual ~Counted();

private:
    /// Number of other things that refer to this object, i.e. have
    /// a pointer to it.
    mutable dint _refCount;

    Counted const *_delegate;

    template <typename Type>
    friend Type *refless(Type *counted);

#ifdef DENG2_DEBUG
public:
    static int totalCount; // Number of Counted objects in existence.
#endif
};

/**
 * Reduces the reference count by one without deleting the object. This should
 * be used when allocating reference-counted objects (with new) in a situation
 * where a reference to the new object is not kept and a reference will
 * immediately be taken by someone else. It avoids one having to call release()
 * on the newly allocated object.
 *
 * @param counted  Reference-counted object.
 */
template <typename Type>
inline Type *refless(Type *counted) {
    if(!counted) return 0;
    counted->addRef(-1);
    return counted;
}

/**
 * Holds a reference to a Counted object.
 * @param counted  Counted object.
 * @return Same as @a counted (with reference count incremented).
 */
template <typename CountedType>
inline CountedType *holdRef(CountedType *counted) {
    if(!counted) return 0;
    return counted->template ref<CountedType>();
}

/**
 * Releases a reference to a Counted object. Afterwards, the pointer is cleared
 * to NULL.
 *
 * @param ref  Pointer that holds a reference. May be NULL.
 */
template <typename CountedType>
inline void releaseRef(CountedType *&ref) {
    if(ref) ref->release();
    ref = 0;
}

/**
 * Releases a const reference to a Counted object. Afterwards, the pointer is
 * cleared to NULL.
 *
 * @param ref  Pointer that holds a reference. May be NULL.
 */
template <typename CountedType>
inline void releaseRef(CountedType const *&ref) {
    if(ref) ref->release();
    ref = 0;
}

} // namespace de

#endif /* LIBDENG2_COUNTED_H */
