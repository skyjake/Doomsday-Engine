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

#ifndef LIBCORE_READER_HH
#define LIBCORE_READER_HH

#include "de/libcore.h"
#include "de/ibytearray.h"
#include "de/byteorder.h"

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
 * When a Reader is destroyed, the source file is automatically released,
 * if there is one.
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
 * 5. Reader is the standard way to read all data in libcore.
 *
 * @ingroup data
 */
class DE_PUBLIC Reader
{
public:
    /// Seeking is not possible, e.g., when reading from a stream. @ingroup errors
    DE_ERROR(SeekError);

    /// The serialization protocol version specified is unknown. @ingroup errors
    DE_ERROR(VersionError);

public:
    /**
     * Copy constructor.
     *
     * @param other  Reader.
     */
    Reader(const Reader &other);

    /**
     * Constructs a new reader.
     *
     * @param source     Byte array containing the data to be read.
     * @param byteOrder  Byte order to use.
     * @param offset     Offset in @a source where to start reading.
     */
    Reader(const IByteArray &source, const ByteOrder &byteOrder = littleEndianByteOrder,
           IByteArray::Offset offset = 0);

    /**
     * Constructs a new reader that reads from a mutable stream. The bytes are
     * expected to be removed from the stream once read.
     *
     * @param stream     Stream where input is read.
     * @param byteOrder  Byte order to use.
     */
    Reader(IIStream &stream, const ByteOrder &byteOrder = littleEndianByteOrder);

    /**
     * Constructs a new reader that reads from an immutable stream. The bytes
     * remain in the stream.
     *
     * @param stream     Const stream where input is read.
     * @param byteOrder  Byte order to use.
     */
    Reader(const IIStream &stream, const ByteOrder &byteOrder = littleEndianByteOrder);

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

    Reader &operator >> (Block &block);

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

    /**
     * Reads a specific number of bytes and puts it to a destination
     * byte array.
     *
     * @param count        Number of bytes to read.
     * @param destination  Destination array.
     *
     * @return Reference to the Reader.
     */
    Reader &readBytes(dsize count, IByteArray &destination);

    /**
     * Reads a fixed number of bytes and puts them into a destination
     * byte array. The complete @a destination is filled with new bytes;
     * its size won't change.
     *
     * @param destination  Destination array. The size of this array
     *                     determines how many bytes to read.
     * @return Reference to the Reader.
     */
    Reader &readBytesFixedSize(IByteArray &destination);

    /// Reads a serializable object from the source buffer.
    Reader &operator >> (IReadable &readable);

    /**
     * Reads a list of objects. ListType is expected to be an iterable
     * list containing pointers to IReadable objects. The list will
     * own the read instances.
     *
     * @param list  List of objects.
     */
    template <typename ObjectType, typename ListType>
    Reader &readObjects(ListType &list) {
        duint32 count;
        *this >> count;
        while (count-- > 0) {
            std::unique_ptr<ObjectType> entry(new ObjectType);
            *this >> *entry;
            list.push_back(entry.release());
        }
        return *this;
    }

    template <typename ObjectType, typename ListType>
    Reader &readObjects(ListType &list,
                        std::function<ObjectType * ()> constructor) {
        duint32 count;
        *this >> count;
        while (count-- > 0) {
            std::unique_ptr<ObjectType> entry(constructor());
            *this >> *entry;
            list.push_back(entry.release());
        }
        return *this;
    }

    template <typename ListType>
    Reader &readElements(ListType &list) {
        duint32 count;
        *this >> count;
        while (count-- > 0) {
            typename ListType::value_type elem;
            *this >> elem;
            list.push_back(elem);
        }
        return *this;
    }

    /**
     * Reads something from the source and converts it to another type before
     * assigning to the destination. Use this for instance when reading an
     * enumerated type that has been written as an integer.
     *
     * @param value  Value to read.
     */
    template <typename SerializedType, typename Type>
    Reader &readAs(Type &value) {
        SerializedType t;
        *this >> t;
        value = Type(t);
        return *this;
    }

    /**
     * Reads bytes from the source buffer until a specified delimiter value is
     * encountered. The delimiter is included as part of the read data. The end
     * of the source data is also considered a valid delimiter; no exception
     * will be thrown if the source data ends.
     *
     * @param byteArray      Destination buffer.
     * @param delimiter      Delimiter value.
     * @param keepDelimiter  Delimiter is kept at the end of the buffer.
     *                       If @c false, @a byteArray will not end with
     *                       the delimiter.
     */
    Reader &readUntil(IByteArray &     byteArray,
                      IByteArray::Byte delimiter     = 0,
                      bool             keepDelimiter = true);

    /**
     * Reads a line of text ending in a @c \\n character. The source data is
     * expected to be UTF-8 encoded text. All carriage returns (@c \\r) are
     * removed from the string.
     *
     * @param string  The read line is returned here. It includes the
     *                terminating newline character.
     */
    Reader &readLine(String &string);

    /**
     * Equivalent to readLine(String), but returns the read string.
     */
    String readLine();

    /**
     * Returns the source byte array of the reader.
     */
    const IByteArray *source() const;

    /**
     * Determines if the reader's position is at the end of the source data;
     * i.e., there is nothing more to read, and attempting to do so would
     * produce an exception.
     *
     * @return @c true, iff at end of source.
     */
    bool atEnd() const;

    /**
     * Returns the offset used by the reader.
     */
    IByteArray::Offset offset() const;

    /**
     * Returns the number of bytes still available for reading.
     * @return Byte count.
     */
    IByteArray::Size remainingSize() const;

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
    void seek(IByteArray::Delta count);

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
    const ByteOrder &byteOrder() const;

    inline void swap(Reader &other) {
        d.swap(other.d);
    }

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_READER_HH */
