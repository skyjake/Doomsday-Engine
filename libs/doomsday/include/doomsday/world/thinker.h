/** @file thinker.h  Base for all thinkers.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_THINKER_H
#define LIBDOOMSDAY_THINKER_H

#include "../libdoomsday.h"
#include <de/libcore.h>
#include <de/legacy/types.h>

struct thinker_s;

/// Function pointer to a function to handle an actor's thinking.
/// The argument is a pointer to the object doing the thinking.
typedef void (*thinkfunc_t) (void *);

// Thinker flags:
#define THINKF_STD_MALLOC  0x1     // allocated using M_Malloc rather than the zone
#define THINKF_DISABLED    0x2     // thinker is disabled (in stasis)

/**
 * Base for all thinker objects.
 */
typedef struct thinker_s {
    struct thinker_s *prev, *next;
    thinkfunc_t function;
    uint32_t _flags;
    thid_t id;              ///< Only used for mobjs (zero is not an ID).
    void *d;                ///< Private data (owned).
} thinker_t;

#define THINKER_DATA(thinker, T)        (reinterpret_cast<Thinker::IData *>((thinker).d)->as<T>())
#define THINKER_DATA_MAYBE(thinker, T)  (de::maybeAs<T>(reinterpret_cast<Thinker::IData *>((thinker).d)))

#define THINKER_NS(thinker)             (THINKER_DATA((thinker), ThinkerData).objectNamespace())

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC dd_bool Thinker_InStasis(const thinker_t *thinker);

/**
 * Change the 'in stasis' state of a thinker (stop it from thinking).
 *
 * @param thinker  The thinker to change.
 * @param on  @c true, put into stasis.
 */
LIBDOOMSDAY_PUBLIC void Thinker_SetStasis(thinker_t *thinker, dd_bool on);

/**
 * Generic thinker function that does nothing. This can be used if the private
 * data does all the thinking.
 */
LIBDOOMSDAY_PUBLIC void Thinker_NoOperation(void *thinker);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <de/libcore.h>

/**
 * C++ wrapper for a POD thinker.
 *
 * Copying or assigning the thinker via this class ensures that the entire allocated
 * thinker size is copied, and a duplicate of the private data instance is made.
 *
 * Thinker::~Thinker() will delete the entire thinker, including its private data.
 * One can use Thinker::take() to acquire ownership of the POD thinker to prevent it
 * from being destroyed.
 *
 * Ultimately, thinkers should become a C++ class hierarchy, with the private data being
 * a normal de::IPrivate.
 */
class LIBDOOMSDAY_PUBLIC Thinker
{
public:
    /**
     * Transparently accesses a member of the internal POD thinker struct via a member
     * that behaves like a regular member variable. Allows old code that deals with
     * thinker_s to work on a Thinker instance.
     */
    template <typename T>
    class LIBDOOMSDAY_PUBLIC MemberDelegate {
    public:
        MemberDelegate(Thinker &thinker, int offset) : _thinker(thinker), _offset(offset) {}
        inline T &ref() { return *reinterpret_cast<T *>((char *)&_thinker.base() + _offset); }
        inline const T &ref() const { return *reinterpret_cast<const T *>((const char *)&_thinker.base() + _offset); }
        inline T &operator -> () { return ref(); } // pointer type T expected
        inline const T &operator -> () const { return ref(); } // pointer type T expected
        inline operator T & () { return ref(); }
        inline operator const T & () const { return ref(); }
        inline T &operator = (const T &other) {
            ref() = other;
            return ref();
        }
    private:
        Thinker &_thinker;
        int _offset;
    };

    /**
     * Base class for the private data of a thinker.
     */
    class LIBDOOMSDAY_PUBLIC IData
    {
    public:
        virtual ~IData() = default;
        virtual void setThinker(thinker_s *thinker) = 0;
        virtual void think() = 0;
        virtual IData *duplicate() const = 0;

        DE_CAST_METHODS()
    };

    enum AllocMethod { AllocateStandard, AllocateMemoryZone };

public:
    /**
     * Allocates a thinker using standard malloc.
     *
     * @param sizeInBytes  Size of the thinker. At least sizeof(thinker_s).
     * @param data         Optional private instance data.
     */
    Thinker(de::dsize sizeInBytes = 0, IData *data = 0);

    Thinker(AllocMethod alloc, de::dsize sizeInBytes = 0, IData *data = 0);

    Thinker(const Thinker &other);

    /**
     * Constructs a copy of a POD thinker. A duplicate of the private data is made
     * if one is present in @a podThinker.
     *
     * @param podThinker   Existing thinker.
     * @param sizeInBytes  Size of the thinker struct.
     * @param alloc        Allocation method.
     */
    Thinker(const thinker_s &podThinker, de::dsize sizeInBytes,
            AllocMethod alloc = AllocateStandard);

