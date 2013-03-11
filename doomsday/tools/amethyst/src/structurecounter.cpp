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

#include "structurecounter.h"

StructureCounter::StructureCounter()
{
    reset();
}

int StructureCounter::indexForName(const String& strucName) const
{
    const char *names[] = { "part", "chapter", "section", "subsec",
        "sub2sec", "sub3sec", "sub4sec", 0 };
    for(int i = 0; names[i]; i++)
        if(strucName == names[i])
            return i;
    return CNT_NONE;
}

int StructureCounter::counter(int index) const
{
    if(index < 0 || index >= NUM_COUNTS) return 0;
    return _counts[index];
}

void StructureCounter::increment(int index)
{
    if(index < 0 || index >= NUM_COUNTS) return;
    _counts[index]++;
    reset(index + 1);
}

void StructureCounter::reset(int index)
{
    if(index < 0 || index >= NUM_COUNTS) return;
    for(int i = index; i < NUM_COUNTS; i++)
        _counts[i] = 0;
}

String StructureCounter::text(int index) const
{
    if(index == CNT_PART)
    {
        return QString::number(_counts[index]);
    }

    String out;
    for(int i = CNT_CHAPTER; i <= index && i < NUM_COUNTS; i++)
    {
        out += QString::number(_counts[i]);
        if(i < index) out += ".";
    }
    return out;
}
