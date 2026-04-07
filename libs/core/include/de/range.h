/** @file range.h  Linear range.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_RANGE_H
#define LIBCORE_RANGE_H

#include "de/list.h"
#include "de/string.h"
#include "de/libcore.h"
#include "de/math.h"

namespace de {

/**
 * Linear value range. The start point is inclusive while the end point is
 * exclusive. The end point should be larger in value than the start point.
 *
 * @ingroup math
 */
template <typename Type>
struct Range
{
    Type start;
    Type end;

    Range(const Type &a = Type(0), const Type &b = Type(0)) : start(a), end(b) {}
    static Range fromSize(const Type &a, const Type &size) { return Range(a, a + size); }
    inline bool isEmpty() const { return end == start; }
    explicit inline operator bool() const { return !isEmpty(); }
    inline decltype(end - start) size() const { return end - start; }
    inline bool contains(const Type &i) const { return i >= start && i < end; }
    inline Type clamp(const Type &i) const {
        if (i < start) return start;
        if (i > end) return end;
        return i;
    }
    inline Range intersection(const Range &other) {
        if (start > other.end || end < other.start) {
            return Range(); // Do not intersect.
        }
        Range inter(de::max(start, other.start), de::min(end, other.end));
        if (inter.end <= inter.start) return Range();
        return inter;
    }
    inline Type wrap(const Type &i) const {
        return de::wrap(i, start, end);
    }
    inline Type random() const {
        return start + randf() * size();
    }
    inline Range &operator |= (const Type &value) {
        start = de::min(start, value);
        end   = de::max(end,   value);
        return *this;
    }
    inline Range &operator &= (const Range &other) {
        start = de::max(start, other.start);
        end   = de::min(end,   other.end);
        if (start > end) end = start;
        return *this;
    }
    inline bool operator == (const Range &other) const {
        return start == other.start && end == other.end;
    }
    inline bool operator != (const Range &other) const {
        return !(*this == other);
    }
    inline bool operator < (const Range &other) const {
        return start < other.start;
    }
    inline bool operator < (const Type &value) const {
        return start < value && end < value;
    }
    inline bool operator > (const Type &value) const {
        return start > value && end > value;
    }
    inline bool operator <= (const Type &value) const {
        return !(*this > value);
    }
    inline bool operator >= (const Type &value) const {
        return !(*this < value);
    }
    inline Range<Type> operator + (Type offset) const {
        return Range<Type>(start + offset, end + offset);
    }
    inline Range<Type> operator - (Type offset) const {
        Range<Type> r(*this);
        r -= offset;
        return r;
    }
    inline Range<Type> &operator -= (Type offset) {
        start -= offset;
        end -= offset;
        return *this;
    }
    String asText() const {
        return String("[") + String::asText(start) + "..." + String::asText(end) + ")";
    }

    typedef List<Range<Type>> ContiguousRanges;

    /**
     * Finds a sequence of contiguous ranges in the input values. Only use with integer types.
     *
     * @param values  List of input values. Must be sorted in ascending order.
     *
     * @return List of contiguous ranges. As usual, range starts are inclusive and range ends are
     * exclusive.
     */
    static ContiguousRanges findContiguousRanges(const List<Type> &values)
    {
        ContiguousRanges cont;
        if (values.isEmpty()) return cont;
        cont.append(Range<Type>(values.first(), values.first() + 1));
        for (dsize i = 1; i < values.size(); ++i) {
            Range<Type> &last = cont.last();
            if (values.at(i) == last.end) {
                last.end = values.at(i) + 1;
            }
            else {
                cont.append(Range<Type>(values.at(i), values.at(i) + 1));
            }
        }
        return cont;
    }
    static String contiguousRangesAsText(const List<Type> &values, const char *separator = ", ")
    {
        StringList msg;
        for (const auto &range : findContiguousRanges(values)) {
            if (range.size() == 1)
                msg << String::asText(range.start);
            else
                msg << String::asText(range.start) + "-" + String::asText(range.end - 1);
        }
        return String::join(msg, separator);
    }
};

typedef Range<duint16>      Rangeui16;
typedef Range<dint32>       Rangei;
typedef Range<duint32>      Rangeui;
typedef Range<dint64>       Rangei64;
typedef Range<duint64>      Rangeui64;
typedef Range<dsize>        Rangez;
typedef Range<dfloat>       Rangef;
typedef Range<ddouble>      Ranged;
typedef Range<const char *> Rangecc;

} // namespace de

#endif // LIBCORE_RANGE_H
