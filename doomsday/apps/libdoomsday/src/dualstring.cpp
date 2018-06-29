#if 0 /* obsolete */

/** @file dualstring.cpp
 *
 * Utility class for strings that need both Unicode and C-string access.
 * @ingroup data
 *
 * @authors Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/dualstring.h"

using namespace de;

DualString::DualString()
{
    _str = Str_NewStd();
}

DualString::DualString(const DualString& other) : String(other)
{
    _str = Str_NewStd();
    Str_Copy(_str, other._str);
}

DualString::DualString(const String& other) : String(other)
{
    _str = Str_NewStd();
}

void DualString::clear()
{
    String::clear();
    Str_Truncate(_str, 0); // existing pointer remains valid
}

DualString::~DualString()
{
    Str_Delete(_str);
}

DualString& DualString::operator = (const DualString& other)
{
    static_cast<String&>(*this) = other;
    Str_Copy(_str, other._str);
    return *this;
}

DualString& DualString::operator = (const char* cStr)
{
    static_cast<String&>(*this) = cStr;
    return *this;
}

DualString& DualString::operator = (const String& str)
{
    static_cast<String&>(*this) = str;
    return *this;
}

const ::Str* DualString::toStrAscii() const
{
    Str_Set(_str, toLatin1().constData());
    return _str;
}

const ::Str* DualString::toStrUtf8() const
{
    Str_Set(_str, toUtf8().constData());
    return _str;
}

::Str* DualString::toStr()
{
    Str_Set(_str, toUtf8().constData());
    return _str;
}

void DualString::update()
{
    static_cast<String&>(*this) = QString::fromUtf8(Str_Text(_str));
}

const char* DualString::asciiCStr()
{
    return Str_Text(toStrAscii());
}

const char* DualString::utf8CStr()
{
    return Str_Text(toStrUtf8());
}

#endif // 0
