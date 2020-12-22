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

#include "de/block.h"
#include "de/file.h"

#include <cstring>

namespace de {

Block::Block(Size initialSize)
{
    init_Block(&_block, initialSize);
}

Block::Block(const iBlock *other)
{
    initCopy_Block(&_block, other);
}

Block::Block(const IByteArray &other)
{
    const dsize num = other.size();
    init_Block(&_block, num);
    // Read the other's data directly into our data buffer.
    other.get(0, data(), num);
    // Ensure it's null-terminated.
    data()[num] = 0;
}

Block::Block(const Block &other)
{
    initCopy_Block(&_block, &other._block);
}

Block::Block(Block &&moved)
{
    initCopy_Block(&_block, &moved._block);
}

Block::Block(const char *nullTerminatedCStr)
{
    initCStr_Block(&_block, nullTerminatedCStr);
}

Block::Block(const std::string &str)
{
    initData_Block(&_block, str.data(), str.size());
}

Block::Block(const void *data, Size length)
{
    initData_Block(&_block, data, length);
}

Block::Block(IIStream &stream)
{
    init_Block(&_block, 0);
    stream >> *this;
}

Block::Block(const IIStream &stream)
{
    init_Block(&_block, 0);
    stream >> *this;
}

Block::Block(const IByteArray &other, Offset at, Size count)
{
    init_Block(&_block, 0);
    copyFrom(other, at, count);
}

Block::~Block()
{
    if (_block.i)
    {
        deinit_Block(&_block);
    }
}

Block::Size Block::size() const
{
    return size_Block(&_block);
}

void Block::get(Offset atPos, Byte *values, Size count) const
{
    if (atPos + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::get", "Out of range " +
                          Stringf("(%zu[+%zu] > %zu)", atPos, count, size()));
    }
    std::memcpy(values, constData() + atPos, count);
}

void Block::set(Offset at, const Byte *values, Size count)
{
    if (at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::set", "Out of range");
    }
    setSubData_Block(&_block, at, values, count);
}

void Block::copyFrom(const IByteArray &array, Offset at, Size count)
{
    // Read the other's data directly into our data buffer.
    resize(count);
    array.get(at, data(), count);
}

void Block::resize(Size size)
{
    resize_Block(&_block, size);
}

const IByteArray::Byte *Block::cdata() const
{
    return reinterpret_cast<const Byte *>(constData_Block(&_block));
}

Block &Block::append(Byte b)
{
    appendData_Block(&_block, &b, 1);
    return *this;
}

Block &Block::append(const void *data, int len)
{
    appendData_Block(&_block, data, len);
    return *this;
}

Block &Block::prepend(const Block &other)
{
    iBlock *cat = concat_Block(other, &_block);
    set_Block(&_block, cat);
    delete_Block(cat);
    return *this;
}

void Block::remove(size_t pos, size_t len)
{
    remove_Block(&_block, pos, len);
}

void Block::removeAll(Byte b)
{
    for (dsize i = 0; i < size(); ++i)
    {
        if (constBegin_Block(&_block)[i] == b)
        {
            remove(i--, 1);
        }
    }
}

bool Block::beginsWith(const char *cstr) const
{
    const dsize len = strlen(cstr);
    if (size() >= len)
    {
        return memcmp(cstr, c_str(), len) == 0;
    }
    return false;
}

Block::Byte &Block::operator[](size_t pos)
{
    if (pos >= size()) resize(pos + 1);
    return data()[pos];
}

Block &Block::operator+=(const char *nullTerminatedCStr)
{
    appendCStr_Block(&_block, nullTerminatedCStr);
    return *this;
}

Block Block::mid(size_t pos, size_t len) const
{
    iBlock *m = mid_Block(&_block, pos, len);
    Block i(m);
    delete_Block(m);
    return i;
}

Block Block::left(size_t len) const
{
    Block trunc(*this);
    truncate_Block(&trunc._block, len);
    return trunc;
}

int Block::compare(const Block &other) const
{
    return cmp_Block(*this, other);
}
    
bool Block::operator==(const Block &other) const
{
    return size() == other.size() && compare(other) == 0;
}

Block Block::operator+(const Block &other) const
{
    Block cat(*this);
    cat += other;
    return cat;
}

Block::Byte *Block::data()
{
    return reinterpret_cast<Byte *>(data_Block(&_block));
}

const Block::Byte *Block::data() const
{
    return reinterpret_cast<const Byte *>(constData_Block(&_block));
}

void Block::operator>>(Writer &to) const
{
    // First write the length of the block.
    to << duint32(size());

    to.writeBytes(size(), *this);
}

