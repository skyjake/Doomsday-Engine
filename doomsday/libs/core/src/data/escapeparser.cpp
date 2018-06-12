/** @file escapeparser.cpp
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

#include "de/EscapeParser"
#include "de/CString"

namespace de {

DE_PIMPL_NOREF(EscapeParser)
{
    String original;
    String plain;

    DE_PIMPL_AUDIENCE(PlainText)
    DE_PIMPL_AUDIENCE(EscapeSequence)
};

DE_AUDIENCE_METHOD(EscapeParser, PlainText)
DE_AUDIENCE_METHOD(EscapeParser, EscapeSequence)

EscapeParser::EscapeParser() : d(new Impl)
{}

void EscapeParser::parse(String const &textWithEscapes)
{
    d->original = textWithEscapes;
    d->plain.clear();

    const String::const_iterator origEnd = d->original.end();
    String::const_iterator       pos     = d->original.begin();
    String::const_iterator       end     = pos;

    const char escape = '\x1b';

    for (;;)
    {
        while (*end != escape && end != origEnd)
        {
            ++end;
        }

        if (*end == escape)
        {
            const CString plain{pos, end};
            if (plain.size() > 0)
            {
                DE_FOR_AUDIENCE2(PlainText, i) { i->handlePlainText(plain); }
                d->plain += plain;
            }

            // Check the escape sequences.
            //int escLen = 2;
            auto escStart = ++end;
            Char ch = *end++;
            switch (ch)
            {
            case '(':
            case '[':
            case '{': {
                // Find the matching end.
                //auto end = d->original.indexOf(ch == '('? ')' : ch == '['? ']' : '}', range.end + 1);
                const char closing = (ch == '('? ')' : ch == '['? ']' : '}');
                while (*end != closing && end != d->original.end())
                {
                    end++;
                }
                //if (end < 0) end = d->original.size() - 1;
//                escLen = escEnd - end;
                break; }

            case 'T':
                end += 2;//escLen = 3;
                break;

            default:
                end++;
                break;
            }

            DE_FOR_AUDIENCE2(EscapeSequence, i)
            {
                i->handleEscapeSequence({escStart, end});
            }

            // Advance the scanner.
            pos = end;
        }
        else
        {
            // Final plain text range.
            const CString plain{pos, origEnd};
            if (plain.size() > 0)
            {
                DE_FOR_AUDIENCE2(PlainText, i) { i->handlePlainText(plain); }
                d->plain += plain;
            }
            break;
        }
    }
}

String EscapeParser::originalText() const
{
    return d->original;
}

String EscapeParser::plainText() const
{
    return d->plain;
}

} // namespace de
