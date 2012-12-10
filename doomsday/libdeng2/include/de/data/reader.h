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

#ifndef LIBDENG2_READER_HH
#define LIBDENG2_READER_HH

#include "../libdeng2.h"
#include "../IByteArray"
#include "../ByteOrder"

#include <algorithm>

namespace de {

class Block;
class String;
class IReadable;
class FixedByteArray;
class IIStream;

/**
 * Provides a protocol for reading data from a byte array object (anything with
 * a IByteArray or IIStream interface). Byte order defaults to little-endian
 * but can be changed to big-endian.
 *
 * Note about versioning: readers must be prepared to support old versions of
 * the serialization protocol in addition to the latest one for backwards
 * compatibility.
 *
 * When there is need to deserialize data, generally it is preferable to use
 * Reader (or IReadable) for this purpose in public interfaces:
 *
 * 1. Reader maintains the position in a longer data stream and can be rewound
 *    when needed,
 * 2. the source can be an input stream in addition to a IByteArray,
 * 3. Reader knows the serialization version number,
 * 4. Reader knows the byte order, which can be verified using appropriate
 *    means (assertion/exception), and
 * 5. Reader is the standard way to read all data in libdeng2.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Reader
{
public:
    /// Seeking is not possible, e.g., when reading from a stream. @ingroup errors
    DENG2_ERROR(SeekError);

    /// The serialization protocol version specified is unknown. @ingroup errors
    DENG2_ERROR(VersionError);

public:
    /**
     * Copy constructor.
     *
     * @param other  Reader.
     */
    Reader(Reader const &other);

    /**
     * Constructs a new reader.
     *
     * @param source     Byte array containing the data to be read.
     * @param byteOrder  Byte order to use.
     * @param offset     Offset in @a source where to start reading.
     */
    Reader(IByteArray const &source, ByteOrder const &byteOrder = littleEndianByteOrder,
           IByteArray::Offset offset = 0);

    /**
     * Constructs a new reader that reads from a mutable stream. The bytes are
     * expected to be removed from the stream once read.
     *
     * @param stream     Stream where input is read.
     * @param byteOrder  Byte order to use.
     */
    Reader(IIStream &stream, ByteOrder const &byteOrder = littleEndianByteOrder);

    /**
     * Constructs a new reader that reads from an immutable stream. The bytes
     * remain in the stream.
     *
     * @param stream     Const stream where input is read.
     * @param byteOrder  Byte order to use.
     */
    Reader(IIStream const &stream, ByteOrder const &byteOrder = littleEndianByteOrder);

    virtual ~Reader();

    /**
     * Reads the serialization protocol header from the source. The header is
     * read at the current read offset. The version can be then queried with
     * version().
     *
     * @return Reference to this Reader.
     */
    Reader &withHeader();

    /**
     * Returns the serialization protocol used by the reader. If no header has been
     * included, defaults to the latest version.
     *
     * @return de::ProtocolVersion.
     */
    duint version() const;

    /**
     * Changes the serialization protocol version used by the reader. Usually calling
     * this manually is not necessary.
     *
     * @param version  Serialization protocol version number.
     */
    void setVersion(duint version);

    //@{ Read a number from the source buffer, in network byte order.
    Reader &operator >> (char &byte);
    Reader &operator >> (dchar &byte);
    Reader &operator >> (duchar &byte);
    Reader &operator >> (dint16 &word);
    Reader &operator >> (duint16 &word);
    Reader &operator >> (dint32 &dword);
    Reader &operator >> (duint32 &dword);
    Reader &operator >> (dint64 &qword);
    Reader &operator >> (duint64 &qword);
    Reader &operator >> (dfloat &value);
    Reader &operator >> (ddouble &value);
    //@}

    /// Reads a string from the source buffer.
    Reader &operator >> (String &text);

    /// Reads a sequence bytes from the source buffer.
    Reader &operator >> (IByteArray &byteArray);

    /**
     * Reads a fixed-size sequence of bytes from the source buffer.
     * The size of the sequence is determined by the size of
     * @a fixedByteArray.
     *
     * @param fixedByteArray  Destination buffer.
     *
     * @return  Reference to the Reader.
     */
    Reader &operator >> (FixedByteArray &fixedByteArray);

    /// Reads a Block from the source buffer.
    Reader &operator >> (Block &block);

    /// Reads a serializable object from the source buffer.
    Reader &operator >> (IReadable &readable);

    /**
     * Reads bytes from the source buffer until a specified delimiter
     * value is encountered. The delimiter is included as part of
     * the read data.
     *
     * @param byteArray  Destination buffer.
     * @param delimiter  Delimiter value.
     */
    Reader &readUntil(IByteArray &byteArray, IByteArray::Byte delimiter = 0);

    /**
     * Returns the source byte array of the reader.
     */
    IByteArray const *source() const;

    /**
     * Returns the offset used by the reader.
     */
    IByteArray::Offset offset() const;

    /**
     * Move to a specific position in the source data.
     *
     * @param offset  Offset to move to.
     */
    void setOffset(IByteArray::Offset offset);

    /**
     * Moves the reader offset forward by a number of bytes. This is a random
     * access seek: it is only possible if the source supports random access
     * (e.g., it is impossible to seek in streams).
     *
     * @param count  Number of bytes to move forward. Negative values move
     *               the reader offset backward.
     */
    void seek(dint count);

    /**
     * Marks the current position for rewinding later. After setting the mark,
     * you are expected to call rewind() to return to the marked position. This
     * method can be used even when reading from streams.
     */
    void mark();

    /**
     * Rewinds the read offset to the mark set previously (using mark()).
     * Rewinding can be done with all readers, regardless of where the data
     * comes from.
     */
    void rewind();

    /**
     * Returns the byte order of the writer.
     */
    ByteOrder const &byteOrder() const;

    inline void swap(Reader &other) {
        std::swap(d, other.d);
    }

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif /* LIBDENG2_READER_HH */