void Block::operator<<(Reader &from)
{
    duint32 size = 0;
    from >> size;

    resize(size);
    from.readBytes(size, *this);
}

Block Block::take(iBlock *b)
{
    Block block(b);
    delete_Block(b);
    return block;
}

Block &Block::operator+=(const Block &other)
{
    append_Block(&_block, other);
    return *this;
}

Block &Block::operator+=(const IByteArray &byteArray)
{
    Offset pos = size();
    resize(size() + byteArray.size());
    byteArray.get(0, data() + pos, byteArray.size());
    return *this;
}

Block &Block::operator=(const Block &other)
{
    set_Block(&_block, &other._block);
    return *this;
}

Block &Block::operator=(Block &&moved)
{
    set_Block(&_block, &moved._block);
    return *this;
}
    
Block &Block::operator=(const IByteArray &byteArray)
{
    copyFrom(byteArray, 0, byteArray.size());
    return *this;
}

Block Block::compressed(int level) const
{
    iBlock *deflated = compressLevel_Block(&_block, level);
    Block i(deflated);
    delete_Block(deflated);
    return i;
}

Block Block::decompressed() const
{
    iBlock *inflated = decompress_Block(&_block);
    Block i(inflated);
    delete_Block(inflated);
    return i;
}

Block Block::md5Hash() const
{
    Block hash(16);
    md5_Block(&_block, hash.data());
    return hash;
}

static char asciiHexNumber(char n)
{
    if (n < 0 || n > 15) return ' ';
    return n <= 9? ('0' + n) : ('a' + n - 10);
}

String Block::asHexadecimalText() const
{
    String hex;
    for (size_t i = 0; i < size(); i++)
    {
        const auto ch = at(i);
        hex += asciiHexNumber(ch >> 4);
        hex += asciiHexNumber(ch & 0xf);
    }
    return hex;
}

void Block::fill(Byte b)
{
    fill_Block(&_block, b);
}

Block Block::mapAsIndices(int                        valuesPerIndex,
                          const IByteArray &         values,
                          const std::array<Byte, 4> &defaultValues) const
{
    DE_ASSERT(valuesPerIndex >= 1);
    DE_ASSERT(valuesPerIndex <= 4);

    Block mapped(4 * size()); // output is always packed as uint32

    const Byte *in  = cdata();
    const Byte *end = cdata() + size();
    uint32_t *  out = reinterpret_cast<uint32_t *>(mapped.data());

    Byte entry[4]{defaultValues[0], defaultValues[1], defaultValues[2], defaultValues[3]};

    for (; in != end; ++in)
    {
        values.get(valuesPerIndex * (*in), entry, valuesPerIndex);
        *out++ = entry[0] | (entry[1] << 8) | (entry[2] << 16) | (entry[3] << 24);
    }

    return mapped;
}

Block Block::mapAsIndices(int               valuesPerIndex,
                          const IByteArray &values,
                          const IByteArray &defaultValues) const
{
    DE_ASSERT(valuesPerIndex >= 1);
    DE_ASSERT(valuesPerIndex <= 4);
    DE_ASSERT(defaultValues.size() == size());

    Block mapped(4 * size()); // output is always packed as uint32

    const Byte *begin = cdata();
    const Byte *in    = begin;
    const Byte *end   = begin + size();
    uint32_t *  out   = reinterpret_cast<uint32_t *>(mapped.data());

    Byte entry[4];
    Byte missing;

    for (; in != end; ++in)
    {
        defaultValues.get(in - begin, &missing, 1);
        for (int i = valuesPerIndex; i < 4; ++i)
        {
            entry[i] = missing;
        }
        values.get(valuesPerIndex * (*in), entry, valuesPerIndex);
        *out++ = entry[0] | (entry[1] << 8) | (entry[2] << 16) | (entry[3] << 24);
    }

    return mapped;
}

Block Block::join(const List<Block> &blocks, const Block &sep) // static
{
    if (blocks.isEmpty()) return Block();

    Block joined = blocks.at(0);
    for (int i = 1; i < blocks.sizei(); ++i)
    {
        joined += sep;
        joined += blocks.at(i);
    }
    return joined;
}

Block Block::readAll(std::istream &is)
{
    Block data;
    char buf[16384];
    while (!is.eof())
    {
        is.read(buf, sizeof(buf));
        appendData_Block(&data._block, buf, is.gcount());
    }
    return data;
}

void Block::clear()
{
    clear_Block(&_block);
}

} // namespace de
