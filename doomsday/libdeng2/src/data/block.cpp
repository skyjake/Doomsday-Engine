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

#include "de/Block"
#include "de/File"

using namespace de;

Block::Block(Size initialSize)
{
    resize(initialSize);
}

Block::Block(IByteArray const &other)
{
    // Read the other's data directly into our data buffer.
    resize(other.size());
    other.get(0, (dbyte *) data(), other.size());
}

Block::Block(Block const &other) : QByteArray(other), IByteArray(), IBlock()
{}

Block::Block(QByteArray const &byteArray)
    : QByteArray(byteArray)
{}

Block::Block(IIStream &stream)
{
    stream >> *this;
}

Block::Block(IIStream const &stream)
{
    stream >> *this;
}

Block::Block(IByteArray const &other, Offset at, Size count) : IByteArray()
{
    copyFrom(other, at, count);
}

Block::Size Block::size() const
{
    return QByteArray::size();
}

void Block::get(Offset atPos, Byte *values, Size count) const
{
    if(atPos + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::get", "Out of range");
    }

    for(Offset i = atPos; count > 0; ++i, --count)
    {
        *values++ = Byte(at(i));
    }
}

void Block::set(Offset at, Byte const *values, Size count)
{
    if(at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::set", "Out of range");
    }
    replace(at, count, QByteArray((char const *) values, count));
}

void Block::copyFrom(IByteArray const &array, Offset at, Size count)
{
    // Read the other's data directly into our data buffer.
    resize(count);
    array.get(at, data(), count);
}

void Block::resize(Size size)
{
    QByteArray::resize(size);
}

Block::Byte *Block::data()
{
    return reinterpret_cast<Byte *>(QByteArray::data());
}

Block::Byte const *Block::data() const
{
    return reinterpret_cast<Byte const *>(QByteArray::data());
}

Block &Block::operator += (Block const &other)
{
    append(other);
    return *this;
}

Block &Block::operator = (Block const &other)
{
    *static_cast<QByteArray *>(this) = static_cast<QByteArray const &>(other);
    return *this;
}

void Block::clear()
{
    QByteArray::clear();
}
