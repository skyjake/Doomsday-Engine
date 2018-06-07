/** @file cstring.h
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

#ifndef LIBCORE_CSTRING_H
#define LIBCORE_CSTRING_H

#include "../Range"
#include "../String"
#include <c_plus/string.h>

namespace de {

/**
 * Range of characters with no null-terminating character at the end.
 * @ingroup data
 */
class DE_PUBLIC CString
{
public:
    CString() : _range{nullptr, nullptr} {}
    CString(const char *cStr) : _range{cStr, nullptr} {} // lazy init
    CString(const char *start, const char *end) : _range{start, end} {}
    CString(const std::string &str) : _range{str.data(), str.data() + str.size()} {}
    CString(const String &str) : _range{str.data(), str.data() + str.size()} {}
    CString(const Rangecc &cc) : _range{cc} {}
    CString(const iRangecc &cc) : _range{cc.start, cc.end} {}

    inline void updateEnd() const
    {
        if (!_range.end) _range.end = _range.start + strlen(_range.start);
    }
    operator String() const { return {_range.start, _range.end}; }
    String toString() const { return String(_range.start, _range.end); }
    operator Rangecc() const
    {
        updateEnd();
        return _range;
    }
    dsize size() const
    {
        updateEnd();
        return _range.size();
    }
    bool isEmpty() const { return size() == 0; }
    bool empty() const { return size() == 0; }
    bool contains(char ch) const;
    dsize indexOf(char ch, size_t from = 0) const;
    dsize indexOf(const char *cStr, size_t from = 0) const;
    CString substr(size_t start, size_t count = npos) const;
    const char *begin() const { return _range.start; }
    const char *end() const { updateEnd(); return _range.end; }
    int compare(const CString &other, const iStringComparison *sc = &iCaseSensitive) const;
    int compare(const char *cStr, const iStringComparison *sc = &iCaseSensitive) const;

    static size_t npos;

private:
    mutable Rangecc _range;
};

String operator+(const char *cStr, const CString &str) {
    return String(cStr) + str;
}

} // namespace de

#endif // LIBCORE_CSTRING_H
