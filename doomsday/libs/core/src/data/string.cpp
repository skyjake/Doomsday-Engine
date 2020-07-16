/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/app.h"
#include "de/string.h"
#include "de/block.h"
#include "de/regexp.h"
#include "de/path.h"
#include "de/charsymbols.h"

#include <the_Foundation/path.h>
#include <cstdio>
#include <cstdarg>
#include <cerrno>

namespace de {

Char Char::upper() const
{
    return upper_Char(_ch);
}

Char Char::lower() const
{
    return lower_Char(_ch);
}

bool Char::isSpace() const
{
    return isSpace_Char(_ch);
}

bool Char::isAlpha() const
{
    return isAlpha_Char(_ch);
}

bool Char::isNumeric() const
{
    return isNumeric_Char(_ch);
}

bool Char::isAlphaNumeric() const
{
    return isAlphaNumeric_Char(_ch);
}

String::String()
{
    init_String(&_str);
}

String::String(const String &other)
{
    initCopy_String(&_str, &other._str);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(String &&moved)
{
    initCopy_String(&_str, &moved._str);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const Block &bytes)
{
    initCopy_Block(&_str.chars, bytes);
    // Blocks are not guaranteed to be well-formed strings, so null characters
    // may appear within.
    truncate_Block(&_str.chars, strlen(bytes.c_str()));
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const iBlock *bytes)
{
    initCopy_Block(&_str.chars, bytes);
    // Blocks are not guaranteed to be well-formed strings, so null characters
    // may appear within.
    truncate_Block(&_str.chars, strlen(constBegin_Block(bytes)));
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const iString *other)
{
    initCopy_String(&_str, other);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const std::string &text)
{
    initCStrN_String(&_str, text.data(), text.size());
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const std::wstring &text)
{
    init_String(&_str);
    for (auto ch : text)
    {
        iMultibyteChar mb;
        init_MultibyteChar(&mb, ch);
        appendCStr_String(&_str, mb.bytes);
    }
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const char *nullTerminatedCStr)
{
    initCStr_String(&_str, nullTerminatedCStr ? nullTerminatedCStr : "");
}

//String::String(const wchar_t *nullTerminatedWideStr)
//{
//    initWide_String(&_str, nullTerminatedWideStr ? nullTerminatedWideStr : L"");
//}

String::String(const char *cStr, int length)
{
    DE_ASSERT(cStr != nullptr);
    initCStrN_String(&_str, cStr, length);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const char *cStr, dsize length)
{
    initCStrN_String(&_str, cStr, length);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(dsize length, char ch)
{
    init_Block(&_str.chars, length);
    fill_Block(&_str.chars, ch);
}

String::String(const char *start, const char *end)
{
    initCStrN_String(&_str, start, end - start);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const Range<const char *> &range)
{
    initCStrN_String(&_str, range.start, range.end - range.start);
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(const CString &cstr)
{
    initCStrN_String(&_str, cstr.begin(), cstr.size());
    DE_ASSERT(strchr(cstr_String(&_str), 0) == constEnd_String(&_str));
}

String::String(dsize length, Char ch)
{
    init_String(&_str);

    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    if (strlen(mb.bytes) == 1)
    {
        resize_Block(&_str.chars, length);
        fill_Block(&_str.chars, mb.bytes[0]);
    }
    else
    {
        while (length-- > 0)
        {
            appendCStr_String(&_str, mb.bytes);
        }
    }
}

String::~String()
{
    // The string may have been deinitialized already by a move.
    if (_str.chars.i)
    {
        deinit_String(&_str);
    }
}

void String::resize(size_t newSize)
{
    resize_Block(&_str.chars, newSize);
}

std::wstring String::toWideString() const
{
    std::wstring ws;
    for (auto c : *this)
    {
        ws.push_back(c);
    }
    return ws;
}

CString String::toCString() const
{
    return {begin(), end()};
}

void String::clear()
{
    clear_String(&_str);
}

bool String::contains(char c) const
{
    return strchr(c_str(), c) != nullptr;
//    for (const char *i = constBegin_String(&_str), *end = constEnd_String(&_str); i != end; ++i)
//    {
//        if (*i == c) return true;
//    }
//    return false;
}

bool String::contains(Char c) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, c);
    return contains(mb.bytes);
}

bool String::contains(const char *cStr, Sensitivity cs) const
{
    return indexOfCStrSc_String(&_str, cStr, cs) != iInvalidPos;
}

int String::count(char ch) const
{
    int num = 0;
    for (const char *i = constBegin_String(&_str), *end = constEnd_String(&_str); i != end; ++i)
    {
        if (*i == ch) ++num;
    }
    return num;
}

bool String::beginsWith(Char ch, Sensitivity cs) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    return beginsWith(mb.bytes, cs);
}

Char String::at(CharPos pos) const
{
    // Multibyte string; we don't know the address matching this character position.
    dsize index = 0;
    for (Char ch : *this)
    {
        if (pos == index++) return ch;
    }
    return {};
}

String String::substr(CharPos pos, dsize count) const
{
    return String::take(mid_String(&_str, pos.index, count));
}

String String::substr(BytePos pos, dsize count) const
{
    iBlock *sub = mid_Block(&_str.chars, pos.index, count);
    String s(sub);
    delete_Block(sub);
    return s;
}

String String::substr(const Range<CharPos> &range) const
{
    return substr(range.start, range.size().index);
}

String String::substr(const Range<BytePos> &range) const
{
    return substr(range.start, range.size().index);
}

String String::right(CharPos count) const
{
    if (count.index == 0) return {};
    auto i = rbegin();
    for (auto end = rend(); --count.index > 0 && i != end; ++i) {}
    return String(static_cast<const char *>(i), data() + size());
}

void String::remove(BytePos start, dsize count)
{
    remove_Block(&_str.chars, start.index, count);
}

void String::remove(BytePos start, CharPos charCount)
{
    // Need to know which bytes to remove.
    mb_iterator i = data() + start;
    while (charCount-- > 0 && i != end()) { ++i; }
    remove(start, i.pos());
}

void String::truncate(BytePos pos)
{
    truncate_Block(&_str.chars, pos.index);
}

List<String> String::split(const char *separator) const
{
    List<String> parts;
    iRangecc seg{};
    iRangecc str{constBegin_String(&_str), constEnd_String(&_str)};
    while (nextSplit_Rangecc(&str, separator, &seg))
    {
        parts << String(seg.start, seg.end);
    }
    return parts;
}

List<CString> String::splitRef(const char *separator) const
{
    List<CString> parts;
    iRangecc seg{};
    iRangecc str{constBegin_String(&_str), constEnd_String(&_str)};
    while (nextSplit_Rangecc(&str, separator, &seg))
    {
        parts << CString(seg.start, seg.end);
    }
    return parts;
}

List<CString> String::splitRef(Char ch) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    return splitRef(mb.bytes);
}

