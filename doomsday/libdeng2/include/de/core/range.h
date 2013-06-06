/** @file range.h  Linear range.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_RANGE_H
#define LIBDENG2_RANGE_H

#include "../libdeng2.h"
#include "../math.h"

#include <QString>

namespace de {

/**
 * Linear value range. The start point is inclusive while the end point is
 * exclusive. The end point should be larger in value than the start point.
 */
template <typename Type>
struct Range
{
    Type start;
    Type end;

    explicit Range(Type const &a = 0, Type const &b = 0) : start(a), end(b) {}
    inline Type size() const { return end - start; }
    inline bool contains(Type const &i) const { return i >= start && i < end; }
    inline Range &operator |= (Type const &value) {
        start = de::min(start, value);
        end   = de::max(end,   value);
        return *this;
    }
    inline Range &operator &= (Range const &other) {
        start = de::max(start, other.start);
        end   = de::min(end,   other.end);
        if(start > end) end = start;
        return *this;
    }
    inline bool operator == (Range const &other) const {
        return start == other.start && end == other.end;
    }
    inline bool operator < (Range const &other) const {
        return start < other.start;
    }
    inline bool operator < (Type const &value) const {
        return start < value && end < value;
    }
    inline bool operator > (Type const &value) const {
        return start > value && end > value;
    }
    inline bool operator <= (Type const &value) const {
        return !(*this > value);
    }
    inline bool operator >= (Type const &value) const {
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
    QString asText() const {
        return QString("[%1...%2)").arg(start).arg(end);
    }
};

typedef Range<dint32>  Rangei;
typedef Range<duint32> Rangeui;
typedef Range<dint64>  Rangei64;
typedef Range<dfloat>  Rangef;
typedef Range<ddouble> Ranged;

} // namespace de

#endif // LIBDENG2_RANGE_H
