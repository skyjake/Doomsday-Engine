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

#ifndef LIBDENG2_WRITER_H
#define LIBDENG2_WRITER_H

#include "../libdeng2.h"
#include "../IByteArray"
#include "../ByteOrder"

#include <algorithm> // std::swap

namespace de {

class IWritable;
class String;
class Block;
class ByteArrayFile;
class FixedByteArray;
class IOStream;

/**
 * Provides a protocol for writing data in a specific byte order into a byte
 * array object (anything with a IByteArray interface). Defaults to
 * little-endian byte order.
 *
 * Note about versioning: when instructed to include a header, Writer always
 * uses the latest version of serialization.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Writer
{
public:
    /// Seeking is not possible, e.g., when writing to a stream. @ingroup errors
    DENG2_ERROR(SeekError);

public:
    /**
     * Constructs a new writer.
     *
     * @param destination  Byte array to write to.
     * @param byteOrder    Byte order to use. The byte order defaults to little-endian byte order.
     * @param offset       Offset in @a destination where to start writing.
     */
    Writer(IByteArray &destination, ByteOrder const &byteOrder = littleEndianByteOrder,
           IByteArray::Offset offset = 0);

    /**
     * Constructs a new writer (with little-endian byte order).
     *
     * @param destination  Byte array to write to.
     * @param offset       Offset in @a destination where to start writing.
     */
    Writer(IByteArray &destination, IByteArray::Offset offset);

    /**
     * Constructs a new writer for writing to an I/O stream.
     *
     * @param stream     Stream to write to.
     * @param byteOrder  Byte order to use.
     */
    Writer(IOStream &stream, ByteOrder const &byteOrder = littleEndianByteOrder);

    /**
     * Constructs a new writer for writing into a byte array file.
     *
     * @param destination  Byte array file to write to.
     * @param byteOrder    Byte order to use.
     * @param offset       Offset in @a destination where to start writing.
     */
    Writer(ByteArrayFile &destination, ByteOrder const &byteOrder = littleEndianByteOrder,
           IByteArray::Offset offset = 0);

    /**
     * Copy constructor.
     *
     * @param other  Writer.
     */
    Writer(Writer const &other);

    /**
     * Constructs a new writer that uses the current offset of @a other as its
     * zero offset.
     *
     * @param other      Writer.
     * @param byteOrder  Byte order.
     */
    Writer(Writer const &other, ByteOrder const &byteOrder);

    /**
     * Writes the serialization protocol header to the destination. This should
     * be used when the Writer is responsible for the containment stream
     * itself; a Reader can then reader the header and use the correct protocol
     * version.
     *
     * @return Reference to this Writer.
     */
    Writer &withHeader();

    /**
     * Returns the protocol version used by the Writer.
     * @see de::ProtocolVersion
     */
    duint version() const;

    //@{ Write a number to the destination buffer, in the chosen byte order.
    Writer &operator << (char const &byte);
    Writer &operator << (dchar const &byte);
    Writer &operator << (duchar const &byte);
    Writer &operator << (dint16 const &word);
    Writer &operator << (duint16 const &word);
    Writer &operator << (dint32 const &dword);
    Writer &operator << (duint32 const &dword);
    Writer &operator << (dint64 const &qword);
    Writer &operator << (duint64 const &qword);
    Writer &operator << (dfloat const &value);
    Writer &operator << (ddouble const &value);
    //@}

    /// Write a string to the destination buffer.
    Writer &operator << (String const &text);

    /// Writes a sequence bytes to the destination buffer.
    Writer &operator << (IByteArray const &byteArray);

    /**
     * Writes a fixed-size sequence of bytes to the destination buffer.
     * The size of the sequence is not included in the written data.
     * When reading, the reader must know the size beforehand.
     * @see Reader::operator >> (FixedByteArray &fixedByteArray)
     *
     * @param fixedByteArray  Data to write.
     *
     * @return  Reference to the Writer.
     */
    Writer &operator << (FixedByteArray const &fixedByteArray);

    /**
     * Writes @a block into the destination buffer. Writes the size of the
     * block in addition to its contents, so a Reader will not need to know
     * beforehand how large the block is.
     *
     * @param block  Block to write.
     *
     * @return  Reference to the Writer.
     */
    Writer &operator << (Block const &block);

    /// Writes a writable object into the destination buffer.
    Writer &operator << (IWritable const &writable);

    /**
     * Writes a list of objects. ListType is expected to be an iterable
     * list containing pointers to IWritable objects.
     *
     * @param list  List of objects.
     */
    template <typename ListType>
    Writer &writeObjects(ListType const &list) {
        *this << duint32(list.size());
        DENG2_FOR_EACH_CONST(typename ListType, i, list) *this << **i;
        return *this;
    }

    /**
     * Returns the destination byte array used by the writer.
     */
    IByteArray const *destination() const;

    /**
     * Returns the destination byte array used by the writer.
     */
    IByteArray *destination();

    /**
     * Returns the offset used by the writer.
     */
    IByteArray::Offset offset() const;

    void setOffset(IByteArray::Offset offset);

    /**
     * Returns the byte order of the writer.
     */
    ByteOrder const &byteOrder() const;

    /**
     * Moves the writer offset forward by a number of bytes.
     *
     * @param count Number of bytes to move forward (negative to move backward).
     */
    void seek(dint count);

    inline void swap(Writer &other) {
        d.swap(other.d);
    }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG2_WRITER_H */