String String::operator+(char *cStr) const
{
    return *this + const_cast<const char *>(cStr);
}

List<String> String::split(Char ch) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    return split(mb.bytes);
}

List<String> String::split(const RegExp &regExp) const
{
    List<String> parts;
    if (!isEmpty())
    {
        const char *pos = constBegin_String(&_str);
        for (RegExpMatch m; regExp.match(*this, m); pos = m.end())
        {
            // The part before the matched separator.
            parts << String(pos, m.begin());
        }
        // The final part.
        parts << String(pos, constEnd_String(&_str));
    }
    return parts;
}

String String::operator+(const std::string &s) const
{
    return *this + CString(s);
}

String String::operator+(const CString &cStr) const
{
    String cat = *this;
    appendCStrN_String(&cat._str, cStr.begin(), cStr.size());
    return cat;
}

String String::operator+(const String &str) const
{
    String cat = *this;
    append_String(&cat._str, str);
    return cat;
}

String String::operator+(const char *cStr) const
{
    String cat(*this);
    appendCStr_String(&cat._str, cStr);
    return cat;
}

String &String::operator+=(char ch)
{
    appendData_Block(&_str.chars, &ch, 1);
    return *this;
}

String &String::operator+=(Char ch)
{
    appendChar_String(&_str, ch);
    return*this;
}

