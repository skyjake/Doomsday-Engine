#if 0 /* obsolete */

/**
 * @file dualstring.h
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

#ifndef LIBDOOMSDAY_DUALSTRING_H
#define LIBDOOMSDAY_DUALSTRING_H

#ifdef __cplusplus

#include "libdoomsday.h"
#include <de/string.h>
#include <de/legacy/str.h>

/**
 * Maintains a secondary, read-only C-style string (Str instance) side-by-side
 * with a full de::String. The secondary Str is only updated on demand, when
 * someone needs access to the C-style string (or Str). There are no guarantees
 * that a previously returned pointer to the Str contents remains valid or is
 * up to date after making changes to the de::String side.
 *
 * The only allowed situation where the string is modified via the Str side is
 * when one calls DualString::toStr() and then DualString::update() after the
 * changes have been done via Str.
 *
 * This class should only be used to support legacy code.
 */
class LIBDOOMSDAY_PUBLIC DualString : public de::String
{
public:
    DualString();

    DualString(const DualString& other);

    DualString(const String& other);

    virtual ~DualString();

    DualString& operator = (const DualString& other);

    DualString& operator = (const String& str);

    DualString& operator = (const char* cStr);

    /**
     * Empties the contents of both the de::String string and the secondary
     * Str instance. Existing const char* pointers remain valid.
     */
    void clear();

    /**
     * Returns a read-only pointer to the string's secondary side (ASCII
     * encoding). @return Str instance.
     */
    const Str* toStrAscii() const;

    /**
     * Returns a read-only pointer to the string's secondary side (UTF-8
     * encoding). @return Str instance.
     */
    const Str* toStrUtf8() const;

    /**
     * Returns a modifiable Str (UTF-8). After making changes, you have to call
     * update() to copy the new contents to the de::String side.
     */
    Str* toStr();

    /**
     * Copies the contents of the Str side, assumed to be in UTF-8 encoding, to
     * the de::String side.
     */
    void update();

    /**
     * Converts the contents of the string to ASCII and returns a read-only
     * pointer to the ASCII, null-terminated C style string. Ownership of the
     * returned string is kept by DualString. The returned pointer is only
     * valid while contents of the DualString remain unchanged; during this
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
    Str* _str;
};

#endif // __cplusplus

#endif // LIBDOOMSDAY_DUALSTRING_H

#endif // 0
