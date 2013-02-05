/** @file lexicon.cpp  Lexicon containing terms and grammatical rules.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/shell/Lexicon"

namespace de {
namespace shell {

Lexicon::Lexicon()
{}

void Lexicon::setAdditionalWordChars(String const &chars)
{
    _extraChars = chars;
}

void Lexicon::addTerm(String const &term)
{
    _terms.insert(term);
}

Lexicon::Terms Lexicon::terms() const
{
    return _terms;
}

String Lexicon::additionalWordChars() const
{
    return _extraChars;
}

bool Lexicon::isWordChar(QChar ch) const
{
    // Default word characters.
    if(ch.isLetterOrNumber()) return true;
    return _extraChars.contains(ch);
}

} // namespace shell
} // namespace de
