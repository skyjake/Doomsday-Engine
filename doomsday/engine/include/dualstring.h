/**
 * @file dualstring.h
 * Utility class for strings that need both Unicode and C-string access.
 * @ingroup data
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_DUALSTRING_CPP
#define LIBDENG_DUALSTRING_CPP

#include <de/String>
#include <de/str.h>

namespace de {

/**
 * Maintains a secondary, read-only C-style string side-by-side with a full
 * de::String. This class should only be used to support legacy code.
 *
 * The secondary Str is only updated on demand.
 */
class DualString : public String
{
public:
    DualString();

    DualString(const String& other);

    virtual ~DualString();

    DualString& operator = (const char* cStr);

    DualString& operator = (const String& str);

    void clear();

    /**
     * Returns a read-only pointer to the string's other half (ASCII encoding).
     * @return Str instance.
     */
    const ::Str* toStrAscii() const;

    /**
     * Returns a read-only pointer to the string's other half (UTF-8 encoding).
     * @return Str instance.
     */
    const ::Str* toStrUtf8() const;

    /**
     * Returns a modifiable Str (UTF-8). After making changes, you have to call
     * update() to copy the new contents to the de::String half.
     */
    ::Str* toStr();

    /**
     * Copies the Str contents, assumed to be in UTF-8 encoding, to the
     * de::String side.
     */
    void update();

    /**
     * Converts the contents of the string to ASCII and returns a read-only
     * pointer to the ASCII, null-terminated C style string. Ownership of the
     * returned string is kept by DualString. The returned pointer is only
     * valid until contents of the DualString remain unchanged; until that
     * time, the caller may hang on to the returned string pointer.
     *
     * @return String contents as ASCII.
     */
    const char* asciiCStr();

    /**
     * Works like asciiCStr() but converts the C style string to UTF-8 encoding.
     *
     * @return String contents as UTF-8.
     */
    const char* utf8CStr();

private:
    ::Str* _str;
};

}

#endif // LIBDENG_DUALSTRING_CPP
