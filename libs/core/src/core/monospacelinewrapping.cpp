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

#include "de/monospacelinewrapping.h"
#include <de/math.h>

namespace de {

MonospaceLineWrapping::MonospaceLineWrapping()
{}

bool MonospaceLineWrapping::isEmpty() const
{
    return _lines.isEmpty();
}

void MonospaceLineWrapping::clear()
{
    _lines.clear();
    _text.clear();
}

void MonospaceLineWrapping::wrapTextToWidth(const String &text, WrapWidth maxWidth)
{
    clear();
    _text = text;

    if (maxWidth < 1) return; // No room to wrap.

    const Char      newline   = '\n';
    const WrapWidth lineWidth = maxWidth;

    mb_iterator       begin   = text.data();
    const mb_iterator textEnd = text.data() + text.size();

    for (;;)
    {
        WrapWidth curLineWidth = 0;

        // Newlines always cause a wrap.
        mb_iterator end = begin;
        while (curLineWidth < lineWidth && end != textEnd && *end != newline)
        {
            ++end;
            ++curLineWidth;
        }

        if (end == textEnd)
        {
            // Time to stop.
            _lines << WrappedLine(CString(begin, textEnd), curLineWidth);
            break;
        }

        // Find a good break point.
        const mb_iterator lineEnding = end;
        while (!(*end).isSpace())
        {
            --end;
            --curLineWidth;
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
            _lines << WrappedLine(CString(begin, end), curLineWidth);
            begin = end + 1;
        }
        else
        {
            if ((*end).isSpace()) ++end;
            _lines << WrappedLine(CString(begin, end), curLineWidth);
            begin = end;
        }
    }

    // Mark the final line.
    _lines.last().isFinal = true;
}

WrapWidth MonospaceLineWrapping::width() const
{
    WrapWidth w = 0;
    for (const auto &line : _lines)
    {
        w = de::max(w, line.width);
    }
    return w;
}

int MonospaceLineWrapping::height() const
{
    return _lines.sizei();
}

WrapWidth MonospaceLineWrapping::rangeWidth(const CString &range) const
{
    WrapWidth w = 0;
    for (mb_iterator i = range.begin(), end = range.end(); i != end; ++i)
    {
        ++w;
    }
    return w;
}

BytePos MonospaceLineWrapping::indexAtWidth(const CString &range, WrapWidth width) const
{
    mb_iterator i = range.begin();
    while (width-- > 0)
    {
        ++i;
    }
    return range.begin().pos(_text) + i.pos();
}

} // namespace de
