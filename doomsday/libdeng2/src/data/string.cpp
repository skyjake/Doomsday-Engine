/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/App"
#include "de/String"
#include "de/Block"

#include <QDir>
#include <QTextStream>

using namespace de;

String::size_type const String::npos = -1;

String::String()
{}

String::String(String const &other) : QString(other)
{}

String::String(QString const &text) : QString(text)
{}

#ifdef DENG2_QT_4_7_OR_NEWER
String::String(QChar const *nullTerminatedStr)
    : QString(nullTerminatedStr)
{}
#else
String::String(QChar const *nullTerminatedStr)
    : QString(nullTerminatedStr, qchar_strlen(nullTerminatedStr))
{}
#endif

String::String(QChar const *str, size_type length) : QString(str, length)
{}

String::String(char const *nullTerminatedCStr)
    : QString(QString::fromUtf8(nullTerminatedCStr))
{}

String::String(char const *cStr, size_type length)
    : QString(QString::fromUtf8(cStr, length))
{}

String::String(size_type length, QChar ch) : QString(length, ch)
{}

String::String(QString const &str, size_type index, size_type length)
    : QString(str.mid(index, length))
{}

String::String(const_iterator start, const_iterator end)
{
    for(const_iterator i = start; i < end; ++i)
    {
        append(*i);
    }
}

String::String(iterator start, iterator end)
{
    for(iterator i = start; i < end; ++i)
    {
        append(*i);
    }
}

QChar String::first() const
{
    if(empty()) return 0;
    return at(0);
}

QChar String::last() const
{
    if(empty()) return 0;
    return at(size() - 1);
}

String String::operator / (String const &path) const
{
    return concatenatePath(path);
}

String String::concatenatePath(String const &other, QChar dirChar) const
{
    if(QDir::isAbsolutePath(other))
    {
        // The other path is absolute - use as is.
        return other;
    }

    String result = *this;

    // Do a path combination. Check for a slash.
    if(!empty() && last() != dirChar)
    {
        result += dirChar;
    }
    result += other;
    return result;
}

String String::concatenateMember(String const &member) const
{
    if(member.first() == '.')
    {
        throw InvalidMemberError("String::concatenateMember", "Invalid: '" + member + "'");
    }
    return concatenatePath(member, '.');
}

String String::strip() const
{
    return trimmed();
}

String String::leftStrip() const
{
    int endOfSpace = 0;
    while(endOfSpace < size() && at(endOfSpace).isSpace())
    {
        ++endOfSpace;
    }
    return mid(endOfSpace);
}

String String::rightStrip() const
{
    int beginOfSpace = size() - 1;
    while(beginOfSpace >= 0 && at(beginOfSpace).isSpace())
    {
        --beginOfSpace;
    }
    return left(beginOfSpace + 1);
}

String String::lower() const
{
    return toLower();
}

String String::upper() const
{    
    return toUpper();
}

