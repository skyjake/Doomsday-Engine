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

#include "de/byterefarray.h"
#include <cstring>

namespace de {

ByteRefArray::ByteRefArray()
    : _writeBase(nullptr)
    , _readBase(nullptr)
    , _size(0)
{}

ByteRefArray::ByteRefArray(void *base, Size size)
    : _writeBase(reinterpret_cast<Byte *>(base))
    , _readBase(reinterpret_cast<const Byte *>(base))
    , _size(size)
{}

ByteRefArray::ByteRefArray(const void *base, Size size)
    : _writeBase(nullptr),
      _readBase(reinterpret_cast<const Byte *>(base)),
      _size(size)
{}

ByteRefArray ByteRefArray::fromCStr(const char *nullTerminatedCStr)
{
    return ByteRefArray(nullTerminatedCStr, std::strlen(nullTerminatedCStr));
}

void *ByteRefArray::base()
{
    return _writeBase;
}

const void *ByteRefArray::base() const
{
    return _readBase;
}

void ByteRefArray::clear()
{
    fill(0);
}

void ByteRefArray::fill(IByteArray::Byte value)
{
    if (!_writeBase)
    {
        /// @throw NonModifiableError  The referenced array is read-only.
        throw NonModifiableError("ByteRefArray::fill", "Array is read-only");
    }
    std::memset(_writeBase, value, _size);
}

ByteRefArray::Size ByteRefArray::size() const
{
    return _size;
}

void ByteRefArray::get(Offset at, Byte *values, Size count) const
{
    DE_ASSERT(_readBase != nullptr);
    if (at + count > _size)
    {
        /// @throw OffsetError  The accessed region was out of range.
        throw OffsetError("ByteRefArray::get", "Out of range");
    }
    std::memmove(values, _readBase + at, count);
}

void ByteRefArray::set(Offset at, const Byte *values, Size count)
{
    if (!_writeBase)
    {
        /// @throw NonModifiableError  The referenced array is read-only.
        throw NonModifiableError("ByteRefArray::set", "Array is read-only");
    }

    if (at + count > _size)
    {
        /// @throw OffsetError  The accessed region was out of range.
        throw OffsetError("ByteRefArray::set", "Out of range");
    }
    std::memmove(_writeBase + at, values, count);
}

} // namespace de
