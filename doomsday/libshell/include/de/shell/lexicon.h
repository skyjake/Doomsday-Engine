/** @file lexicon.h  Lexicon containing terms and grammatical rules.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "libshell.h"
#include <QSet>
#include <de/String>

namespace de {
namespace shell {

/**
 * Lexicon containing terms and grammatical rules. By default, the lexicon is
 * case insensitive.
 */
class LIBSHELL_PUBLIC Lexicon
{
public:
    typedef QSet<String> Terms;

public:
    Lexicon();
    Lexicon(Lexicon const &other);
    Lexicon &operator = (Lexicon const &other);

    Terms terms() const;

    String additionalWordChars() const;

    bool isWordChar(QChar ch) const;

    bool isCaseSensitive() const;

    void addTerm(String const &term);

    void setAdditionalWordChars(String const &chars);

    void setCaseSensitive(bool sensitive);

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_LEXICON_H
