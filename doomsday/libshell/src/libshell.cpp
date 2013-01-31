/** @file libshell.cpp  Common utility functions for libshell.
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

#include "de/shell/libshell.h"
#include <de/math.h>

namespace de {
namespace shell {

void LineWrapping::wrapTextToWidth(String const &text, int maxWidth)
{
    clear();

    int const lineWidth = maxWidth;
    int begin = 0;
    forever
    {
        int end = begin + lineWidth;
        if(end >= text.size())
        {
            // Time to stop.
            append(WrappedLine(begin, text.size()));
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
        if(text.at(end).isSpace()) ++end;
        append(WrappedLine(begin, end));
        begin = end;
    }

    // Mark the final line.
    last().isFinal = true;
}

int LineWrapping::width() const
{
    int w = 0;
    for(int i = 0; i < size(); ++i)
    {
        WrappedLine const &span = at(i);
        w = de::max(w, span.end - span.start);
    }
    return w;
}

int LineWrapping::height() const
{
    return size();
}

} // namespace shell
} // namespace de