String &String::operator+=(const char *cStr)
{
    appendCStr_String(&_str, cStr);
    return *this;
}

String &String::operator+=(const CString &s)
{
    appendCStrN_String(&_str, s.begin(), s.size());
    return *this;
}

String &String::operator+=(const String &other)
{
    append_String(&_str, &other._str);
    return *this;
}

String &String::prepend(Char ch)
{
    prependChar_String(&_str, ch);
    return *this;
}

String &String::prepend(const String &other)
{
    return *this = other + *this;
}

void String::insert(BytePos pos, const char *cStr)
{
    insertData_Block(&_str.chars, pos.index, cStr, cStr ? strlen(cStr) : 0);
}

void String::insert(BytePos pos, const String &str)
{
    insertData_Block(&_str.chars, pos.index, str.data(), str.size());
}

String &String::replace(Char before, Char after)
{
    iMultibyteChar mb1, mb2;
    init_MultibyteChar(&mb1, before);
    init_MultibyteChar(&mb2, after);
    return replace(mb1.bytes, mb2.bytes);
}

String &String::replace(const CString &before, const CString &after)
{
    const String oldTerm{before};
    const iRangecc newTerm = after;
    iRangecc remaining(*this);
    String result;
    dsize found;
    while ((found = CString(remaining).indexOf(oldTerm)) != npos)
    {
        const iRangecc prefix{remaining.start, remaining.start + found};
        appendRange_String(result.i_str(), &prefix);
        appendRange_String(result.i_str(), &newTerm);
        remaining.start += found + before.size();
    }
    if (size_Range(&remaining) == size())
    {
        // No changes were applied.
        return *this;
    }
    appendRange_String(result.i_str(), &remaining);
    return *this = result;
}

String &String::replace(const RegExp &before, const CString &after)
{
    static const char *capSubs[8] = {
        "\\1", "\\2", "\\3", "\\4", "\\5", "\\6", "\\7", "\\8",
    };
    const iRangecc newTerm = after;
    iRangecc remaining(*this);
    String result;
    RegExpMatch found;
    while (before.match(*this, found))
    {
        DE_ASSERT(found.end() > found.begin());
        const iRangecc prefix{remaining.start, found.begin()};
        appendRange_String(result.i_str(), &prefix);
        String replaced{newTerm};
        for (int cap = 0; cap < 8; ++cap)
        {
            replaced.replace(capSubs[cap], found.capturedCStr(cap + 1));
        }
        append_String(result.i_str(), replaced);
        remaining.start = found.end();
    }
    appendRange_String(result.i_str(), &remaining);
    return *this = result;
}

Char String::first() const
{
    return empty() ? Char() : *begin();
}

Char String::last() const
{
    if (empty()) return {};
    const Char ch = *rbegin();
    DE_ASSERT(ch != 0);
    return ch;
}

String String::operator/(const String &path) const
{
    return concatenatePath(path);
}

String String::operator/(const CString &path) const
{
    return concatenatePath(String(path));
}

String String::operator/(const char *path) const
{
    return concatenatePath(String(path));
}

String String::operator/(const Path &path) const
{
    return concatenatePath(path.toString());
}

String String::operator%(const PatternArgs &args) const
{
    String result;
//    QTextStream output(&result);

    PatternArgs::const_iterator arg = args.begin();

    for (auto i = begin(); i != end(); ++i)
    {
        if (*i == '%')
        {
            String::const_iterator next = i;
            advanceFormat(next, end());
            if (*next == '%')
            {
                // Escaped.
                result += *next;
                ++i;
                continue;
            }

            if (arg == args.end())
            {
                // Out of args.
                throw IllegalPatternError("String::operator%", "Ran out of arguments");
            }

            result += patternFormat(i, end(), **arg);
            ++arg;
        }
        else
        {
            result += *i;
        }
    }

    // Just append the rest of the arguments without special instructions.
    for (; arg != args.end(); ++arg)
    {
        result += (*arg)->asText();
    }

    return result;
}

