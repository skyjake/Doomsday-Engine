/** @file escapeparser.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/EscapeParser"

namespace de {

DENG2_PIMPL_NOREF(EscapeParser)
{
    String original;
    String plain;
};

EscapeParser::EscapeParser() : d(new Instance)
{}

void EscapeParser::parse(String const &textWithEscapes)
{
    d->original = textWithEscapes;
    d->plain.clear();

    Rangei range;

    forever
    {
        range.end = d->original.indexOf(QChar('\x1b'), range.start);
        if(range.end >= 0)
        {
            // Empty ranges are ignored.
            if(range.size() > 0)
            {
                DENG2_FOR_AUDIENCE(PlainText, i)
                {
                    i->handlePlainText(range);
                }

                // Update the plain text.
                d->plain += d->original.substr(range);
            }

            // Check the escape sequences.
            int escLen = 2;
            char ch = d->original[range.end + 1].toLatin1();
            switch(ch)
            {
            case '(':
            case '[':
            case '{':
            case '<': {
                // Find the matching end.
                int end = d->original.indexOf(ch == '('? ')' : ch == '['? ']' :
                                              ch == '{'? '}' : '>', range.end + 1);
                if(end < 0) end = d->original.size() - 1;
                escLen = end - range.end + 1;
                break; }

            case 'T':
                escLen = 3;
                break;

            default:
                break;
            }

            DENG2_FOR_AUDIENCE(EscapeSequence, i)
            {
                i->handleEscapeSequence(Rangei(range.end + 1, range.end + escLen));
            }

            // Advance the scanner.
            range.start = range.end + escLen;
        }
        else
        {
            // Final plain text range.
            range.end = d->original.size();
            if(range.size() > 0)
            {
                DENG2_FOR_AUDIENCE(PlainText, i)
                {
                    i->handlePlainText(range);
                }
                d->plain += d->original.substr(range);
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
