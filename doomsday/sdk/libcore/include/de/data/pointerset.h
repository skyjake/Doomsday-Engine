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

#ifndef LIBDENG2_POINTERSET_H
#define LIBDENG2_POINTERSET_H

#include "../libcore.h"
#include "../Range"
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
class DENG2_PUBLIC PointerSet
{
public:
    typedef void * Pointer;
    typedef Pointer const * const_iterator;
    typedef duint16 Flag;

    static Flag const AllowInsertionDuringIteration;
    static Flag const BeingIterated;

public:
    PointerSet();
    PointerSet(PointerSet const &other);
    PointerSet(PointerSet &&moved);

    ~PointerSet();

    void insert(Pointer ptr);
    void remove(Pointer ptr);
    bool contains(Pointer ptr) const;
    void clear();

    PointerSet &operator = (PointerSet &&moved);

    inline void setFlags(Flag flags, FlagOpArg op = SetFlags) {
        applyFlagOperation(_flags, flags, op);
    }

    inline Flag flags() const           { return _flags; }
    inline int size() const             { return _range.size(); }
    inline Rangeui16 usedRange() const  { return _range; }
    inline int allocatedSize() const    { return _size; }
    inline const_iterator begin() const { return _pointers + _range.start; }
    inline const_iterator end() const   { return _pointers + _range.end; }

protected:
    Rangeui16 locate(Pointer ptr) const;

    inline Pointer at(duint16 pos) const { return _pointers[pos]; }

private:
    Pointer *_pointers;
    duint16 _flags;
    duint16 _size;
    Rangeui16 _range;
};

} // namespace de


#endif // LIBDENG2_POINTERSET_H
