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

#include "de/Writer"
#include "de/String"
#include "de/Block"
#include "de/ISerializable"
#include "de/IOStream"
#include "de/FixedByteArray"
#include "de/ByteRefArray"
#include "de/ByteArrayFile"
#include "de/data/byteorder.h"

#include <QScopedPointer>

namespace de {

DENG2_PIMPL_NOREF(Writer)
{
    ByteOrder const &convert;
    IByteArray *destination;
    IOStream *stream;
    IByteArray::Offset offset;
    IByteArray::Offset markOffset = 0;
    IByteArray::Offset const fixedOffset;

    Impl(ByteOrder const &order, IByteArray *dest, IByteArray::Offset off)
        : convert(order), destination(dest), stream(0), offset(off), fixedOffset(0) {}

    Impl(ByteOrder const &order, IOStream *str)
        : convert(order), destination(0), stream(str), offset(0), fixedOffset(0)
    {
        destination = dynamic_cast<IByteArray *>(str);
        if (destination)
        {
            // The object that implements the stream may also implement a byte
            // array interface -- that provides us more freedom to write, so
            // prefer to use it instead.
            stream = 0;
        }
    }

    Impl(Impl const &other, ByteOrder const &order)
        : convert    (order            ),
          destination(other.destination),
          stream     (other.stream     ),
          offset     (other.offset     ),
          fixedOffset(other.fixedOffset)
    {}

    void write(IByteArray::Byte const *ptr, dsize size)
    {
        if (destination)
        {
            destination->set(fixedOffset + offset, ptr, size);
            offset += size;
        }
        else if (stream)
        {
            *stream << ByteRefArray(ptr, size);
        }
    }
};

Writer::Writer(IByteArray &destination, ByteOrder const &byteOrder, IByteArray::Offset offset)
    : d(new Impl(byteOrder, &destination, offset))
{}

Writer::Writer(IByteArray &destination, IByteArray::Offset offset)
    : d(new Impl(littleEndianByteOrder, &destination, offset))
{}

Writer::Writer(IOStream &stream, ByteOrder const &byteOrder)
    : d(new Impl(byteOrder, &stream))
{}

Writer::Writer(ByteArrayFile &destination, ByteOrder const &byteOrder, IByteArray::Offset offset)
    : d(new Impl(byteOrder, static_cast<IByteArray *>(&destination), offset))
{}

Writer::Writer(Writer const &other)
    : d(new Impl(*other.d, other.d->convert))
{}

Writer::Writer(Writer const &other, ByteOrder const &byteOrder)
    : d(new Impl(*other.d, byteOrder))
{}

Writer &Writer::withHeader()
{
    // Currently the header is very simple.
    return *this << duint32(version());
}

duint Writer::version() const
{
    return DENG2_PROTOCOL_LATEST;
}

Writer &Writer::operator << (char const &byte)
{
    d->write(reinterpret_cast<IByteArray::Byte const *>(&byte), 1);
    return *this;
}

Writer &Writer::operator << (dchar const &byte)
{
    d->write(reinterpret_cast<IByteArray::Byte const *>(&byte), 1);
    return *this;
}

Writer &Writer::operator << (duchar const &byte)
{
    d->write(&byte, 1);
    return *this;
}

Writer &Writer::operator << (dint16 const &word)
{
    return *this << static_cast<duint16>(word);
}

Writer &Writer::operator << (duint16 const &word)
{
    duint16 netWord;
    d->convert.hostToNetwork(word, netWord);
    d->write(reinterpret_cast<IByteArray::Byte *>(&netWord), 2);
    return *this;
}

Writer &Writer::operator << (dint32 const &dword)
{
    return *this << static_cast<duint32>(dword);
}

Writer &Writer::operator << (duint32 const &dword)
{
    duint32 netDword;
    d->convert.hostToNetwork(dword, netDword);
    d->write(reinterpret_cast<IByteArray::Byte *>(&netDword), 4);
    return *this;
}

Writer &Writer::operator << (dint64 const &qword)
{
    return *this << static_cast<duint64>(qword);
}

Writer &Writer::operator << (duint64 const &qword)
{
    duint64 netQword;
    d->convert.hostToNetwork(qword, netQword);
    d->write(reinterpret_cast<IByteArray::Byte *>(&netQword), 8);
    return *this;
}

Writer &Writer::operator << (dfloat const &value)
{
    return *this << *reinterpret_cast<duint32 const *>(&value);
}

Writer &Writer::operator << (ddouble const &value)
{
    return *this << *reinterpret_cast<duint64 const *>(&value);
}

Writer &Writer::operator << (String const &text)
{
    Block bytes = text.toUtf8();

    // First write the length of the text.
    duint32 size = bytes.size();
    *this << size;

    d->write(bytes.data(), size);
    return *this;
}

Writer &Writer::writeText(String const &text)
{
    Block const utf8 = text.toUtf8();
    d->write(utf8.dataConst(), utf8.size());
    return *this;
}

Writer &Writer::operator << (IByteArray const &byteArray)
{
    // First write the length of the array.
    return *this << duint32(byteArray.size()) << FixedByteArray(byteArray);
}

Writer &Writer::operator << (FixedByteArray const &fixedByteArray)
{
    /**
     * @note  A copy of @a fixedByteArray is done because there is no certainty
     * that the source data actually exists anywhere. The object implementing
     * IByteArray could be generating it on the fly.
     */

    // Read the entire contents of the array.
    Block copy(fixedByteArray);
    d->write(copy.data(), copy.size());
    return *this;
}

Writer &Writer::writeBytes(dsize count, IByteArray const &array)
{
    return *this << FixedByteArray(array, 0, count);
}

Writer &Writer::writeBytes(IByteArray const &array)
{
    return *this << FixedByteArray(array);
}

Writer &Writer::operator << (Block const &block)
{
    // First write the length of the block.
    duint32 size = block.size();
    *this << size;

    d->write(block.data(), size);
    return *this;
}

Writer &Writer::operator << (IWritable const &writable)
{
    writable >> *this;
    return *this;
}

IByteArray const *Writer::destination() const
{
    return d->destination;
}

IByteArray *Writer::destination()
{
    return d->destination;
}

IByteArray::Offset Writer::offset() const
{
    return d->offset;
}

void Writer::setOffset(IByteArray::Offset offset)
{
    if (d->stream)
    {
        throw SeekError("Writer::setOffset", "Cannot change offset when writing to a stream");
    }
    d->offset = offset;
}

void Writer::mark()
{
    if (d->stream)
    {
        throw SeekError("Writer::mark", "Marking positions is not possible in a stream");
    }
    d->markOffset = d->offset;
}

void Writer::rewind()
{
    setOffset(d->markOffset);
}

Writer &Writer::beginIndeterminateSpan()
{
    mark();
    *this << duint32(0);
    return *this;
}

Writer &Writer::endIndeterminateSpan()
{
    if (d->stream)
    {
        throw SeekError("Writer::endIndeterminateSpan", "Not possible in a stream");
    }
    auto const oldOffset = d->offset;
    duint32 const delta = duint32(d->offset - d->markOffset) - 4;
    rewind();
    *this << delta;
    setOffset(oldOffset);
    return *this;
}

Writer &Writer::operator << (InlineOperation op)
{
    switch (op)
    {
    case BeginSpan:
        beginIndeterminateSpan();
        break;

    case EndSpan:
        endIndeterminateSpan();
        break;
    }
    return *this;
}

ByteOrder const &Writer::byteOrder() const
{
    return d->convert;
}

void Writer::seek(IByteArray::Delta count)
{
    if (d->stream)
    {
        throw SeekError("Writer::seek", "Cannot seek when writing to a stream");
    }

    if (IByteArray::Delta(d->fixedOffset + d->offset) + count < 0)
    {
        throw IByteArray::OffsetError("Writer::seek", "Seek past beginning of destination");
    }
    d->offset = IByteArray::Offset(IByteArray::Delta(d->offset) + count);
}

void Writer::seekToEnd()
{
    if (d->destination)
    {
        setOffset(d->destination->size() - d->fixedOffset);
    }
}

} // namespace de
