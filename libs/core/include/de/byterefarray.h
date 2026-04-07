/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_BYTEREFARRAY_H
#define LIBCORE_BYTEREFARRAY_H

#include "de/ibytearray.h"

namespace de {

/**
 * Byte array that operates on a pointer-based array. Instances of
 * ByteRefArray are fixed size: one cannot write past the end of
 * the array. @ingroup data
 */
class DE_PUBLIC ByteRefArray : public IByteArray
{
public:
    /// set() is attempted on a nonmodifiable array. @ingroup errors
    DE_ERROR(NonModifiableError);

public:
    /**
     * Constructs a reference array to NULL with zero size.
     */
    ByteRefArray();

    /**
     * Constructs a new byte reference array.
     *
     * @param base  Pointer to the start of the array.
     * @param size  Total size of the array.
     */
    ByteRefArray(void *base, Size size);

    /**
     * Constructs a new non-modifiable byte reference array.
     *
     * @param base  Pointer to the start of the array.
     * @param size  Total size of the array.
     */
    ByteRefArray(const void *base, Size size);

    /**
     * Constructs a non-modifiable byte reference array from a null terminated C
     * string.
     *
     * @param nullTerminatedCStr  Pointer to the start of the string.
     */
    static ByteRefArray fromCStr(const char *nullTerminatedCStr);

    /**
     * Returns a pointer to the start of the array.
     */
    void *base();

    template <typename Type>
    const Type *baseAs() const { return reinterpret_cast<const Type *>(base()); }

    /**
     * Returns a non-modifiable pointer to the start of the array.
     */
    const void *base() const;

    /**
     * Returns a non-modifiable pointer to the start of the array.
     */
    const void *readBase() const { return base(); }

    /**
     * Sets the contents of the array to zero.
     */
    void clear();

    /**
     * Sets the contents of the array to a specific value.
     *
     * @param value  Value to write to all bytes.
     */
    void fill(Byte value);

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, const Byte *values, Size count);

private:
    Byte *_writeBase;
    const Byte *_readBase;
    Size _size;
};

} // namespace de

#endif // LIBCORE_BYTEREFARRAY_H
