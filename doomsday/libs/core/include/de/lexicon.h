/** @file lexicon.h  Lexicon containing terms and grammatical rules.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_LEXICON_H
#define LIBSHELL_LEXICON_H

#include "de/string.h"
#include "de/set.h"

namespace de {

/**
 * Lexicon containing terms and grammatical rules. By default, the lexicon is
 * case insensitive.
 *
 * @ingroup shell
 */
class DE_PUBLIC Lexicon
{
public:
    typedef Set<String> Terms;

public:
    Lexicon();
    Lexicon(const Lexicon &other);
    Lexicon &operator=(const Lexicon &other);

    Terms  terms() const;
    String additionalWordChars() const;
    bool   isWordChar(Char ch) const;
    bool   isCaseSensitive() const;

    void addTerm(const String &term);
    void setAdditionalWordChars(const String &chars);
    void setCaseSensitive(bool sensitive);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBSHELL_LEXICON_H
