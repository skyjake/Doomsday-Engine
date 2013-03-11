/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gemclass.h"
#include "utils.h"

GemClass::GemClass(GemType t, int styleFlags, FlushMode mode,
                     const String& filterString)
{
    _type = t;
    _style = styleFlags;
    _flush = mode;
    _filter = filterString;
}

GemClass::GemClass(int styleFlags, FlushMode mode)
{
    _type = Gem;
    _style = styleFlags;
    _flush = mode;
}

int GemClass::modifyStyle(int setFlags, int clearFlags)
{
    _style |= setFlags;
    _style &= ~clearFlags;
    return _style;
}

bool GemClass::hasStyle(int flags) const
{
    return (_style & flags) == flags;
}

GemClass GemClass::operator + (const GemClass &other) const
{
    String combinedFilter = _filter;
    // Combine the two filters with the chaining operator.
    if(!other._filter.isEmpty())
    {
        if(combinedFilter.isEmpty())
            combinedFilter = other._filter;
        else
            combinedFilter += "@\\" + other._filter;
    }
    // Override inherited flush modes.
    FlushMode combinedFlush = other._flush;
    if(combinedFlush == FlushInherit)
    {
        combinedFlush = _flush;
    }
    return GemClass(_type, _style | other._style, combinedFlush, combinedFilter);
}

String GemClass::typeAsString() const
{
    switch(_type)
    {
    case None: return "None";
    case Gem: return "Gem";
    case Indent: return "Indent";
    case List: return "List";
    case DefinitionList: return "DefList";
    case Table: return "Table";
    case PartTitle: return "Part";
    case ChapterTitle: return "Chapter";
    case SectionTitle: return "Section";
    case SubSectionTitle: return "Subsec";
    case Sub2SectionTitle: return "Sub2sec";
    case Sub3SectionTitle: return "Sub3sec";
    case Sub4SectionTitle: return "Sub4sec";
    case Contents: return "Contents";
    }
    return "Unknown!";
}

String GemClass::styleAsString() const
{
    String out;
    for(int i = 0; i < 32; ++i)
    {
        int flag = (1 << i);
        if(_style & flag)
        {
            if(!out.isEmpty()) out += "|";
            out += nameForStyle(flag);
        }
    }
    if(out.isEmpty()) return out;
    return "(" + out + ")";
}