/*
std::wstring String::stringToWide(String const &str)
{
    duint inputSize = str.size();
    dbyte const *input = reinterpret_cast<dbyte const *>(str.c_str());
    dbyte const *inputEnd = input + inputSize;
    std::vector<wchar_t> output;
    duint c;
    duint d;
    duint trailing;

    while(input < inputEnd)
    {
        d = *input++;
        
        if(d < 0x80) 
        {
            c = d;
            trailing = 0;
        }
        else if(d < 0xc0)
        {
            throw ConversionError("String::stringToWide", "Trailing byte in leading position");
        }
        else if(d < 0xe0)
        {
            c = d & 0x1f;
            trailing = 1;
        }
        else if(d < 0xf0)
        {
            c = d & 0x0f;
            trailing = 2;
        }
        else if(d < 0xf8)
        {
            c = d & 0x07;
            trailing = 3;
        }
        else
        {
            throw ConversionError("String::stringToWide", "Bad input");
        }
        
        for(; trailing; trailing--)
        {
            if((input >= inputEnd) || (((d = *input++) & 0xc0) != 0x80))
            {
                throw ConversionError("String::stringToWide", "Bad input");
            }
            c <<= 6;
            c |= d & 0x3f;
        }
        
        if(c < 0x10000)
        {
            output.push_back(wchar_t(c));
        }
        else if(c < 0x110000)
        {
            c -= 0x10000;
            output.push_back(wchar_t(0xd800 | (c >> 10)));
            output.push_back(0xdc00 | (c & 0x03ff));
        }
        else
        {
            throw ConversionError("String::stringToWide", "Bad input");
        }
    }

    output.push_back(0);
    return std::wstring(&output[0]);
}

String String::wideToString(std::wstring const &str)
{
    const wchar_t *input = &str[0];
    const wchar_t *inputEnd = input + str.size();
    String output;
    duint c;
    duint d;
    dint bits;
    
    while(input < inputEnd)
    {
        c = *input++;
        if((c & 0xfc00) == 0xd800)
        {
            if((input < inputEnd) && (((d = *input++) & 0xfc00) == 0xdc00))
            {
                c &= 0x3ff;
                c <<= 10;
                c |= d & 0x3ff;
                c += 0x10000;
            }
            else
            {
                throw ConversionError("String::wideToString", "Bad input"); 
            }
        }
        
        if(c < 0x80)
        {
            output += dchar(c);
            bits = -6;
        }
        else if(c < 0x800)
        {
            output += dchar((c >> 6) | 0xc0);
            bits = 0;
        }
        else if(c < 0x10000)
        {
            output += dchar((c >> 12) | 0xe0);
            bits = 6;
        }
        else 
        {
            output += dchar((c >> 18) | 0xf0);
            bits = 12;
        }
        
        for(; bits > 0; bits -= 6)
        {
            output += dchar((c >> bits) & 0x3f);
        }
    }
    return output;
}
*/

String String::fileName() const
{
    size_type pos = lastIndexOf('/');
    if(pos >= 0)
    {
        return mid(pos + 1);
    }
    return *this;
}

String String::fileNameWithoutExtension() const
{
    String name = fileName();
    size_type pos = name.lastIndexOf('.');
    if(pos > 0)
    {
        return name.mid(0, pos);
    }
    return name;
}

String String::fileNameExtension() const
{
    size_type pos = lastIndexOf('.');
    size_type slashPos = lastIndexOf('/');
    if(pos > 0)
    {
        // If there is a directory included, make sure there it at least
        // one character's worth of file name before the period.
        if(slashPos < 0 || pos > slashPos + 1)
        {
            return mid(pos);
        }
    }
    return "";
}

String String::fileNamePath(QChar dirChar) const
{
    size_type pos = lastIndexOf(dirChar);
    if(pos >= 0)
    {
        return mid(0, pos);
    }
    return "";
}

dint String::compareWithCase(String const &str) const
{
    return compare(str, Qt::CaseSensitive);
}

dint String::compareWithoutCase(String const &str) const
{
    return compare(str, Qt::CaseInsensitive);
}

dint String::compareWithoutCase(const String &str, int n) const
{
    return left(n).compare(str.left(n), Qt::CaseInsensitive);
}

int String::commonPrefixLength(String const &str) const
{
    int count = 0;
    int len = qMin(str.size(), size());
    for(int i = 0; i < len; ++i, ++count)
    {
        if(at(i) != str.at(i)) break;
    }
    return count;
}

dint String::compareWithCase(QChar const *a, QChar const *b, dsize count)
{
    return QString(a, count).compare(QString(b, count), Qt::CaseSensitive);
}

void String::skipSpace(String::const_iterator &i, String::const_iterator const &end)
{
    while(i != end && (*i).isSpace()) ++i;
}

// Seems like an ommission on the part of QChar...
static inline bool isSign(QChar const &ch)
{
    return ch == '-' || ch == '+';
}