String String::concatenatePath(const String &other, Char dirChar) const
{
    if (other.beginsWith(dirChar) || 
        // Could be a file system path, though? That has additional rules for 
        // absolute paths.
        ((dirChar == '/' || dirChar == '\\') && isAbsolute_Path(&other._str)))
    {
        // The other path is absolute - use as is.
        return other;
    }
    return concatenateRelativePath(other, dirChar);
}

String String::concatenateRelativePath(const String &other, Char dirChar) const
{
    if (other.isEmpty()) return *this;

    const auto startPos = CharPos(other.first() == dirChar? 1 : 0);

    // Do a path combination. Check for a slash.
    String result(*this);
    if (!empty() && last() != dirChar)
    {
        result += dirChar;
    }
    result += other.substr(startPos);
    return result;
}

String String::concatenateMember(const String &member) const
{
    if (member.isEmpty()) return *this;
    if (member.first() == '.')
    {
        throw InvalidMemberError("String::concatenateMember", "Invalid: '" + member + "'");
    }
    return concatenatePath(member, '.');
}

String String::strip() const
{
    String ts(*this);
    trim_String(&ts._str);
    return ts;
}

String String::leftStrip() const
{
    String ts(*this);
    trimStart_String(&ts._str);
    return ts;
    }

String String::rightStrip() const
{
    String ts(*this);
    trimEnd_String(&ts._str);
    return ts;
}

String String::normalizeWhitespace() const
{
    String s = *this;
    s.replace(RegExp::WHITESPACE, " ");
    return s.strip();
}

String String::removed(const RegExp &expr) const
{
    String s = *this;
    s.replace(expr, "");
    return s;
}

String String::lower() const
{
    return String::take(lower_String(&_str));
}

String String::upper() const
{
    return String::take(upper_String(&_str));
}

String String::upperFirstChar() const
{
    if (isEmpty()) return "";
    const_iterator i = begin();
    String capitalized(1, (*i++).upper());
    appendCStr_String(&capitalized._str, i);
    return capitalized;
}

CString String::fileName(Char dirChar) const
{
    if (auto bytePos = lastIndexOf(dirChar))
    {
        return {data() + bytePos.index + 1, data() + size()};
    }
    return *this;
}

CString String::fileNameWithoutExtension() const
{
    const CString name = fileName();
    if (auto dotPos = lastIndexOf('.'))
    {
        if (dotPos.index > dsize(name.begin() - data()))
        {
            return {name.begin(), data() + dotPos.index};
        }
    }
    return name;
}

CString String::fileNameExtension() const
{
    auto pos      = lastIndexOf('.');
    auto slashPos = lastIndexOf('/');
    if (pos)
    {
        // If there is a directory included, make sure there it at least
        // one character's worth of file name before the period.
        if (pos > 0 && (!slashPos || pos > slashPos + 1))
        {
            return {data() + pos.index, data() + size()};
        }
    }
    return "";
}

CString String::fileNamePath(Char dirChar) const
{
    if (auto pos = lastIndexOf(dirChar))
    {
        return {data(), data() + pos.index};
    }
    return "";
}

String String::fileNameAndPathWithoutExtension(Char dirChar) const
{
    return String(fileNamePath(dirChar)) / fileNameWithoutExtension();
}

bool String::containsWord(const String &word) const
{
    if (word.isEmpty())
    {
        return false;
    }
    return RegExp(stringf("\\b%s\\b", word.c_str())).hasMatch(*this);
}

bool String::operator==(const CString &cstr) const
{
    return CString(*this) == cstr;
}

int String::compare(const CString &str, Sensitivity cs) const
{
    const iRangecc s = *this;
    return cmpCStrNSc_Rangecc(&s, str.begin(), str.size(), cs);
}

dint String::compareWithCase(const String &other) const
{
    return cmpSc_String(&_str, other, &iCaseSensitive);
}

dint String::compareWithoutCase(const String &other) const
{
    return cmpSc_String(&_str, other, &iCaseInsensitive);
}

dint String::compareWithoutCase(const String &other, int n) const
{
    return cmpNSc_String(&_str, other, n, &iCaseInsensitive);
}

