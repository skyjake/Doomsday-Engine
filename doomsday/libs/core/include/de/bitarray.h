/** @file bitarray.h  Array of bits.
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

#ifndef LIBCORE_BITARRAY_H
#define LIBCORE_BITARRAY_H

#include "de/libcore.h"
#include <vector>

namespace de {

/**
 * Array of bits.
 * @ingroup data
 */
class DE_PUBLIC BitArray
{
public:
    BitArray(dsize initialSize = 0);

    bool        isEmpty() const;
    bool        testBit(dsize pos) const;
    inline bool at(dsize pos) const { return testBit(pos); }
    dsize       size() const;
    dsize       count(bool bit) const;

    inline int sizei() const { return int(_bits.size()); }
    void clear();
    void resize(dsize count);
    void fill(bool bit);
    void setBit(dsize pos, bool bit = true);

    BitArray &operator=(const BitArray &);

    BitArray &operator<<(bool bit);

private:
    std::vector<char> _bits;
};

} // namespace de

#endif // LIBCORE_BITARRAY_H
