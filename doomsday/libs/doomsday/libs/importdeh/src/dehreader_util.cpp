/** @file dehreader_util.cpp  Miscellaneous utility routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "dehreader_util.h"
#include <de/block.h>

using namespace de;

res::Uri composeMapUri(int episode, int map)
{
    if(episode > 0) // ExMy format.
    {
        return res::Uri("Maps", Stringf("E%dM%d", episode, map));
    }
    else // MAPxx format.
    {
        return res::Uri("Maps", Stringf("MAP%02d", map % 100));
    }
}

int valueDefForPath(const String &id, ded_value_t **def)
{
    if(!id.isEmpty())
    {
        for(int i = ded->values.size() - 1; i >= 0; i--)
        {
            ded_value_t &value = ded->values[i];
            if(!iCmpStrCase(value.id, id))
            {
                if(def) *def = &value;
                return i;
            }
        }
    }
    return -1; // Not found.
}

StringList splitMax(const String &str, Char sep, int max)
{
    if (max < 0)
    {
        return str.split(sep);
    }
    else if (max == 0)
    {
        return {};
    }
    else if (max == 1)
    {
        return {str};
    }

    String     buf = str;
    StringList tokens;

    BytePos pos{0}, substrEnd{0};
    while (pos < max - 1 && bool(substrEnd = buf.indexOf(sep)))
    {
        tokens << buf.substr(BytePos(0), substrEnd);

        // Find the start of the next token.
        while (substrEnd < buf.size() && buf.at(substrEnd) == sep)
        {
            ++substrEnd;
        }

        buf.remove(BytePos(0), substrEnd);

        pos++; // On to the next substring.
    }

    // Anything remaining goes into the last token (the rest of the line).
    if (pos < max)
    {
        tokens << buf;
    }

    //qDebug() << "splitMax: " << tokens.join("|");

    return tokens;
}
