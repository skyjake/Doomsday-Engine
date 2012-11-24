/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_IBYTEARRAY_H
#define LIBDENG2_IBYTEARRAY_H

#include "../libdeng2.h"

namespace de {

/**
 * Interface for byte arrays that support random access to the array elements.
 *
 * @ingroup data
 */
class DENG2_PUBLIC IByteArray
{
public:
    /// Invalid offset was used in set() or get(). @ingroup errors
    DENG2_ERROR(OffsetError);

    /// Size of the array is indicated with this type.
    typedef dsize Size;

    /// Array elements are indexed using this type.
    typedef dsize Offset;

    /// The elements of the array must be of type Byte.
    typedef dbyte Byte;

public:
    virtual ~IByteArray() {}

    /// Returns the length of the array.
    virtual Size size() const = 0;

    /**
     * Gets bytes from the array.
     *
     * @param at      Start offset of region to read.
     * @param values  Bytes are written here.
     * @param count   Number of bytes to read.
     *
     * @throw IByteArray::OffsetError  Region's start or end goes outside the bounds of the array.
     */
    virtual void get(Offset at, Byte *values, Size count) const = 0;

    /**
     * Sets the bytes starting from location @a at to the given values.
     * The array grows to fit the written region. For example, if the offset
     * is exactly at the end of the array, the array is grown
     * by @a count.
     *
     * @param at      Start offset of region to write.
     * @param values  Bytes to write.
     * @param count   Number of bytes to write.
     *
     * @throw IByteArray::OffsetError  @a at is past the end of the array. For example,
     * attempting to write to offset 1 when the array is empty.
     */
    virtual void set(Offset at, Byte const *values, Size count) = 0;
};

} // namespace de

#endif /* LIBDENG2_IBYTEARRAY_H */
