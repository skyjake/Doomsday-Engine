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

#include "de/writer.h"
#include "de/string.h"
#include "de/block.h"
#include "de/iserializable.h"
#include "de/iostream.h"
#include "de/fixedbytearray.h"
#include "de/byterefarray.h"
#include "de/bytearrayfile.h"
#include "de/byteorder.h"

namespace de {

DE_PIMPL_NOREF(Writer)
{
    const ByteOrder &convert;
    IByteArray *destination = nullptr;
    IOStream *stream = nullptr;
    IByteArray::Offset offset = 0;
    IByteArray::Offset markOffset = 0;
    IByteArray::Offset const fixedOffset;

    Impl(const ByteOrder &order, IByteArray *dest, IByteArray::Offset off)
        : convert(order), destination(dest), offset(off), fixedOffset(0) {}

    Impl(const ByteOrder &order, IOStream *str)
        : convert(order), stream(str), fixedOffset(0)
    {
        destination = dynamic_cast<IByteArray *>(str);
        if (destination)
        {
            // The object that implements the stream may also implement a byte
            // array interface -- that provides us more freedom to write, so
            // prefer to use it instead.
            stream = nullptr;
        }
    }

    Impl(const Impl &other, const ByteOrder &order)
        : convert    (order            ),
          destination(other.destination),
          stream     (other.stream     ),
          offset     (other.offset     ),
          fixedOffset(other.fixedOffset)
    {}
    
    ~Impl()
    {
        if (File *f = maybeAs<File>(destination))
        {
            f->release();
        }
    }

    void write(const IByteArray::Byte *ptr, dsize size)
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

Writer::Writer(IByteArray &destination, const ByteOrder &byteOrder, IByteArray::Offset offset)
    : d(new Impl(byteOrder, &destination, offset))
{}

Writer::Writer(IByteArray &destination, IByteArray::Offset offset)
    : d(new Impl(littleEndianByteOrder, &destination, offset))
{}

Writer::Writer(IOStream &stream, const ByteOrder &byteOrder)
    : d(new Impl(byteOrder, &stream))
{}

Writer::Writer(ByteArrayFile &destination, const ByteOrder &byteOrder, IByteArray::Offset offset)
    : d(new Impl(byteOrder, static_cast<IByteArray *>(&destination), offset))
{}

Writer::Writer(const Writer &other)
    : d(new Impl(*other.d, other.d->convert))
{}

Writer::Writer(const Writer &other, const ByteOrder &byteOrder)
    : d(new Impl(*other.d, byteOrder))
{}

Writer &Writer::withHeader()
{
    // Currently the header is very simple.
    return *this << duint32(version());
}

duint Writer::version() const
{
    return DE_PROTOCOL_LATEST;
}

Writer &Writer::operator << (const char &byte)
{
    d->write(reinterpret_cast<const IByteArray::Byte *>(&byte), 1);
    return *this;
}

Writer &Writer::operator << (const dchar &byte)
{
    d->write(reinterpret_cast<const IByteArray::Byte *>(&byte), 1);
    return *this;
}

Writer &Writer::operator << (const duchar &byte)
{
    d->write(&byte, 1);
    return *this;
}

Writer &Writer::operator << (const dint16 &word)
{
    return *this << static_cast<duint16>(word);
}

Writer &Writer::operator << (const duint16 &word)
{
    duint16 netWord;
    d->convert.hostToNetwork(word, netWord);
    d->write(reinterpret_cast<IByteArray::Byte *>(&netWord), 2);
    return *this;
}

Writer &Writer::operator << (const dint32 &dword)
{
    return *this << static_cast<duint32>(dword);
}

Writer &Writer::operator << (const duint32 &dword)
{
    duint32 netDword;
    d->convert.hostToNetwork(dword, netDword);
    d->write(reinterpret_cast<IByteArray::Byte *>(&netDword), 4);
    return *this;
}

Writer &Writer::operator << (const dint64 &qword)
{
    return *this << static_cast<duint64>(qword);
}

Writer &Writer::operator << (const duint64 &qword)
{
    duint64 netQword;
    d->convert.hostToNetwork(qword, netQword);
    d->write(reinterpret_cast<IByteArray::Byte *>(&netQword), 8);
    return *this;
}

Writer &Writer::operator << (const dfloat &value)
{
    duint32 tmp;
    static_assert(sizeof(tmp) == sizeof(value), "Size of float expected to match uint32");
    memcpy(&tmp, &value, 4);
    return *this << tmp;
}

Writer &Writer::operator << (const ddouble &value)
{
    duint64 tmp;
    static_assert(sizeof(tmp) == sizeof(value), "Size of double expected to match uint64");
    memcpy(&tmp, &value, 8);
    return *this << tmp;
}

Writer &Writer::operator << (const String &text)
{
    Block bytes = text;

    // First write the length of the text.
    duint32 size = bytes.size();
    *this << size;

    d->write(bytes.data(), size);
    return *this;
}

Writer &Writer::operator << (const Block &block)
{
    return *this << static_cast<const IWritable &>(block);
}

Writer &Writer::writeText(const String &text)
{
    const Block utf8 = text.toUtf8();
    d->write(utf8.cdata(), utf8.size());
    return *this;
}

Writer &Writer::operator << (const IByteArray &byteArray)
{
    // First write the length of the array.
    return *this << duint32(byteArray.size()) << FixedByteArray(byteArray);
}

Writer &Writer::operator << (const FixedByteArray &fixedByteArray)
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

Writer &Writer::writeBytes(dsize count, const IByteArray &array)
{
    return *this << FixedByteArray(array, 0, count);
}

Writer &Writer::writeBytes(const IByteArray &array)
{
    return *this << FixedByteArray(array);
}

Writer &Writer::operator << (const IWritable &writable)
{
    writable >> *this;
    return *this;
}

const IByteArray *Writer::destination() const
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
    const auto oldOffset = d->offset;
    const duint32 delta = duint32(d->offset - d->markOffset) - 4;
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

const ByteOrder &Writer::byteOrder() const
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