CharPos String::commonPrefixLength(const String &str, Sensitivity sensitivity) const
{
    CharPos count{0};
    for (const_iterator a = begin(), b = str.begin(), aEnd = end(), bEnd = str.end();
         a != aEnd && b != bEnd; ++a, ++b, ++count)
    {
        if (sensitivity == CaseSensitive)
        {
            if (*a != *b) break;
        }
        else
        {
            if ((*a).lower() != (*b).lower()) break;
        }
    }
    return count;
}

String::const_iterator String::begin() const
{
    return const_iterator(*this);
}

String::const_iterator String::end() const
{
    return {data() + size(), data()};
}

String::const_reverse_iterator String::rbegin() const
{
    return {*this};
}

String::const_reverse_iterator String::rend() const
{
    return mb_iterator(data() - 1);
//    init_StringReverseConstIterator(&i.iter, &_str);
//    i.iter.pos = i.iter.next = constBegin_String(&_str);
//    i.iter.remaining = 0;
//    return i;
}

String String::take(iString *str) // static
{
    String s(str);
    delete_String(str);
    return s;
}

void String::skipSpace(const_iterator &i, const const_iterator &end)
{
    while (i != end && (*i).isSpace()) ++i;
}

String String::format(const char *format, ...)
{
    va_list args;
    Block buffer;

    for (;;)
    {
        va_start(args, format);
        int count = vsnprintf(buffer ? buffer.writableCharPointer() : nullptr,
                              buffer.size(), format, args);
        va_end(args);

        if (count < 0)
        {
            warning("[String::format] Error %i: %s", errno, strerror(errno));
            return buffer;
        }
        if (dsize(count) < buffer.size())
        {
            buffer.resize(count);
            break;
        }
        buffer.resize(count + 1);
    }
    return buffer;
}

String String::asText(dfloat value, int precision)
{
    return Stringf(stringf("%%.%if", precision).c_str(), value);
}

dint String::toInt(bool *ok, int base, duint flags) const
{
    char *endp;
    auto value = std::strtol(*this, &endp, base);
    if (ok) *ok = (errno != ERANGE);
    if (!(flags & AllowSuffix) && !(*endp == 0 || isspace(*endp)))
    {
        // Suffix not allowed; consider this a failure.
        if (ok) *ok = false;
        value = 0;
        }
    return dint(value);
}

duint32 String::toUInt32(bool *ok, int base) const
{
    const auto value = std::strtoul(*this, nullptr, base);
    if (ok) *ok = (errno != ERANGE);
    return duint32(value);
}

long String::toLong(bool *ok, int base) const
{
    long value = std::strtol(*this, nullptr, base);
    if (ok) *ok = (errno != ERANGE);
    return value;
}

dfloat String::toFloat() const
{
    return std::strtof(*this, nullptr);
}

ddouble String::toDouble() const
{
    return std::strtod(*this, nullptr);
}

String String::addLinePrefix(const String &prefix) const
{
    String result;
    iRangecc str{constBegin_String(&_str), constEnd_String(&_str)};
    iRangecc range{};
    while (nextSplit_Rangecc(&str, "\n", &range))
    {
        if (!result.empty()) result += '\n';
        result += prefix;
        result += String(range.start, range.end);
    }
    return result;
}

String String::escaped() const
{
    String esc = *this;
    esc.replace("\\", "\\\\")
       .replace("\"", "\\\"")
       .replace("\b", "\\b")
       .replace("\f", "\\f")
       .replace("\n", "\\n")
       .replace("\r", "\\r")
       .replace("\t", "\\t");
    return esc;
}

void String::get(Offset at, Byte *values, Size count) const
{
    if (at + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("String::get", "Out of range " +
                          Stringf("(%zu[+%zu] > %zu)", at, count, size()));
    }
    std::memcpy(values, constBegin_String(&_str) + at, count);
}

void String::set(Offset at, const Byte *values, Size count)
{
    if (at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("String::set", "Out of range");
    }
    setSubData_Block(&_str.chars, at, values, count);
}

String String::truncateWithEllipsis(dsize maxLength) const
{
    if (size() <= maxLength)
    {
        return *this;
    }
    return left(CharPos(maxLength/2 - 1)) + "..." + right(CharPos(maxLength/2 - 1));
}

