/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Block"

using namespace de;

Block::Block(Size initialSize)
{
    _data.resize(initialSize);
}

Block::Block(const IByteArray& other)
{
    // Read the other's data directly into our data buffer.
    _data.resize(other.size());
    other.get(0, &_data[0], other.size());
}

Block::Block(const IByteArray& other, Offset at, Size count) : IByteArray()
{
    copyFrom(other, at, count);
}

Block::Size Block::size() const
{
    return _data.size();
}

void Block::get(Offset at, Byte* values, Size count) const
{
    if(at + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::get", "Out of range");
    }

    for(Data::const_iterator i = _data.begin() + at; count > 0; ++i, --count)
    {
        *values++ = *i;
    }
}

void Block::set(Offset at, const Byte* values, Size count)
{
    if(at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::set", "Out of range");
    }

    _data.insert(_data.begin() + at, values, values + count);
}

void Block::copyFrom(const IByteArray& array, Offset at, Size count)
{
    // Read the other's data directly into our data buffer.
    _data.resize(count);
    array.get(at, &_data[0], count);
}

void Block::resize(Size size)
{
    _data.resize(size);
}

const Block::Byte* Block::data() const
{
    return &_data[0];
}

Block& Block::operator += (const Block& other)
{
    _data.reserve(size() + other.size());
    _data.insert(_data.end(), other._data.begin(), other._data.end());
    return *this;
}

void Block::clear()
{
    _data.clear();
}
