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

#ifndef LIBCORE_BLOCK_H
#define LIBCORE_BLOCK_H

#include "../IByteArray"
#include "../IBlock"
#include "../ISerializable"
#include "../List"
#include "../Writer"

#include <c_plus/block.h>
#include <array>

namespace de {

class String;
class IIStream;

/**
 * Data buffer that implements the byte array interface.
 *
 * Note that Block is based on QByteArray, and thus automatically always ensures
 * that the data is followed by a terminating \0 character (even if one is not
 * part of the actual Block contents). Therefore it is safe to use it in functions
 * that assume zero-terminated strings.
 *
 * @ingroup data
 */
class DE_PUBLIC Block
    : public IByteArray
    , public IBlock
    , public ISerializable
{
public:
    Block(Size initialSize = 0);
    Block(const iBlock *);
    Block(const IByteArray &array);
    Block(const Block &other);
    Block(Block &&moved);
    Block(const char *nullTerminatedCStr);
    Block(const void *data, Size length);

    /**
     * Constructs a block by reading the contents of an input stream. The block
     * will contain all the data that is available immediately; will not wait
     * for additional data to become available later.
     *
     * @param stream  Stream to read from.
     */
    Block(IIStream &stream);

    /**
     * Constructs a block by reading the contents of an input stream in const
     * mode. The block will contain all the data that is available immediately;
     * will not wait for additional data to become available later.
     *
     * @param stream  Stream to read from.
     */
    Block(const IIStream &stream);

    /**
     * Construct a new block and copy its contents from the specified
     * location at another array.
     *
     * @param array  Source data.
     * @param at     Offset within source data.
     * @param count  Number of bytes to copy. Also the size of the new block.
     */
    Block(const IByteArray &array, Offset at, Size count);

    virtual ~Block();

    Byte *data();
    const Byte *cdata() const;
    inline const Byte *constData() const { return cdata(); }

    Block &append(Byte b);
    Block &append(const char *str, int len);
    Block &prepend(const Block &);
    void   remove(size_t pos, size_t len = 1);

                    operator const iBlock *() const { return &_block; }
    inline explicit operator bool() const { return !isEmpty_Block(&_block); }

    Block &operator += (const char *nullTerminatedCStr);
//    Block &operator += (const QByteArray &bytes);

    inline Byte &operator[](size_t pos) { return data()[pos]; }
    inline Byte operator[](size_t pos) const { return at(pos); }

    inline Byte at(size_t pos) const { return at_Block(&_block, pos); }

    Block mid(size_t pos, size_t len = iInvalidSize) const;

    /// Appends a block after this one.
    Block &operator += (const Block &other);

    /// Appends a byte array after this one.
    Block &operator += (const IByteArray &byteArray);

    /// Copies the contents of another block.
    Block &operator = (const Block &other);

    Block &operator = (const IByteArray &byteArray);

    Block compressed(int level = -1) const;
    Block decompressed() const;
    Block md5Hash() const;
    String asHexadecimalText() const;

    /**
     * Uses each byte in the block as an index to @a values and composes a new
     * block by copying sets of values by indices.
     *
     * In the resulting block, each byte of this block is represented by four bytes.
     * If @a valuesPerIndex is less than four, the missing values are taken from
     * @a defaultValues.
     *
     * In each quartet of output values, the bytes are packed in little-endian order.
     *
     * @param valuesPerIndex  Number of values per index stored in @a values.
     * @param values          Value array.
     * @param defaultValues   Output to use for missing values.
     *
     * @return New block whose size is four times larger than this block.
     */
    Block mapAsIndices(int                        valuesPerIndex,
                       const IByteArray &         values,
                       const std::array<Byte, 4> &defaultValues) const;

    /**
     * Uses each byte in the block as an index to @a values and composes a new
     * block by copying sets of values by indices.
     *
     * In the resulting block, each byte of this block is represented by four bytes.
     * If @a valuesPerIndex is less than four, the missing values are taken from
     * the @a defaultValues array.
     *
     * In each quartet of output values, the bytes are packed in little-endian order.
     *
     * @param valuesPerIndex  Number of values per index stored in @a values.
     * @param values          Value array.
     * @param defaultValues   Output to use for missing values. The size must match
     *                        this block's size. Treated as a separate layer whose
     *                        dimensions match this block. Each missing value in the
     *                        output gets the same byte from @a defaultValues, whose
     *                        position matches the position of the source byte.
     *
     * @return New block whose size is four times larger than this block.
     */
    Block mapAsIndices(int               valuesPerIndex,
                       const IByteArray &values,
                       const IByteArray &defaultValues) const;

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, const Byte *values, Size count);

    // Implements IBlock.
    void clear();
    void copyFrom(const IByteArray &array, Offset at, Size count);
    void resize(Size size);
    const Byte *data() const;

    // Implements ISerializable.
    /**
     * Writes @a block into the destination buffer. Writes the size of the
     * block in addition to its contents, so a Reader will not need to know
     * beforehand how large the block is.
     *
     * @param block  Block to write.
     *
     * @return  Reference to the Writer.
     */
    void operator >> (Writer &to) const;

    void operator << (Reader &from);

public:
    // Iterators:

    struct const_iterator
    {
        const Byte *ptr;

        Byte operator*() const { return *ptr; }
        bool operator==(const const_iterator &i) const { return ptr == i.ptr; }
        bool operator!=(const const_iterator &i) const { return ptr != i.ptr; }
        void operator++() { ptr++; }
    };

    const_iterator begin() const { return {data()}; }
    const_iterator end() const   { return {data() + size()}; }

public:
    static Block join(const List<Block> &blocks, const Block &sep = Block());

private:
    iBlock _block;
};

template <typename... Args>
Block md5Hash(Args... args) {
    Block data;
    Writer writer(data);
    writer.writeMultiple(args...);
    return data.md5Hash();
}

} // namespace de

#endif // LIBCORE_BLOCK_H