    /**
     * Takes ownership of a previously allocated POD thinker.
     *
     * @param podThinkerToTake  Thinker.
     * @param sizeInBytes       Size of the thinker.
     */
    Thinker(thinker_s *podThinkerToTake, de::dsize sizeInBytes);

    Thinker &operator = (const Thinker &other);

    operator thinker_s * () { return &base(); }
    operator const thinker_s * () { return &base(); }

    struct thinker_s &base();
    const struct thinker_s &base() const;

    void enable(bool yes = true);
    inline void disable(bool yes = true) { enable(!yes); }

    /**
     * Clear everything to zero. The private data is destroyed, so that it will be
     * recreated if needed.
     */
    void zap();

    bool isDisabled() const;
    bool hasData() const;

    /**
     * Determines the size of the thinker in bytes.
     */
    de::dsize sizeInBytes() const;

    /**
     * Gives ownership of the contained POD thinker to the caller. The caller also
     * gets ownership of the private data owned by the thinker (a C++ instance).
     * You should use destroy() to delete the returned memory.
     *
     * After the operation, this Thinker becomes invalid.
     *
     * @return POD thinker. The size of this struct is actually sizeInBytes(), not
     * sizeof(thinker_s).
     */
    struct thinker_s *take();

    IData &data();
    const IData &data() const;

    /**
     * Sets the private data for the thinker.
     *
     * @param data  Private data object. Ownership taken.
     */
    void setData(IData *data);

public:
    /**
     * Destroys a POD thinker that has been acquired using take(). All the memory owned
     * by the thinker is released.
     *
     * @param thinkerBase  POD thinker base.
     */
    static void destroy(thinker_s *thinkerBase);

    static void release(thinker_s &thinkerBase);

    static void zap(thinker_s &thinkerBase, de::dsize sizeInBytes);

private:
    DE_PRIVATE(d)

public:
    // Value accessors (POD thinker_s compatibility for old code; TODO: remove in the future):
    MemberDelegate<thinker_s *> prev;
    MemberDelegate<thinker_s *> next;
    MemberDelegate<thinkfunc_t> function;
    MemberDelegate<thid_t> id;
};

#ifdef _MSC_VER
// MSVC needs some hand-holding.
template class LIBDOOMSDAY_PUBLIC Thinker::MemberDelegate<thinker_s *>;
template class LIBDOOMSDAY_PUBLIC Thinker::MemberDelegate<thinkfunc_t>;
template class LIBDOOMSDAY_PUBLIC Thinker::MemberDelegate<thid_t>;
#endif

/**
 * Template that acts like a smart pointer to a specific type of thinker.
 *
 * Like the base class Thinker, the thinker instance is created in the constructor
 * and destroyed in the destructor of ThinkerT.
 */
template <typename Type>
class ThinkerT : public Thinker
{
public:
    ThinkerT(AllocMethod alloc = AllocateStandard)
        : Thinker(alloc, sizeof(Type)) {}
    ThinkerT(de::dsize sizeInBytes, AllocMethod alloc = AllocateStandard)
        : Thinker(alloc, sizeInBytes) {}
    ThinkerT(const Type &thinker,
             de::dsize sizeInBytes = sizeof(Type),
             AllocMethod alloc     = AllocateStandard)
        : Thinker(reinterpret_cast<const thinker_s &>(thinker), sizeInBytes, alloc) {}
    ThinkerT(Type *thinkerToTake, de::dsize sizeInBytes = sizeof(Type))
        : Thinker(reinterpret_cast<thinker_s *>(thinkerToTake), sizeInBytes) {}

    Type &base() { return *reinterpret_cast<Type *>(&Thinker::base()); }
    const Type &base() const { return *reinterpret_cast<const Type *>(&Thinker::base()); }

    Type *operator -> () { return &base(); }
    const Type *operator -> () const { return &base(); }

    operator Type * () { return &base(); }
    operator const Type * () const { return &base(); }

    operator void * () { return reinterpret_cast<void *>(&base()); }
    operator const void * () const { return reinterpret_cast<const void *>(&base()); }

    operator Type & () { return base(); }
    operator const Type & () const { return base(); }

    Type *take() { return reinterpret_cast<Type *>(Thinker::take()); }

    static void destroy(Type *thinker) { Thinker::destroy(reinterpret_cast<thinker_s *>(thinker)); }

    static void zap(Type &thinker, de::dsize sizeInBytes = sizeof(Type)) {
        Thinker::zap(reinterpret_cast<thinker_s &>(thinker), sizeInBytes);
    }

    static void release(Type &thinker) {
        Thinker::release(reinterpret_cast<thinker_s &>(thinker));
    }
};

#endif // __cplusplus

#endif // LIBDOOMSDAY_THINKER_H