dint String::toInt(bool *ok, int base, IntConversionFlags flags) const
{
    String token = leftStrip();

    if(flags & AllowSuffix)
    {
        // Truncate at the first non-numeric, non-notation or sign character.
        int endOfNumber = 0;
        while(endOfNumber < token.size() &&
              (token.at(endOfNumber).isDigit() || (endOfNumber == 0 && isSign(token.at(endOfNumber))) ||
               ((base == 0 || base == 16) && endOfNumber <= 1 &&
                (token.at(endOfNumber) == QChar('x') || token.at(endOfNumber) == QChar('X')))))
        {
            ++endOfNumber;
        }
        token.truncate(endOfNumber);
    }

    return token.QString::toInt(ok, base);
}

void String::advanceFormat(String::const_iterator &i, String::const_iterator const &end)
{
    ++i;
    if(i == end)
    {
        throw IllegalPatternError("String::advanceFormat", 
            "Incomplete formatting instructions");
    }
}

String String::patternFormat(String::const_iterator &formatIter, 
                             String::const_iterator const &formatEnd,
                             IPatternArg const &arg)
{
    advanceFormat(formatIter, formatEnd);

    // An argument comes here.
    bool rightAlign = true;
    dint maxWidth = 0;
    dint minWidth = 0;

    if(*formatIter == '%')
    {
        // Escaped.
        return String(1, *formatIter);
    }
    if(*formatIter == '-')
    {
        // Left aligned.
        rightAlign = false;
        advanceFormat(formatIter, formatEnd);
    }
    String::const_iterator k = formatIter;
    while((*formatIter).isDigit())
    {
        advanceFormat(formatIter, formatEnd);
    }
    if(k != formatIter)
    {
        // Got the minWidth.
        minWidth = String(k, formatIter).toInt();
    }
    if(*formatIter == '.')
    {
        advanceFormat(formatIter, formatEnd);
        k = formatIter;
        // There's also a maxWidth.
        while((*formatIter).isDigit())
        {
            advanceFormat(formatIter, formatEnd);
        }
        maxWidth = String(k, formatIter).toInt();
    }
    
    // Finally, the type formatting.
    QString result;
    QTextStream output(&result);

    switch((*formatIter).toLatin1())
    {
    case 's':
        output << arg.asText();
        break;
        
    case 'b':
        output << (int(arg.asNumber())? "true" : "false");
        break;

    case 'c':
        output << QChar(ushort(arg.asNumber()));
        break;

    case 'i':
    case 'd':
        output << dint64(arg.asNumber());
        break;

    case 'u':
        output << duint64(arg.asNumber());
        break;
    
    case 'X':
        output << uppercasedigits;
    case 'x':
        output << "0x" << hex << dint64(arg.asNumber()) << dec << lowercasedigits;
        break;

    case 'p':
        output << "0x" << hex << dintptr(arg.asNumber()) << dec;
        break;
        
    case 'f':
        // Max width is interpreted as the number of decimal places.
        output << fixed << qSetRealNumberPrecision(maxWidth? maxWidth : 3) << arg.asNumber();
        maxWidth = 0;
        break;
        
    default:
        throw IllegalPatternError("Log::Entry::str", 
            "Unknown format character '" + String(1, *formatIter) + "'");
    }

    output.flush();
    
    // Align and fit.
    if(maxWidth && result.size() > maxWidth)
    {
        // Cut it.
        result = result.mid(!rightAlign? 0 : result.size() - maxWidth, maxWidth);
    }
    if(result.size() < minWidth)
    {
        // Pad it.
        String padding = String(minWidth - result.size(), ' ');
        if(rightAlign)
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
    return QString::toUtf8();
}

Block String::toLatin1() const
{
    return QString::toLatin1();
}

String String::fromUtf8(IByteArray const &byteArray)
{
    return QString::fromUtf8(reinterpret_cast<char *>(Block(byteArray).data()));
}

String String::fromLatin1(IByteArray const &byteArray)
{
    return QString::fromLatin1(reinterpret_cast<char const *>(Block(byteArray).data()));
}

size_t de::qchar_strlen(QChar const *str)
{
    if(!str) return 0;

    size_t len = 0;
    while(str->unicode() != 0) { ++str; ++len; }
    return len;
}
