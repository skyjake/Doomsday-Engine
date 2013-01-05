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

#include "de/ByteRefArray"
#include <cstring>

using namespace de;

ByteRefArray::ByteRefArray(Byte *base, Size size) : _writeBase(base), _readBase(base), _size(size)
{}

ByteRefArray::ByteRefArray(Byte const *base, Size size) : _writeBase(0), _readBase(base), _size(size)
{}

ByteRefArray::Size ByteRefArray::size() const
{
    return _size;
}

void ByteRefArray::get(Offset at, Byte *values, Size count) const
{
    if(at + count > size())
    {
        /// @throw OffsetError  The accessed region was out of range.
        throw OffsetError("ByteRefArray::get", "Out of range");
    }
    std::memmove(values, _readBase + at, count);
}

void ByteRefArray::set(Offset at, Byte const *values, Size count)
{
    if(!_writeBase)
    {
        /// @throw NonModifiableError  The referenced array is read-only.
        throw NonModifiableError("ByteRefArray::set", "Array is read-only");
    }

    if(at + count > size())
    {
        /// @throw OffsetError  The accessed region was out of range.
        throw OffsetError("ByteRefArray::set", "Out of range");
    }
    std::memmove(_writeBase + at, values, count);
}
