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

#ifndef LIBCORE_BYTESUBARRAY_H
#define LIBCORE_BYTESUBARRAY_H

#include "de/ibytearray.h"

namespace de {

/**
 * Accesses a portion of a IByteArray object.
 *
 * @ingroup data
 */
class DE_PUBLIC ByteSubArray : public IByteArray
{
public:
    /// set() is attempted on a nonmodifiable array. @ingroup errors
    DE_ERROR(NonModifiableError);

public:
    /**
     * Constructs a modifiable sub-array which refers to the @a mainArray.
     */
    ByteSubArray(IByteArray &mainArray, Offset at, Size size);

    /**
     * Constructs a non-modifiable sub-array which refers to the @a mainArray.
     */
    ByteSubArray(const IByteArray &mainArray, Offset at, Size size);

    /**
     * Constructs a non-modifiable sub-array which refers to the @a mainArray.
     * The sub-array starts at @a at and continues until the end of @a mainArray.
     */
    ByteSubArray(const IByteArray &mainArray, Offset at);

    virtual ~ByteSubArray() {}

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, const Byte *values, Size count);

private:
    IByteArray *_mainArray;
    const IByteArray *_constMainArray;
    Offset _at;
    Size _size;
};

} // namespace de

#endif /* LIBCORE_BYTESUBARRAY_H */
