/** @file bitarray.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/bitarray.h"

namespace de {

BitArray::BitArray(dsize initialSize)
    : _bits(initialSize, 0)
{}

bool BitArray::isEmpty() const
{
    return _bits.empty();
}

bool BitArray::testBit(dsize pos) const
{
    DE_ASSERT(pos < _bits.size());
    return _bits.at(pos) != 0;
}

dsize BitArray::size() const
{
    return _bits.size();
}

dsize BitArray::count(bool bit) const
{
    const char bval = bit ? 1 : 0;
    dsize num = 0;
    for (auto &i : _bits)
    {
        if (i == bval) ++num;
    }
    return num;
}

void BitArray::clear()
{
    _bits.clear();
}

void BitArray::resize(dsize count)
{
    return _bits.resize(count);
}

void BitArray::fill(bool bit)
{
    for (auto &b : _bits)
    {
        b = bit ? 1 : 0;
    }
}

void BitArray::setBit(dsize pos, bool bit)
{
    DE_ASSERT(pos < _bits.size());
    _bits[pos] = bit ? 1 : 0;
}

BitArray &BitArray::operator=(const BitArray &other)
{
    _bits = other._bits;
    return *this;
}

BitArray &BitArray::operator<<(bool bit)
{
    _bits.push_back(bit ? 1 : 0);
    return *this;
}

} // namespace de
