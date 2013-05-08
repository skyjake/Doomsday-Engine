/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_BYTEREFARRAY_H
#define LIBDENG2_BYTEREFARRAY_H

#include "../IByteArray"

namespace de {

/**
 * Byte array that operates on a pointer-based array. Instances of
 * ByteRefArray are fixed size: one cannot write past the end of
 * the array.
 */
class DENG2_PUBLIC ByteRefArray : public IByteArray
{
public:
    /// set() is attempted on a nonmodifiable array. @ingroup errors
    DENG2_ERROR(NonModifiableError);

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
    ByteRefArray(void const *base, Size size);

    /**
     * Constructs a non-modifiable byte reference array from a null terminated C
     * string.
     *
     * @param nullTerminatedCStr  Pointer to the start of the string.
     */
    static ByteRefArray fromCStr(char const *nullTerminatedCStr);

    /**
     * Returns a pointer to the start of the array.
     */
    void *base();

    /**
     * Returns a non-modifiable pointer to the start of the array.
     */
    void const *base() const;

    /**
     * Returns a non-modifiable pointer to the start of the array.
     */
    void const *readBase() const { return base(); }

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
    void set(Offset at, Byte const *values, Size count);

private:
    Byte *_writeBase;
    Byte const *_readBase;
    Size _size;
};

} // namespace de

#endif // LIBDENG2_BYTEREFARRAY_H
