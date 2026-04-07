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

#include "de/range.h"
#include "de/string.h"
#include <the_Foundation/string.h>
#include <string>

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
    CString(const char *start, const char *end) : _range{start, end} { DE_ASSERT(end >= start); }
    CString(const std::string &str) : _range{str.data(), str.data() + str.size()} {}
    CString(const String &str) : _range{str.data(), str.data() + str.size()} {}
    CString(const Rangecc &cc) : _range{cc} {}
    CString(const iRangecc &cc) : _range{cc.start, cc.end} {}

    inline void updateEnd() const
    {
        if (!_range.end && _range.start) _range.end = _range.start + strlen(_range.start);
    }

    inline explicit operator bool() const { return _range.start != nullptr; }
    inline String toString() const { return String(_range.start, _range.end); }
    inline operator Rangecc() const
    {
        updateEnd();
        return _range;
    }
    inline operator iRangecc() const
    {
        updateEnd();
        return iRangecc{begin(), end()};
    }
    inline dsize size() const
    {
        updateEnd();
        return dsize(_range.size());
    }
    inline bool isEmpty() const { return size() == 0; }
    inline bool empty() const { return size() == 0; }
    inline void setStart(const char *p) { _range.start = p; }
    inline void setEnd(const char *p) { _range.end = p; }
    inline const char *ptr(BytePos pos = BytePos(0)) const { return _range.start + pos; }
    inline const char *endPtr() const { updateEnd(); return _range.end; }
    dsize length() const;
    bool contains(char ch) const;
    bool beginsWith(const CString &prefix, Sensitivity cs = CaseSensitive) const;
    bool endsWith(const CString &suffix, Sensitivity cs = CaseSensitive) const;
    dsize indexOf(char ch, size_t from = 0) const;
    dsize indexOf(const char *cStr, size_t from = 0) const;
    dsize indexOf(const String &str, size_t from = 0) const;
    CString substr(size_t start, size_t count = npos) const;
    CString leftStrip() const;
    CString rightStrip() const;
    inline CString strip() const { return leftStrip().rightStrip(); }
    CString left(BytePos pos) const { return {_range.start, _range.start + pos}; }
    inline mb_iterator begin() const { return _range.start; }
    inline mb_iterator end() const { updateEnd(); return {_range.end, _range.start}; }
    int compare(const CString &other, Sensitivity cs = CaseSensitive) const;
    int compare(const char *cStr, Sensitivity cs = CaseSensitive) const;
    inline bool operator==(const char *cStr) const { return compare(cStr) == 0; }
    inline bool operator!=(const char *cStr) const { return compare(cStr) != 0; }
    inline bool operator==(const CString &other) const { return compare(other) == 0; }
    inline bool operator==(const String &other) const { return compare(CString(other)) == 0; }
    inline String operator/(const CString &other) const { return String(*this) / other; }
    inline String operator/(const String &other) const { return String(*this) / other; }
    inline String operator+(const String &other) const { return String(*this) + other; }
    Char first() const { return *begin(); }
    String lower() const;
    String upper() const;
    std::string toStdString() const { updateEnd(); return {_range.start, _range.end}; }

    static dsize npos;

private:
    mutable Rangecc _range;
};

inline String operator+(const char *cStr, const CString &str)
{
    return String(cStr) + str;
}

inline String operator+(const CString &str, const char *cStr)
{
    return String(str) + cStr;
}

inline String operator+(const CString &str, Char ch)
{
    String s(str);
    s += ch;
    return s;
}

inline String operator/(const CString &str, const char *cStr)
{
    return String(str).concatenatePath(cStr);
}

inline std::ostream &operator<<(std::ostream &os, const CString &str)
{
    os.write(str.begin(), str.size());
    return os;
}

} // namespace de

namespace std
{
    template<>
    struct hash<de::CString> {
        std::size_t operator()(const de::CString &key) const {
            return hash<std::string>()(key.toStdString());
        }
    };
}

#endif // LIBCORE_CSTRING_H
