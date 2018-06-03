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

#include "de/App"
#include "de/String"
#include "de/Block"
#include "de/charsymbols.h"

#include <c_plus/path.h>
#include <cstdio>
#include <cstdarg>
#include <cerrno>

namespace de {

const String::size_type String::npos = String::size_type(-1);

String::String()
{}

String::String(const String &other)
{
    initCopy_String(&_str, &other._str);
}

String::String(String &&moved)
{
    _str = moved._str;
    iZap(moved._str);
}

String::String(const iString *other)
{
    initCopy_String(&_str, other);
}

String::String(const std::string &text)
{
    initCStrN_String(&_str, text.data(), text.size());
}

String::String(const char *nullTerminatedCStr)
{
    initCStr_String(&_str, nullTerminatedCStr);
}

String::String(const Char *nullTerminatedWideStr)
{
    initWide_String(&_str, nullTerminatedWideStr);
}

String::String(char const *cStr, int length)
{
    initCStrN_String(&_str, cStr, length);
}

String::String(char const *cStr, dsize length)
{
    initCStrN_String(&_str, cStr, length);
}

String::String(dsize length, char ch)
{
    init_Block(&_str.chars, length);
    fill_Block(&_str.chars, ch);
}

String::String(const char *start, const char *end)
{
    initCStrN_String(&_str, start, end - start);
}

String::String(dsize length, iChar ch)
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

//String::String(Qconst String &str, size_type index, size_type length)
//    : QString(str.mid(index, length))
//{}

String::~String()
{
    deinit_String(&_str);
}

std::wstring String::toWideString() const
{
    std::wstring ws;
    for (auto i : *this)
    {
        ws.push_back(i);
    }
    return ws;
}

String String::substr(int position, int n) const
{
    iString *sub = mid_String(&_str, position, n);
    String s(sub);
    delete_String(sub);
    return s;
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

String &String::operator+=(iChar ch)
{
    appendChar_String(&_str, ch);
    return*this;
}

String &String::operator+=(const char *cStr)
    {
    appendCStr_String(&_str, cStr);
    return *this;
    }

String &String::operator+=(const String &other)
{
    append_String(&_str, &other._str);
    return *this;
}

String String::substr(String::size_type pos, String::size_type n) const
{
    iString *sub = mid_String(&_str, pos, n);
    String s(sub);
    delete_String(sub);
    return s;
}

iChar String::first() const
{
    return empty() ? 0 : *begin();
}

iChar String::last() const
{
    return empty() ? 0 : *end();
}

String String::operator/(const String &path) const
{
    return concatenatePath(path);
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

String String::concatenatePath(const String &other, iChar dirChar) const
{
    if ((dirChar == '/' || dirChar == '\\') && isAbsolute_Path(&other._str))
    {
        // The other path is absolute - use as is.
        return other;
    }
    return concatenateRelativePath(other, dirChar);
}

String String::concatenateRelativePath(const String &other, iChar dirChar) const
{
    if (other.isEmpty()) return *this;

    int const startPos = (other.first() == dirChar? 1 : 0);

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
    static const RegExp reg("\\s+");
    String s = *this;
    s.replace(reg, " ");
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
    iString *low = toLower_String(&_str);
    String str(low);
    delete_String(low);
    return str;
}

String String::upper() const
{
    iString *upr = toUpper_String(&_str);
    String str(upr);
    delete_String(upr);
    return str;
}

String String::upperFirstChar() const
{
    if (isEmpty()) return "";
    if (size() == 1) return upper();
    return towupper(first()) + substr(1);
}

String String::fileName(Char dirChar) const
{
    int pos = lastIndexOf(dirChar);
    if (pos >= 0)
    {
        return mid(pos + 1);
    }
    return *this;
}

String String::fileNameWithoutExtension() const
{
    String name = fileName();
    int pos = name.lastIndexOf('.');
    if (pos > 0)
    {
        return name.mid(0, pos);
    }
    return name;
}

String String::fileNameExtension() const
{
    int pos      = lastIndexOf('.');
    int slashPos = lastIndexOf('/');
    if (pos > 0)
    {
        // If there is a directory included, make sure there it at least
        // one character's worth of file name before the period.
        if (slashPos < 0 || pos > slashPos + 1)
        {
            return mid(pos);
        }
    }
    return "";
}

String String::fileNamePath(QChar dirChar) const
{
    int pos = lastIndexOf(dirChar);
    if (pos >= 0)
    {
        return mid(0, pos);
    }
    return "";
}

String String::fileNameAndPathWithoutExtension(QChar dirChar) const
{
    return fileNamePath(dirChar) / fileNameWithoutExtension();
}

bool String::containsWord(const String &word) const
{
    if (word.isEmpty())
    {
        return false;
    }
    return RegExp(stringf("\\b%s\\b", word.c_str())).match(*this).hasMatch();
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

int String::commonPrefixLength(const String &str, CaseSensitivity sensitivity) const
{
    int count = 0;
    size_t len = std::min(str.size(), size());
    for (int i = 0; i < len; ++i, ++count)
    {
        if (sensitivity == CaseSensitive)
        {
            if (at(i) != str.at(i)) break;
        }
        else
        {
            if (at(i).toLower() != str.at(i).toLower()) break;
        }
    }
    return count;
}

String::const_iterator String::begin() const
{
    const_iterator i;
    init_StringConstIterator(&i.iter, &_str);
    return i;
}

String::const_iterator String::end() const
{
    const_iterator i;
    init_StringConstIterator(&i.iter, &_str);
    i.iter.pos = i.iter.next = constEnd_String(&_str);
    i.iter.remaining = 0;
    return i;
}

String::const_reverse_iterator String::rbegin() const
    {
    const_reverse_iterator i;
    init_StringReverseConstIterator(&i.iter, &_str);
    return i;
    }

String::const_reverse_iterator String::rend() const
{
    const_reverse_iterator i;
    init_StringReverseConstIterator(&i.iter, &_str);
    i.iter.pos = i.iter.next = constBegin_String(&_str);
    i.iter.remaining = 0;
    return i;
}

//dint String::compareWithCase(QChar const *a, QChar const *b, dsize count) // static
//{
//    return QString(a).leftRef(count).compare(QString(b).leftRef(count), Qt::CaseSensitive);
//}

//bool String::equals(QChar const *a, QChar const *b, dsize count) // static
//{
//    while (count--)
//    {
//        // Both strings the same length?
//        if (a->isNull() && b->isNull()) break;
//        // Mismatch?
//        if (*a != *b) return false;
//        // Advance.
//        a++;
//        b++;
//    }
//    return true;
//}

void String::skipSpace(const_iterator &i, const const_iterator &end)
{
    while (i != end && iswspace(*i)) ++i;
}

String String::format(const char *format, ...)
{
    va_list args;
    Block buffer;
    int neededSize = 512;

    for (;;)
    {
        buffer.resize(neededSize);

        va_start(args, format);
        int count = vsnprintf(reinterpret_cast<char *>(buffer.data()), buffer.size(), format, args);
        va_end(args);

        if (count >= 0 && count < neededSize)
        {
            buffer.resize(count);
            return fromUtf8(buffer);
        }

        if (count >= 0)
        {
            neededSize = count + 1;
        }
        else
        {
            neededSize *= 2; // Try bigger.
        }
    }
    return fromUtf8(buffer);
}

static inline bool isSign(Char ch)
{
    return ch == '-' || ch == '+';
}

dint String::toInt(bool *ok, int base, duint flags) const
{
    char *endp;
    auto value = std::strtol(*this, &endp, base);
    if (ok) *ok = (errno != ERANGE);
    if (!(flags & AllowSuffix) && !(*endp == 0 && isspace(*endp)))
    {
        // Suffix not allowed; consider this a failure.
        if (ok) *ok = false;
        value = 0;
        }
    return dint(value);
}

duint32 String::toUInt32(bool *ok, int base) const
{
    char *endp;
    const auto value = std::strtoul(*this, nullptr, base);
    if (ok) *ok = (errno != ERANGE);
    return duint32(value);
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
       .replace("\"", "\\\"");
    return esc;
}

Block String::toPercentEncoding() const
{
    return QUrl::toPercentEncoding(*this);
}

String String::truncateWithEllipsis(dsize maxLength) const
{
    if (size() <= maxLength)
    {
        return *this;
    }
    return mid(0, maxLength/2 - 1) + "..." + right(maxLength/2 - 1);
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

String String::join(const StringList &stringList, const String &sep)
{
    if (stringList.isEmpty()) return {};

    String joined = stringList.at(0);
    for (int i = 1; i < stringList.size(); ++i)
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
    //QTextStream output(&result);

    // An argument comes here.
    bool rightAlign = true;
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
    while (iswdigit(*formatIter))
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
        while (iswdigit(*formatIter))
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
        result += Char(arg.asNumber());
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
    if (maxWidth && result.size() > maxWidth)
    {
        // Cut it.
        result = result.substr(!rightAlign ? 0 : (result.size() - maxWidth), maxWidth);
    }
    if (result.size() < minWidth)
    {
        // Pad it.
        String padding = String(minWidth - result.size(), ' ');
        if (rightAlign)
        {
            result = padding + result;
        }
        else
        {
            result += padding;
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
    for (iChar ch : *this)
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

String String::fromUtf8(char const *nullTerminatedCStr)
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
    for (dbyte ch : chars)
    {
        conv += Char(codePage437ToUnicode(ch));
    }
    return conv;
}

String String::fromPercentEncoding(const Block &percentEncoded) // static
{
    return QUrl::fromPercentEncoding(percentEncoded);
}

//size_t qchar_strlen(Char const *str)
//{
//    if (!str) return 0;

//    size_t len = 0;
//    while (str->unicode() != 0) { ++str; ++len; }
//    return len;
//}

std::string stringf(const char *format, ...)
{
    // First determine the length of the result.
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    const int requiredLength = vsprintf(nullptr, format, args1);
    va_end(args1);
    // Format the output to a new string.
    std::string str(requiredLength);
    vsprintf(str.data(), format, args2);
    va_end(args2);
    return str;
}

} // namespace de
