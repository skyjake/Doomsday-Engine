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

#include "de/Writer"
#include "de/String"
#include "de/Block"
#include "de/ISerializable"
#include "de/FixedByteArray"
#include "de/data/byteorder.h"
#include <QScopedPointer>

using namespace de;

Writer::Writer(IByteArray& destination, const ByteOrder& byteOrder, IByteArray::Offset offset)
    : _destination(destination), _offset(offset), _fixedOffset(0), _convert(byteOrder)
{}

Writer::Writer(IByteArray& destination, IByteArray::Offset offset)
    : _destination(destination), _offset(offset), _fixedOffset(0), _convert(littleEndianByteOrder)
{}

Writer::Writer(const Writer& other, const ByteOrder& byteOrder)
    : _destination(other._destination), _offset(0), 
      _fixedOffset(other._fixedOffset + other._offset), _convert(byteOrder)
{}

Writer& Writer::operator << (const char& byte)
{
    _destination.set(_fixedOffset + _offset, reinterpret_cast<const IByteArray::Byte*>(&byte), 1);
    ++_offset;
    return *this;
}

Writer& Writer::operator << (const dchar& byte)
{
    _destination.set(_fixedOffset + _offset, reinterpret_cast<const IByteArray::Byte*>(&byte), 1);
    ++_offset;
    return *this;
}

Writer& Writer::operator << (const duchar& byte)
{
    _destination.set(_fixedOffset + _offset, &byte, 1);
    ++_offset;
    return *this;
}

Writer& Writer::operator << (const dint16& word)
{
    return *this << static_cast<duint16>(word);
}   

Writer& Writer::operator << (const duint16& word)
{
    duint16 netWord;
    _convert.nativeToForeign(word, netWord);
    _destination.set(_fixedOffset + _offset, reinterpret_cast<IByteArray::Byte*>(&netWord), 2);
    _offset += 2;
    return *this;
}

Writer& Writer::operator << (const dint32& dword)
{
    return *this << static_cast<duint32>(dword);
}   

Writer& Writer::operator << (const duint32& dword)
{
    duint32 netDword;
    _convert.nativeToForeign(dword, netDword);
    _destination.set(_fixedOffset + _offset, reinterpret_cast<IByteArray::Byte*>(&netDword), 4);
    _offset += 4;
    return *this;
}

Writer& Writer::operator << (const dint64& qword)
{
    return *this << static_cast<duint64>(qword);
}   

Writer& Writer::operator << (const duint64& qword)
{
    duint64 netQword;
    _convert.nativeToForeign(qword, netQword);
    _destination.set(_fixedOffset + _offset, reinterpret_cast<IByteArray::Byte*>(&netQword), 8);
    _offset += 8;
    return *this;
}

Writer& Writer::operator << (const dfloat& value)
{
    return *this << *reinterpret_cast<const duint32*>(&value);
}

Writer& Writer::operator << (const ddouble& value)
{
    return *this << *reinterpret_cast<const duint64*>(&value);
}

Writer& Writer::operator << (const String& text)
{
    Block bytes = text.toUtf8();

    // First write the length of the text.
    duint size = bytes.size();
    *this << size;

    _destination.set(_fixedOffset + _offset,
                     bytes.data(),
                     size);
    _offset += size;
    return *this;
}

Writer& Writer::operator << (const IByteArray& byteArray)
{
    // First write the length of the array.
    return *this << duint32(byteArray.size()) << FixedByteArray(byteArray);
}

Writer& Writer::operator << (const FixedByteArray& fixedByteArray)
{
    /**
     * @note  A copy of @a fixedByteArray is done because there is no certainty
     * that the source data actually exists anywhere. The object implementing
     * IByteArray could be generating it on the fly.
     */
    
    // Read the entire contents of the array.
    const dsize size = fixedByteArray.size();
    QScopedPointer<IByteArray::Byte> data(new IByteArray::Byte[size]);
    fixedByteArray.get(0, data.data(), size);
    _destination.set(_fixedOffset + _offset, data.data(), size);
    _offset += size;
    return *this;
}

Writer& Writer::operator << (const Block& block)
{
    // First write the length of the block.
    duint size = block.size();
    *this << size;

    _destination.set(_fixedOffset + _offset, block.data(), size);
    _offset += size;
    return *this;
}

Writer& Writer::operator << (const IWritable& writable)
{
    writable >> *this;
    return *this;
}

void Writer::seek(dint count)
{
    if(dint(_fixedOffset + _offset) + count < 0)
    {
        throw IByteArray::OffsetError("Writer::seek", "Seek past beginning of destination");
    }
    _offset += count;
}
