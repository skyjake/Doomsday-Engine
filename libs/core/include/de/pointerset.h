/** @file pointerset.h  Set of pointers.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_POINTERSET_H
#define LIBCORE_POINTERSET_H

#include "de/libcore.h"
#include "de/range.h"
#include <vector>

namespace de {

/**
 * Set of pointers.
 *
 * Light-weight class specifically designed to be used for observer audiences. Maintains
 * a sorted vector of pointers. Insertions, deletions, and lookups are done with an
 * O(log n) binary search. Insertions start at the middle to allow expansion to both
 * directions. Removing individual pointers is allowed at any time.
 */
class DE_PUBLIC PointerSet
{
public:
    typedef void * Pointer;
    typedef const Pointer * const_iterator;
    typedef duint16 Flag;

    static Flag const AllowInsertionDuringIteration;

    /// Addition was not possible because the set is being iterated.
    DE_ERROR(AdditionForbiddenError);

    class DE_PUBLIC IIterationObserver
    {
    public:
        virtual ~IIterationObserver() = default;
        virtual void pointerSetIteratorsWereInvalidated(const Pointer *oldBase, const Pointer *newBase) = 0;
    };

public:
    PointerSet();
    PointerSet(const PointerSet &other);
    PointerSet(PointerSet &&moved);

    ~PointerSet();

    void insert(Pointer ptr);
    void remove(Pointer ptr);
    bool contains(Pointer ptr) const;
    void clear();
    Pointer take();

    PointerSet &operator = (const PointerSet &other);
    PointerSet &operator = (PointerSet &&moved);

    inline Flag flags() const           { return _flags; }
    inline int size() const             { return _range.size(); }
    inline bool isEmpty() const         { return _range.isEmpty(); }
    inline Rangeui16 usedRange() const  { return _range; }
    inline int allocatedSize() const    { return _size; }
    inline const_iterator begin() const { return _pointers + _range.start; }
    inline const_iterator end() const   { return _pointers + _range.end; }

    inline void setFlags(Flag flags, FlagOpArg op = SetFlags) {
        applyFlagOperation(_flags, flags, op);
    }
    void setBeingIterated(bool yes) const;
    bool isBeingIterated() const;

    void setIterationObserver(IIterationObserver *observer) const;
    inline IIterationObserver *iterationObserver() const { return _iterationObserver; }

protected:
    Rangeui16 locate(Pointer ptr) const;

    inline Pointer at(duint16 pos) const { return _pointers[pos]; }

private:
    Pointer *_pointers;
    mutable IIterationObserver *_iterationObserver;
    mutable duint16 _flags;
    duint16 _size;
    Rangeui16 _range;
};

/**
 * Utility template for storing a particular type of pointers in a PointerSet.
 */
template <typename Type>
class PointerSetT : public PointerSet
{
public:
    typedef PointerSet::Pointer BasePointer;
    typedef Type * Pointer;
    typedef const Type * ConstPointer;
    typedef const Pointer * const_iterator;

public:
    PointerSetT() {}
    PointerSetT(const PointerSetT &other) : PointerSet(other) {}
    PointerSetT(PointerSetT &&moved)      : PointerSet(moved) {}

    inline PointerSetT &operator = (const PointerSetT &other) {
        PointerSet::operator = (other);
        return *this;
    }
    inline PointerSetT &operator = (PointerSetT &&moved) {
        PointerSet::operator = (moved);
        return *this;
    }
    inline void insert(Pointer ptr) {
        PointerSet::insert(reinterpret_cast<BasePointer>(ptr));
    }
    inline void insert(ConstPointer ptr) {
        PointerSet::insert(reinterpret_cast<BasePointer>(const_cast<Pointer>(ptr)));
    }
    inline void remove(Pointer ptr) {
        PointerSet::remove(reinterpret_cast<BasePointer>(ptr));
    }
    inline void remove(ConstPointer ptr) {
        PointerSet::remove(reinterpret_cast<BasePointer>(const_cast<Pointer>(ptr)));
    }
    inline bool contains(Pointer ptr) const {
        return PointerSet::contains(reinterpret_cast<BasePointer>(ptr));
    }
    inline bool contains(ConstPointer ptr) const {
        return PointerSet::contains(reinterpret_cast<BasePointer>(const_cast<Pointer>(ptr)));
    }
    inline const_iterator begin() const {
        return reinterpret_cast<const_iterator>(PointerSet::begin());
    }
    inline const_iterator end() const {
        return reinterpret_cast<const_iterator>(PointerSet::end());
    }
    inline Pointer take() {
        return reinterpret_cast<Pointer>(PointerSet::take());
    }
};

} // namespace de

#endif // LIBCORE_POINTERSET_H
