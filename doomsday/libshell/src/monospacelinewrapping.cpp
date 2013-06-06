/** @file monospacelinewrapping.cpp
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

#include "de/shell/MonospaceLineWrapping"
#include <de/math.h>

namespace de {
namespace shell {

MonospaceLineWrapping::MonospaceLineWrapping()
{
    //_lines.append(WrappedLine(Range(), true));
}

bool MonospaceLineWrapping::isEmpty() const
{
    return _lines.isEmpty();
}

void MonospaceLineWrapping::clear()
{
    _lines.clear();
}

void MonospaceLineWrapping::wrapTextToWidth(String const &text, int maxWidth)
{
    QChar const newline('\n');

    clear();

    if(maxWidth < 1) return; // No room to wrap.

    int const lineWidth = maxWidth;
    int begin = 0;
    forever
    {
        // Newlines always cause a wrap.
        int end = begin;
        while(end < begin + lineWidth &&
              end < text.size() && text.at(end) != newline) ++end;

        if(end == text.size())
        {
            // Time to stop.
            _lines.append(WrappedLine(Rangei(begin, text.size())));
            break;
        }

        // Find a good break point.
        while(!text.at(end).isSpace())
        {
            --end;
            if(end == begin)
            {
                // Ran out of non-space chars, force a break.
                end = begin + lineWidth;
                break;
            }
        }

        if(text.at(end) == newline)
        {
            // The newline will be omitted from the wrapped lines.
            _lines.append(WrappedLine(Rangei(begin, end)));
            begin = end + 1;
        }
        else
        {
            if(text.at(end).isSpace()) ++end;
            _lines.append(WrappedLine(Rangei(begin, end)));
            begin = end;
        }
    }

    // Mark the final line.
    _lines.last().isFinal = true;
}

int MonospaceLineWrapping::width() const
{
    int w = 0;
    for(int i = 0; i < _lines.size(); ++i)
    {
        WrappedLine const &span = _lines[i];
        w = de::max(w, span.range.size());
    }
    return w;
}

int MonospaceLineWrapping::height() const
{
    return _lines.size();
}

int MonospaceLineWrapping::rangeWidth(Rangei const &range) const
{
    return range.size();
}

int MonospaceLineWrapping::indexAtWidth(Rangei const &range, int width) const
{
    if(width <= range.size())
    {
        return range.start + width;
    }
    return range.end;
}

} // namespace shell
} // namespace de
