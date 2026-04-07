/** @file regexp.h  Perl-compatible regular expressions.
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

#ifndef LIBCORE_REGEXP_H
#define LIBCORE_REGEXP_H

#include "de/cstring.h"
#include <the_Foundation/object.h>
#include <the_Foundation/regexp.h>

namespace de {

class DE_PUBLIC RegExpMatch
{
public:
    iRegExpMatch match;   ///< Match results.
    String       subject; // ensures a persistent copy exists

    RegExpMatch();

    const char *begin() const;
    const char *end() const;

    void    clear();
    String  captured(int index = 0) const;
    CString capturedCStr(int index = 0) const;
};

/**
 * Perl-compatible regular expression.
 *
 * @ingroup data
 */
class DE_PUBLIC RegExp
{
public:
    RegExp(const String &expression = {}, Sensitivity cs = CaseSensitive);

    bool match(const String &subject, RegExpMatch &match) const;
    bool hasMatch(const String &subject) const;
    bool exactMatch(const String &subject) const;
    bool exactMatch(const String &subject, RegExpMatch &match) const;

    static const RegExp WHITESPACE;

private:
    tF::ref<iRegExp> _d;
};

} // namespace de

#endif // LIBCORE_REGEXP_H
