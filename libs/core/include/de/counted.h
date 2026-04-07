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

#ifndef LIBCORE_COUNTED_H
#define LIBCORE_COUNTED_H

#include "libcore.h"
#include <atomic>

namespace de {

/**
 * Reference-counted object. Gets destroyed when its reference counter
 * hits zero.
 *
 * @ingroup data
 */
class DE_PUBLIC Counted
{
public:
    /**
     * Constructs a new reference-counted object. New counted objects have a
     * reference count of 1 -- it is assumed that whoever constructs the object
     * holds on to one reference.
     */
    Counted();

    /**
     * Acquires a reference to the reference-counted object. Use the
     * template to get the correct type of pointer from a derived class.
     *
     * See de::holdRef() for a slightly more convenient way to hold a reference.
     */
    template <typename Type>
    inline Type *ref() {
        addRef();
        return static_cast<Type *>(this);
    }

    template <typename Type>
    inline const Type *ref() const {
        addRef();
        return static_cast<const Type *>(this);
    }

    /**
     * Releases a reference that was acquired earlier with ref(). The
     * object destroys itself when the reference counter hits zero.
     *
     * See de::releaseRef() for a more convenient way to release a reference.
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

public:
    /// Number of other things that refer to this object, i.e. have
    /// a pointer to it.
    mutable std::atomic_int _refCount;

private:
    template <typename Type>
    friend Type *refless(Type *counted);

#ifdef DE_DEBUG
public:
    static std::atomic_int totalCount; // Number of Counted objects in existence.
# ifdef DE_USE_COUNTED_TRACING
    static void printAllocs();
# endif
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
    if (!counted) return 0;
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
    if (!counted) return 0;
    return counted->template ref<CountedType>();
}

/**
 * Holds a reference to a Counted object (by reference).
 * @param counted  Counted object.
 * @return Same as @a counted (with reference count incremented).
 */
template <typename CountedType>
inline CountedType *holdRef(CountedType &counted) {
    return counted.template ref<CountedType>();
}

/**
 * Holds a reference to a Counted object (by const reference).
 * @param counted  Counted object.
 * @return Same as @a counted (with reference count incremented).
 */
template <typename CountedType>
inline const CountedType *holdRef(const CountedType &counted) {
    return counted.template ref<CountedType>();
}

template <typename CountedType>
inline void changeRef(const CountedType *&counted, const Counted *newRef) {
    const CountedType *old = counted;
    counted = (newRef? newRef->ref<CountedType>() : 0);
    releaseRef(old);
}

template <typename CountedType>
inline void changeRef(const CountedType *&counted, const Counted &newRef) {
    const CountedType *old = counted;
    counted = newRef.ref<CountedType>();
    releaseRef(old);
}

template <typename CountedType>
inline void changeRef(CountedType *&counted, Counted *newRef) {
    CountedType *old = counted;
    counted = (newRef? newRef->ref<CountedType>() : 0);
    releaseRef(old);
}

template <typename CountedType>
inline void changeRef(CountedType *&counted, Counted &newRef) {
    CountedType *old = counted;
    counted = newRef.ref<CountedType>();
    releaseRef(old);
}

/**
 * Releases a reference to a Counted object. Afterwards, the pointer is cleared
 * to NULL.
 *
 * @param ref  Pointer that holds a reference. May be NULL.
 */
template <typename CountedType>
inline void releaseRef(CountedType *&ref) {
    if (ref) ref->release();
    ref = 0;
}

/**
 * Releases a const reference to a Counted object. Afterwards, the pointer is
 * cleared to NULL.
 *
 * @param ref  Pointer that holds a reference. May be NULL.
 */
template <typename CountedType>
inline void releaseRef(const CountedType *&ref) {
    if (ref) ref->release();
    ref = 0;
}

/**
 * Utility for passing Counted objects as arguments.
 *
 * RefArg enforces the following conventions when used as a method argument type:
 * - If a Counted non-const pointer is given as an argument, it is assumed the
 *   caller has already held a reference and is giving that reference's ownership
 *   away. For instance, when constructing new Counted objects.
 * - If a Counted const reference is given as an argument, no changes occur in the
 *   object's refcount.
 *
 * The method that uses RefArgs must hold a reference to each object that is passed in.
 */
template <typename CountedType>
class RefArg
{
public:
    RefArg() : _ref(0) {}
    RefArg(const RefArg &other) : _ref(other._ref) {}
    RefArg(CountedType *preHeld) : _ref(refless(preHeld)) {}
    RefArg(const CountedType &ref) : _ref(const_cast<CountedType *>(&ref)) {}
    operator const CountedType * () const { return _ref; }
    operator CountedType * () { return _ref; }
    operator const CountedType & () const { return *_ref; }
    CountedType *get() { return _ref; }
    const CountedType &operator *  () const { return *_ref; }
    CountedType &      operator *  ()       { return *_ref; }
    const CountedType *operator -> () const { return _ref;  }
    CountedType *      operator -> ()       { return _ref;  }
    CountedType *holdRef() { return de::holdRef(_ref); }
private:
    CountedType *_ref;
};

/**
 * Utility for managing a newly created Counted object. Automatically releases the
 * reference added by the constructor.
 */
template <typename CountedType>
class AutoRef
{
public:
    AutoRef() : _ref(nullptr) {}
    AutoRef(CountedType *preHeld) : _ref(preHeld) {}
    AutoRef(CountedType &ref) : _ref(holdRef(ref)) {}
    ~AutoRef() { releaseRef(_ref); }
    void reset(RefArg<CountedType> ref) { changeRef(_ref, ref); }
    CountedType *get() const { return _ref; }
    CountedType &operator *  () const { return *_ref; }
    CountedType *operator -> () const { return _ref; }
    operator const CountedType * () const { return _ref; }
    operator CountedType *       ()       { return _ref; }
    operator const CountedType & () const { return *_ref; }
    operator CountedType &       ()       { return *_ref; }
    operator RefArg<CountedType> () const { return RefArg<CountedType>(*_ref); }
    explicit operator bool       () const { return _ref != nullptr; }
private:
    CountedType *_ref;
};

} // namespace de

#endif /* LIBCORE_COUNTED_H */
