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
#include <QList>
#include <QStringList>

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
    inline bool isEmpty() const { return end == start; }
    inline Type size() const { return end - start; }
    inline bool contains(Type const &i) const { return i >= start && i < end; }
    inline Type clamp(Type const &i) const {
        if(i < start) return start;
        if(i > end) return end;
        return i;
    }
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
    inline bool operator != (Range const &other) const {
        return !(*this == other);
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

    typedef QList< Range<Type> > ContiguousRanges;

    /**
     * Finds a sequence of contiguous ranges in the input values. Only use with integer types.
     *
     * @param values  List of input values. Must be sorted in ascending order.
     *
     * @return List of contiguous ranges. As usual, range starts are inclusive and range ends are
     * exclusive.
     */
    static ContiguousRanges findContiguousRanges(QList<Type> const &values) {
        ContiguousRanges cont;
        if(values.isEmpty()) return cont;
        cont.append(Range<Type>(values.first(), values.first() + 1));
        for(int i = 1; i < values.size(); ++i) {
            Range<Type> &last = cont.last();
            if(values.at(i) == last.end) {
                last.end = values.at(i) + 1;
            }
            else {
                cont.append(Range<Type>(values.at(i), values.at(i) + 1));
            }
        }
        return cont;
    }
    static QString contiguousRangesAsText(QList<Type> const &values,
                                          QString const &separator = ", ") {
        QStringList msg;
        foreach(Range<Type> const &range, findContiguousRanges(values)) {
            if(range.size() == 1)
                msg << QString::number(range.start);
            else
                msg << QString("%1-%2").arg(range.start).arg(range.end - 1);
        }
        return msg.join(separator);
    }
};

typedef Range<dint32>  Rangei;
typedef Range<duint32> Rangeui;
typedef Range<dint64>  Rangei64;
typedef Range<dfloat>  Rangef;
typedef Range<ddouble> Ranged;

} // namespace de

#endif // LIBDENG2_RANGE_H
