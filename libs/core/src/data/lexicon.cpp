/** @file lexicon.cpp  Lexicon containing terms and grammatical rules.
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

#include "de/lexicon.h"

namespace de {

DE_PIMPL_NOREF(Lexicon)
{
    Terms  terms;
    String extraChars;
    bool   caseSensitive;

    Impl() : caseSensitive(false) {}
};

Lexicon::Lexicon() : d(new Impl)
{}

Lexicon::Lexicon(const Lexicon &other) : d(new Impl(*other.d))
{}

Lexicon &Lexicon::operator = (const Lexicon &other)
{
    d.reset(new Impl(*other.d));
    return *this;
}

void Lexicon::setAdditionalWordChars(const String &chars)
{
    d->extraChars = chars;
}

void Lexicon::setCaseSensitive(bool sensitive)
{
    d->caseSensitive = sensitive;
}

void Lexicon::addTerm(const String &term)
{
    d->terms.insert(term);
}

Lexicon::Terms Lexicon::terms() const
{
    return d->terms;
}

String Lexicon::additionalWordChars() const
{
    return d->extraChars;
}

bool Lexicon::isWordChar(Char ch) const
{
    // Default word characters.
    if (ch.isAlphaNumeric()) return true;
    return d->extraChars.contains(ch);
}

bool Lexicon::isCaseSensitive() const
{
    return d->caseSensitive;
}

} // namespace de
