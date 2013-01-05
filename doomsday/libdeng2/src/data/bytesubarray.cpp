/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ByteSubArray"

namespace de {

ByteSubArray::ByteSubArray(IByteArray &mainArray, Offset at, Size size)
    : _mainArray(&mainArray), _constMainArray(&mainArray), _at(at), _size(size)
{}

ByteSubArray::ByteSubArray(IByteArray const &mainArray, Offset at, Size size)
    : _mainArray(0), _constMainArray(&mainArray), _at(at), _size(size)
{}

ByteSubArray::Size ByteSubArray::size() const
{
    return _size;
}

void ByteSubArray::get(Offset at, Byte *values, Size count) const
{
    _constMainArray->get(_at + at, values, count);
}

void ByteSubArray::set(Offset at, Byte const *values, Size count)
{
    if(!_mainArray)
    {
        /// @throw NonModifiableError @a mainArray is non-modifiable.
        throw NonModifiableError("ByteSubArray::set", "Array is non-modifiable.");
    }    
    _mainArray->set(_at + at, values, count);
    _size = qMax(_size, at + count);
}

} // namespace de
