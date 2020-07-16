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

#ifndef LIBCORE_IBYTEARRAY_H
#define LIBCORE_IBYTEARRAY_H

#include "de/libcore.h"

namespace de {

/**
 * Interface for byte arrays that support random access to the array elements.
 *
 * @ingroup data
 */
class DE_PUBLIC IByteArray
{
public:
    /// Invalid offset was used in set() or get(). @ingroup errors
    DE_ERROR(OffsetError);

    /// Size of the array is indicated with this type.
    typedef dsize Size;

    /// Array elements are indexed using this type.
    typedef dsize Offset;

    /// Difference between two offsets.
    typedef dsigsize Delta;

    /// The elements of the array must be of type Byte.
    typedef dbyte Byte;

public:
    virtual ~IByteArray() = default;

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
    virtual void set(Offset at, const Byte *values, Size count) = 0;
};

} // namespace de

#endif /* LIBCORE_IBYTEARRAY_H */
