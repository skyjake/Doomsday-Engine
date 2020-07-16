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

#include "de/bytesubarray.h"
#include "de/math.h"

namespace de {

ByteSubArray::ByteSubArray(IByteArray &mainArray, Offset at, Size size)
    : _mainArray(&mainArray), _constMainArray(&mainArray), _at(at), _size(size)
{}

ByteSubArray::ByteSubArray(const IByteArray &mainArray, Offset at, Size size)
    : _mainArray(nullptr), _constMainArray(&mainArray), _at(at), _size(size)
{}

ByteSubArray::ByteSubArray(const IByteArray &mainArray, Offset at)
    : _mainArray(nullptr), _constMainArray(&mainArray), _at(at), _size(mainArray.size() - at)
{}

ByteSubArray::Size ByteSubArray::size() const
{
    return _size;
}

void ByteSubArray::get(Offset at, Byte *values, Size count) const
{
    _constMainArray->get(_at + at, values, count);
}

void ByteSubArray::set(Offset at, const Byte *values, Size count)
{
    if (!_mainArray)
    {
        /// @throw NonModifiableError @a mainArray is non-modifiable.
        throw NonModifiableError("ByteSubArray::set", "Array is non-modifiable.");
    }
    _mainArray->set(_at + at, values, count);
    _size = de::max(_size, at + count);
}

} // namespace de
