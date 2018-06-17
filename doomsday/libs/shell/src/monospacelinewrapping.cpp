/** @file monospacelinewrapping.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/MonospaceLineWrapping"
#include <de/math.h>

namespace de { namespace shell {

MonospaceLineWrapping::MonospaceLineWrapping()
{}

bool MonospaceLineWrapping::isEmpty() const
{
    return _lines.isEmpty();
}

void MonospaceLineWrapping::clear()
{
    _lines.clear();
}

void MonospaceLineWrapping::wrapTextToWidth(String const &text, String::CharPos maxWidth)
{
    clear();
    if (maxWidth < 1) return; // No room to wrap.

    const Char            newline   = '\n';
    const String::CharPos lineWidth = maxWidth;

    dsize curLineWidth = 0;
    mb_iterator begin = text.data();
    const mb_iterator textEnd = text.data() + text.size();
    for (;;)
    {
        // Newlines always cause a wrap.
        mb_iterator end = begin;
        while (curLineWidth < lineWidth.index && end != textEnd && *end != newline)
        {
            ++end;
            ++curLineWidth;
        }

        if (end == textEnd)
        {
            // Time to stop.
            _lines << WrappedLine(String::ByteRange(begin.pos(), text.sizeb()));
            break;
        }

        // Find a good break point.
        const mb_iterator lineEnding = end;
        while (!iswspace(*end))
        {
            --end;
            if (end == begin)
            {
                // Ran out of non-space chars, force a break.
                end = lineEnding;
                break;
            }
        }

        if (*end == newline)
        {
            // The newline will be omitted from the wrapped lines.
            _lines << WrappedLine(String::ByteRange(begin.pos(), end.pos()));
            begin = end + 1;
        }
        else
        {
            if (iswspace(*end)) ++end;
            _lines << WrappedLine(String::ByteRange(begin.pos(), end.pos()));
            begin = end;
        }
    }

    // Mark the final line.
    _lines.last().isFinal = true;
}

int MonospaceLineWrapping::width() const
{
    dsize w = 0;
    for (duint i = 0; i < _lines.size(); ++i)
    {
        WrappedLine const &span = _lines[i];
        w = de::max(w, span.range.size().index);
    }
    return int(w);
}

int MonospaceLineWrapping::height() const
{
    return int(_lines.size());
}

int MonospaceLineWrapping::rangeWidth(Rangei const &range) const
{
    return range.size();
}

int MonospaceLineWrapping::indexAtWidth(Rangei const &range, int width) const
{
    if (width <= range.size())
    {
        return range.start + width;
    }
    return range.end;
}

}} // namespace de::shell
