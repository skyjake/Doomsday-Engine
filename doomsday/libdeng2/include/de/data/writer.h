/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
     * Constructs a new writer that uses the current offset of @a other as its
     * zero offset.
     *
     * @param other      Writer.
     * @param byteOrder  Byte order. Defaults to little-endian.
     */
    Writer(Writer const &other, ByteOrder const &byteOrder = littleEndianByteOrder);

    virtual ~Writer();

    //@{ Write a number to the destination buffer, in the chosen byte order.
    Writer &operator << (char const &byte);
    Writer &operator << (dchar const &byte);
    Writer &operator << (duchar const &byte);
    Writer &operator << (const dint16& word);
    Writer &operator << (const duint16& word);
    Writer &operator << (const dint32& dword);
    Writer &operator << (const duint32& dword);
    Writer &operator << (const dint64& qword);
    Writer &operator << (const duint64& qword);
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

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif /* LIBDENG2_WRITER_H */
