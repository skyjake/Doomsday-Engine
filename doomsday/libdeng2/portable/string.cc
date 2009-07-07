/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/string.h"

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef UNIX
#   include <strings.h> // strcasecmp
#endif

#ifdef WIN32
#   define strcasecmp _stricmp
#endif

using namespace de;

using std::string;

String::String(const std::string& text) : string(text)
{}

String::String(const char* cStr) : string(cStr)
{}

String::String(const IByteArray& array)
{
	Size len = array.size();
	Byte* buffer = new Byte[len + 1];
	array.get(0, buffer, len);
    buffer[len] = 0;
	set(0, buffer, len);
	delete [] buffer;
}

String::String(const String& other) : string(other)
{}

bool String::beginsWith(const std::string& s) const
{
    if(size() < s.size())
    {
        // This is too short to be a match.
        return false;
    }
    return std::equal(s.begin(), s.end(), begin());
}

bool String::contains(const std::string& s) const
{
    return find(s) != npos;
}

String String::concatenatePath(const std::string& other, char dirChar) const
{
    if(!other.empty() && other[0] == dirChar)
    {
        // The other begins with a slash, therefore it's an absolute path.
        // Use it as is.
        return other;
    }

    String result = *this;
    // Do a path combination. Check for a slash.
    if(!empty() && *rbegin() != dirChar)
    {
        result += dirChar;
    }
    result += other;
    return result;
}

String String::concatenateNativePath(const std::string& nativePath) const
{
#ifdef UNIX
    return concatenatePath(nativePath);
#endif

#ifdef WIN32
    /// @todo Check for "(drive-letter):" and "(drive-letter):\".

    return concatenatePath(nativePath, '\\');
#endif
}

String String::strip() const
{
    return leftStrip().rightStrip();
}

String String::leftStrip() const
{
    String result = *this;
    String::iterator endOfSpace = result.begin();
    while(endOfSpace != result.end() && std::isspace(*endOfSpace)) 
    {
        ++endOfSpace;   
    }
    result.erase(result.begin(), endOfSpace);
    return result;
}

String String::rightStrip() const
{
    String result = *this;
    while(result.size() && std::isspace(*result.rbegin()))
    {
        result.erase(result.size() - 1, 1);
    }
    return result;
}

String String::lower() const
{
    std::ostringstream result;
    for(String::const_iterator i = begin(); i != end(); ++i)
    {
        result << char(std::tolower(*i));
    }
    return result.str();
}

String String::upper() const
{    
    std::ostringstream result;
    for(String::const_iterator i = begin(); i != end(); ++i)
    {
        result << char(std::toupper(*i));
    }
    return result.str();
}

String::Size String::size() const
{
	return std::string::size();
}

void String::get(Offset at, Byte* values, Size count) const
{
	if(at + count > size())
	{
		throw OffsetError("String::get", "Out of range");
	}

	memcpy(values, c_str() + at, count);
}

void String::set(Offset at, const Byte* values, Size count)
{
	replace(at, count, reinterpret_cast<const char*>(values));
}

std::wstring String::wide() const
{
    return stringToWide(*this);
}

std::wstring String::stringToWide(const std::string& str)
{
    duint inputSize = str.size();
    const dbyte* input = reinterpret_cast<const dbyte*>(str.c_str());
    const dbyte* inputEnd = input + inputSize;
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

String String::wideToString(const std::wstring& str)
{
    const wchar_t* input = &str[0];
    const wchar_t* inputEnd = input + str.size();
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

void String::advanceFormat(std::string::const_iterator& i, const std::string::const_iterator& end)
{
    ++i;
    if(i == end)
    {
        throw IllegalPatternError("String::advanceFormat", 
            "Incomplete formatting instructions");
    }
}

String String::patternFormat(std::string::const_iterator& formatIter, 
    const std::string::const_iterator& formatEnd, const IPatternArg& arg)
{
    advanceFormat(formatIter, formatEnd);

    std::ostringstream output;
    
    // An argument comes here.
    bool rightAlign = true;
    dint maxWidth = 0;
    dint minWidth = 0;

    if(*formatIter == '%')
    {
        // Escaped.
        return std::string(1, *formatIter);
    }
    if(*formatIter == '-')
    {
        // Left aligned.
        rightAlign = false;
        advanceFormat(formatIter, formatEnd);
    }
    std::string::const_iterator k = formatIter;
    while(std::isdigit(*formatIter))
    {
        advanceFormat(formatIter, formatEnd);
    }
    if(k != formatIter)
    {
        // Got the minWidth.
        minWidth = std::atoi(std::string(k, formatIter).c_str());
    }
    if(*formatIter == '.')
    {
        advanceFormat(formatIter, formatEnd);
        k = formatIter;
        // There's also a maxWidth.
        while(std::isdigit(*formatIter))
        {
            advanceFormat(formatIter, formatEnd);
        }
        maxWidth = std::atoi(std::string(k, formatIter).c_str());
    }
    
    // Finally, the type formatting.
    std::ostringstream valueStr;
    switch(*formatIter)
    {
    case 's':
        valueStr << arg.asText();
        break;
        
    case 'i':
    case 'd':
        valueStr << dint(arg.asNumber());
        break;
    
    case 'X':
        valueStr << std::uppercase;
    case 'x':
        valueStr << "0x" << std::hex << dint(arg.asNumber()) <<
            std::dec << std::nouppercase;
        break;
        
    case 'f':
        // Max width is interpreted as the number of decimal places.
        valueStr << std::fixed << std::setprecision(maxWidth? maxWidth : 3) << arg.asNumber();
        maxWidth = 0;
        break;
        
    default:
        throw IllegalPatternError("Log::Entry::str", 
            "Unknown format character '" + std::string(1, *formatIter) + "'");
    }
    
    // Align and fit.
    std::string value = valueStr.str();
    if(maxWidth && value.size() > maxWidth)
    {
        // Cut it.
        value = value.substr(!rightAlign? 0 : value.size() - maxWidth, maxWidth);                    
    }
    if(value.size() < minWidth)
    {
        // Pad it.
        std::string padding = std::string(minWidth - value.size(), ' ');
        if(rightAlign)
        {
            value = padding + value;
        }
        else
        {
            value += padding;
        }
    }
    return value;
}

String String::fileName(const std::string& path)
{
    string::size_type pos = path.find_last_of('/');

    if(pos != string::npos)
    {
        return path.substr(pos + 1);
    }
    return path;
}

String String::fileNameExtension(const std::string& path)
{
    string::size_type pos = path.find_last_of('.');
    string::size_type slashPos = path.find_last_of('/');
    
    if(pos != string::npos && pos > 0)
    {
        // If there is a directory included, make sure there it at least
        // one character's worth of file name before the period.
        if(slashPos == string::npos || pos > slashPos + 1)
        {
            return path.substr(pos);
        }
    }
    return "";
}

String String::fileNamePath(const std::string& path)
{
    string::size_type pos = path.find_last_of('/');
    
    if(pos != string::npos)
    {
        return path.substr(0, pos);
    }
    return "";
}

dint String::compareWithCase(const std::string& a, const std::string& b)
{
    return a.compare(b);
}

dint String::compareWithoutCase(const std::string& a, const std::string& b)
{
    return strcasecmp(a.c_str(), b.c_str());
}

void String::skipSpace(std::string::const_iterator& i, const std::string::const_iterator& end)
{
    while(i != end && std::isspace(*i)) ++i;
}
