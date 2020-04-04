/**
 * @file str.hh
 * C++ wrapper for Str (ddstring_t).
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_DDSTRING_CPP_WRAPPER_HH
#define LIBDENG_DDSTRING_CPP_WRAPPER_HH

#include "liblegacy.h"
#include "str.h"

#include <QString>

namespace de {

/**
 * Minimal C++ wrapper for ddstring_t. @ingroup legacyData
 */
class Str {
public:
    Str(const char *text = 0) {
        Str_InitStd(&str);
        if (text) {
            Str_Set(&str, text);
        }
    }
    Str(const QString &text) {
        Str_InitStd(&str);
        Str_Set(&str, text.toUtf8());
    }
    ~Str() {
        // This should never be called directly.
        Str_Free(&str);
    }
    operator const char *(void) const {
        return str.str;
    }
    operator ddstring_t *(void) {
        return &str;
    }
    operator const ddstring_t *(void) const {
        return &str;
    }
private:
    ddstring_t str;
};

} // namespace de

#endif // LIBDENG_DDSTRING_CPP_WRAPPER_HH
