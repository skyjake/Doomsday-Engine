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

#include "de/Reader"
#include "de/String"
#include "de/Block"
#include "de/ISerializable"
#include "de/FixedByteArray"
#include "de/data/byteorder.h"

#include <QTextStream>

using namespace de;

Reader::Reader(const IByteArray& source, const ByteOrder& byteOrder, IByteArray::Offset offset)
    : _source(source), _offset(offset), _convert(byteOrder)
{}

Reader& Reader::operator >> (char& byte)
{
    return *this >> reinterpret_cast<duchar&>(byte);
}

Reader& Reader::operator >> (dchar& byte)
{
    return *this >> reinterpret_cast<duchar&>(byte);
}

Reader& Reader::operator >> (duchar& byte)
{
    _source.get(_offset, &byte, 1);
    ++_offset;
    return *this;
}

Reader& Reader::operator >> (dint16& word)
{
    return *this >> reinterpret_cast<duint16&>(word);
}

Reader& Reader::operator >> (duint16& word)
{
    _source.get(_offset, reinterpret_cast<IByteArray::Byte*>(&word), 2);
    _offset += 2;
    _convert.foreignToNative(word, word);
    return *this;
}

Reader& Reader::operator >> (dint32& dword)
{
    return *this >> reinterpret_cast<duint32&>(dword);
}

Reader& Reader::operator >> (duint32& dword)
{
    _source.get(_offset, reinterpret_cast<IByteArray::Byte*>(&dword), 4);
    _offset += 4;
    _convert.foreignToNative(dword, dword);
    return *this;
}

Reader& Reader::operator >> (dint64& qword)
{
    return *this >> reinterpret_cast<duint64&>(qword);
}

Reader& Reader::operator >> (duint64& qword)
{
    _source.get(_offset, reinterpret_cast<IByteArray::Byte*>(&qword), 8);
    _offset += 8;
    _convert.foreignToNative(qword, qword);
    return *this;
}

Reader& Reader::operator >> (dfloat& value)
{
    return *this >> *reinterpret_cast<duint32*>(&value);
}

Reader& Reader::operator >> (ddouble& value)
{
    return *this >> *reinterpret_cast<duint64*>(&value);
}

Reader& Reader::operator >> (String& text)
{
    duint size = 0;
    *this >> size;

    Block bytes;
    for(duint i = 0; i < size; ++i)
    {
        IByteArray::Byte ch = 0;
        *this >> ch;
        bytes.append(ch);
    }
    text = String::fromUtf8(bytes);
    
    return *this;
}

Reader& Reader::operator >> (IByteArray& byteArray)
{
    duint size = 0;
    *this >> size;

    /**
     * @note  A temporary copy of the contents of the array is made
     * because the destination byte array is not guaranteed to be
     * a memory buffer where you can copy the contents directly.
     */
    QScopedPointer<IByteArray::Byte> data(new IByteArray::Byte[size]);
    _source.get(_offset, data.data(), size);
    _offset += size;
    byteArray.set(0, data.data(), size);
    return *this;
}

Reader& Reader::operator >> (FixedByteArray& fixedByteArray)
{
    /**
     * @note  A temporary copy of the contents of the array is made
     * because the destination byte array is not guaranteed to be
     * a memory buffer where you can copy the contents directly.
     */
    const dsize size = fixedByteArray.size();
    QScopedPointer<IByteArray::Byte> data(new IByteArray::Byte[size]);
    _source.get(_offset, data.data(), size);
    _offset += size;
    fixedByteArray.set(0, data.data(), size);
    return *this;
}

Reader& Reader::operator >> (Block& block)
{
    duint size = 0;
    *this >> size;

    block.copyFrom(_source, _offset, size);
    _offset += size;
    return *this;
}

Reader& Reader::operator >> (IReadable& readable)
{
    readable << *this;
    return *this;
}

Reader& Reader::readUntil(IByteArray& byteArray, IByteArray::Byte delimiter)
{
    int pos = 0;
    IByteArray::Byte b = 0;
    do {
        *this >> b;
        byteArray.set(pos++, &b, 1);
    } while(b != delimiter);
    return *this;
}

void Reader::seek(dint count)
{
    if(IByteArray::Offset(_offset + count) >= _source.size())
    {
        throw IByteArray::OffsetError("Reader::seek", "Seek past bounds of source data");
    }
    _offset += count;
}

void Reader::rewind(dint count)
{
    if(IByteArray::Offset(_offset - count) >= _source.size())
    {
        QString msg;
        QTextStream os(&msg);
        os << "(count: " << count << ", offset: " << _offset << ", size: " << _source.size() << ")";
        throw IByteArray::OffsetError("Reader::rewind", "Rewound past bounds of source data " + msg);
    }
    _offset -= count;
}
