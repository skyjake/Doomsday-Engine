/** @file cstring.cpp
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

#include "de/CString"
#include <cstring>

namespace de {

dsize CString::npos = dsize(-1);

dsize CString::indexOf(char ch, size_t from = 0) const
{
    const char str[2] = { ch, 0 };
    return indexOf(str, from);
}

dsize CString::indexOf(const char *cStr, size_t from = 0) const
{
    if (from >= _range.size()) return npos;
    const char *pos = strnstr(_range.start + from, cStr, _range.size() - from);
    return pos ? (pos - _range.start) : npos;
}

CString CString::substr(size_t start, size_t count) const
{
    if (start > size()) return CString();
    if (start + count > size()) count = size() - start;
    return {_range.start + start, _range.start + start + count};
}

} // namespace de