void String::advanceFormat(const_iterator &i, const const_iterator &end)
{
    ++i;
    if (i == end)
    {
        throw IllegalPatternError("String::advanceFormat",
                                  "Incomplete formatting instructions");
    }
}

bool String::contains(const StringList &stringList, const char *str, Sensitivity s)
{
    const CString j(str);
    for (const auto &i : stringList)
    {
        if (!i.compare(j, s)) return true;
    }
    return false;
}

String String::join(const StringList &stringList, const char *sep)
{
    if (stringList.isEmpty()) return {};

    String joined = stringList.at(0);
    for (dsize i = 1; i < stringList.size(); ++i)
    {
        joined += sep;
        joined += stringList.at(i);
    }
    return joined;
}

String String::patternFormat(const_iterator &      formatIter,
                             const const_iterator &formatEnd,
                             const IPatternArg &   arg)
{
    advanceFormat(formatIter, formatEnd);

    String result;

    // An argument comes here.
    bool  rightAlign = true;
    dsize maxWidth = 0;
    dsize minWidth = 0;

    DE_ASSERT(*formatIter != '%');

    if (*formatIter == '-')
    {
        // Left aligned.
        rightAlign = false;
        advanceFormat(formatIter, formatEnd);
    }
    String::const_iterator k = formatIter;
    while ((*formatIter).isNumeric())
    {
        advanceFormat(formatIter, formatEnd);
    }
    if (k != formatIter)
    {
        // Got the minWidth.
        minWidth = String(k, formatIter).toInt();
    }
    if (*formatIter == '.')
    {
        advanceFormat(formatIter, formatEnd);
        k = formatIter;
        // There's also a maxWidth.
        while ((*formatIter).isNumeric())
        {
            advanceFormat(formatIter, formatEnd);
        }
        maxWidth = String(k, formatIter).toInt();
    }

    // Finally, the type formatting.
    switch (*formatIter)
    {
    case 's':
        result += arg.asText();
        break;

    case 'b':
        result += (int(arg.asNumber())? "True" : "False");
        break;

    case 'c':
        result += Char(uint32_t(arg.asNumber()));
        break;

    case 'i':
    case 'd':
        result += format("%lli", dint64(arg.asNumber()));
        break;

    case 'u':
        result += format("%llu", duint64(arg.asNumber()));
        break;

    case 'X':
        result += format("%llX", dint64(arg.asNumber()));
        break;

    case 'x':
        result += format("%llx", dint64(arg.asNumber()));
        break;

    case 'p':
        result += format("%p", dintptr(arg.asNumber()));
        break;

    case 'f':
        // Max width is interpreted as the number of decimal places.
        result += format(stringf("%%.%df", maxWidth? maxWidth : 3).c_str(), arg.asNumber());
        maxWidth = 0;
        break;

    default:
        throw IllegalPatternError("Log::Entry::str",
            "Unknown format character '" + String(1, *formatIter) + "'");
    }

    // Align and fit.
    if (maxWidth || minWidth)
    {
        // Must determine actual character count.
        CharPos len = result.sizec();

        if (maxWidth && len.index > maxWidth)
        {
            result = result.left(CharPos(maxWidth));
            len.index = maxWidth;
        }

        if (minWidth && len.index < minWidth)
        {
            // Pad it.
            String padding = String(minWidth - len.index, ' ');
            if (rightAlign)
            {
                result = padding + result;
            }
            else
            {
                result += padding;
            }
        }
    }
    return result;
}

Block String::toUtf8() const
{
    return Block(&_str.chars);
}

Block String::toLatin1() const
{
    // Non-8-bit characters are simply filtered out.
    Block latin;
    for (Char ch : *this)
{
        if (ch < 256) latin.append(Block::Byte(ch));
}
    return latin;
}

String String::fromUtf8(const IByteArray &byteArray)
{
    String s;
    setBlock_String(&s._str, Block(byteArray));
    return s;
}

String String::fromUtf8(const Block &block)
{
    String s;
    setBlock_String(&s._str, block);
    return s;
}

String String::fromUtf8(const char *nullTerminatedCStr)
{
    return String(nullTerminatedCStr);
}

