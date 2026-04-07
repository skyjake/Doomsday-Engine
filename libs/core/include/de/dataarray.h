/** @file dataarray.h  Block interpreted as an array of C structs.
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

#ifndef LIBCORE_DATAARRAY_H
#define LIBCORE_DATAARRAY_H

#include "de/block.h"

namespace de {

template <typename T>
class DataArray
{
public:
    DataArray(Block data)
        : _data(std::move(data))
        , _entries(reinterpret_cast<const T *>(_data.constData()))
        , _size(int(_data.size() / sizeof(T)))
    {}

    int size() const
    {
        return _size;
    }

    const T &at(int pos) const
    {
        DE_ASSERT(pos >= 0);
        DE_ASSERT(pos < _size);
        return _entries[pos];
    }

    const T &operator[](int pos) const
    {
        return at(pos);
    }

private:
    Block    _data;
    const T *_entries;
    int      _size;
};

} // namespace de

#endif // LIBCORE_DATAARRAY_H
