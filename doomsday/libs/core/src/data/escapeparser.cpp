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

#include "de/escapeparser.h"
#include "de/cstring.h"

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

void EscapeParser::parse(const String &textWithEscapes)
{
#if defined (DE_DEBUG)
    {
        const char *nullp = strchr(textWithEscapes, 0);
        if (nullp != textWithEscapes.end())
        {
            debug("string size: %zu\nfound zero at: %zu",
                  textWithEscapes.size(),
                  nullp - textWithEscapes.c_str());
            debug("[%s] ^ [%s]",
                  String(nullp - 20, nullp).c_str(),
                  String(nullp + 1, nullp + 20).c_str());
        }
    }
#endif

    d->original = textWithEscapes;
    d->plain.clear();

//    const char *           origEnd = d->original.end();
    String::const_iterator pos     = d->original.begin();
    String::const_iterator end     = pos;

    const char escape = '\x1b';

    for (;;)
    {
        while (*end != escape && *end) // != origEnd)
        {
            ++end;
//            DE_ASSERT(*end || end == origEnd);
        }

        if (*end == escape)
        {
            const CString plain{pos, end};
            if (plain.size() > 0)
            {
                DE_ASSERT(!plain.contains(escape));
                DE_NOTIFY(PlainText, i) { i->handlePlainText(plain); }
                d->plain += plain;
            }

            // Check the escape sequences.
            auto escStart = ++end;
            Char ch = *end++; // First character of the escape sequence.
            switch (ch)
            {
            case '(':
            case '[':
            case '{': {
                // Find the matching end.
                const Char closing = (ch == '('? ')' : ch == '['? ']' : '}');
                while (*end != closing && *end/* != d->original.end()*/)
                {
                    end++;
                }
                break; }

            case 'T':
                // One character for the tab stop.
                end++;
                break;

            default:
                break;
            }

            DE_NOTIFY(EscapeSequence, i)
            {
                i->handleEscapeSequence({escStart, end});
            }

            // Advance the scanner.
            pos = end;
        }
        else
        {
            // Final plain text range.
            const CString plain{pos, end};
            if (plain.size() > 0)
            {
                DE_ASSERT(!plain.contains(escape));
                DE_NOTIFY(PlainText, i) { i->handlePlainText(plain); }
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
