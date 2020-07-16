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

#include "de/cstring.h"
#include <cstring>

namespace de {

dsize CString::npos = std::numeric_limits<dsize>::max();

dsize CString::length() const
{
    dsize len = 0;
    for (auto i = begin(), j = end(); i != j; ++i, ++len) {}
    return len;
}

bool CString::contains(char ch) const
{
    updateEnd();
    for (const char *i = _range.start; i != _range.end; ++i)
    {
        if (*i == ch) return true;
    }
    return false;
}

bool CString::beginsWith(const CString &prefix, Sensitivity cs) const
{
    if (prefix.size() > size()) return false;
    return CString(_range.start, _range.start + prefix.size()).compare(prefix, cs) == 0;
}

bool CString::endsWith(const CString &suffix, Sensitivity cs) const
{
    if (suffix.size() > size()) return false;
    return CString(_range.end - suffix.size(), end()).compare(suffix, cs) == 0;
}

dsize CString::indexOf(char ch, size_t from) const
{
    const char str[2] = { ch, 0 };
    return indexOf(str, from);
}

dsize CString::indexOf(const char *cStr, size_t from) const
{
    if (from >= size_t(_range.size())) return npos;
    const char *pos = iStrStrN(_range.start + from, cStr, _range.size() - from);
    return pos ? dsize(pos - _range.start) : npos;
}

dsize CString::indexOf(const String &str, size_t from) const
{
    return indexOf(str.c_str(), from);
}

CString CString::substr(size_t start, size_t count) const
{
    if (start > size()) return CString();
    if (count == npos || start + count > size()) count = size() - start;
    return {_range.start + start, _range.start + start + count};
}

CString CString::leftStrip() const
{
    CString s(*this);
    mb_iterator i = s.begin();
    while (!s.isEmpty() && (*i).isSpace())
    {
        s._range.start = ++i;
    }
    return s;
}

CString CString::rightStrip() const
{
    CString s(*this);
    mb_iterator i = s.end();
    while (!s.isEmpty())
    {
        if ((*--i).isSpace())
        {
            s._range.end = i;
        }
        else break;
    }
    return s;
}

int CString::compare(const CString &other, Sensitivity cs) const
{
    const iRangecc range{begin(), end()};
    return cmpCStrNSc_Rangecc(&range, other.begin(), other.size(), cs);
}

int CString::compare(const char *cStr, Sensitivity cs) const
{
    const iRangecc range{begin(), end()};
    return cmpCStrSc_Rangecc(&range, cStr, cs);
}

String CString::lower() const
{
    String low;
    for (mb_iterator i = begin(), j = end(); i != j; ++i)
    {
        low += (*i).lower();
    }
    return low;
}

String CString::upper() const
{
    String up;
    for (mb_iterator i = begin(), j = end(); i != j; ++i)
    {
        up += (*i).upper();
    }
    return up;
}

} // namespace de