String String::fromLatin1(const IByteArray &byteArray)
{
    const Block bytes(byteArray);
    return String(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

String String::fromCP437(const IByteArray &byteArray)
{
    const Block chars(byteArray);
    String conv;
    for (auto ch : chars)
    {
        conv += Char(codePage437ToUnicode(ch));
    }
    return conv;
}

Block String::toPercentEncoding() const
{
    iString *enc = urlEncode_String(*this);
    Block b(&enc->chars);
    delete_String(enc);
    return b;
}

String String::fromPercentEncoding(const Block &percentEncoded) // static
{
    return String::take(urlDecode_String(String(percentEncoded)));
}

Block String::toUtf16() const
{
    return Block::take(toUtf16_String(*this));
}

String String::fromUtf16(const Block &utf16) // static
{
    return String::take(newUtf16_String(reinterpret_cast<const uint16_t *>(utf16.data())));
}

//------------------------------------------------------------------------------------------------

String::const_reverse_iterator::const_reverse_iterator(const Range<const char *> &range)
    : iter(range.end, range.start)
{
    --iter;
}

String::const_reverse_iterator::const_reverse_iterator(const CString &cstr)
    : iter(cstr.end(), cstr.begin())
{
    --iter;
}

String::const_reverse_iterator::const_reverse_iterator(const String &str)
    : iter(str.data() + str.size(), str.data())
{
    --iter;
}

//------------------------------------------------------------------------------------------------

mb_iterator::mb_iterator(const String &str)
    : cur(str.data())
    , start(str.data())
{}

Char mb_iterator::operator*() const
{
    return decode();
}

Char mb_iterator::decode(const char **end) const
{
    if (*cur == 0)
    {
        if (end) *end = cur;
        return {};
    }

    uint32_t ch = 0;
    int      rc = decodeBytes_MultibyteChar(cur, strnlen(cur, 8), &ch);
    if (rc < 0)
    {
        // Multibyte decoding failed. Let's return something valid, though, so the iterator
        // doesn't hang due to never reaching the end of the string.
        ch = '?';
        if (end) *end = nullptr;
    }
    else
    {
        DE_ASSERT(rc > 0 || *cur == 0);
        if (end) *end = cur + rc;
    }
    return ch;
}

mb_iterator &mb_iterator::operator++()
{
    const char *end;
    decode(&end);
    if (end)
    {
        cur = end;
    }
    else
    {
        ++cur;
    }
    DE_ASSERT(cur >= start && cur <= start + strlen(start));
    return *this;
}

mb_iterator mb_iterator::operator++(int)
{
    mb_iterator i = *this;
    ++(*this);
    return i;
}

mb_iterator &mb_iterator::operator--()
{
    while (--cur > start)
    {
        const char *end;
        memset(&mb, 0, sizeof(mb));
        decode(&end);
        if (end) break;
    }
    return *this;
}

mb_iterator mb_iterator::operator--(int)
{
    mb_iterator i = *this;
    --(*this);
    return i;
}

mb_iterator mb_iterator::operator+(int offset) const
{
    mb_iterator i = *this;
    return i += offset;
}

mb_iterator &mb_iterator::operator+=(int offset)
{
    if (offset < 0)
    {
        while (offset++ < 0) --(*this);
    }
    else
    {
        while (offset-- > 0) ++(*this);
    }
    return *this;
}

mb_iterator mb_iterator::operator-(int offset) const
{
    mb_iterator i = *this;
    return i -= offset;
}

mb_iterator &mb_iterator::operator-=(int offset)
{
    return (*this) += -offset;
}

BytePos mb_iterator::pos(const String &reference) const
{
    return BytePos(cur - reference.c_str());
}

//------------------------------------------------------------------------------------------------

std::string stringf(const char *format, ...)
{
    // First determine the length of the result.
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    const int requiredLength = vsnprintf(nullptr, 0, format, args1);
    va_end(args1);
    // Format the output to a new string.
    std::string str(requiredLength, '\0');
    vsnprintf(&str[0], str.size() + 1, format, args2);
    va_end(args2);
    return str;
}

} // namespace de
