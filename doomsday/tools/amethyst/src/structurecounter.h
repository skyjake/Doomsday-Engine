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

#ifndef __AMETHYST_STRUCT_COUNTER_H__
#define __AMETHYST_STRUCT_COUNTER_H__

#include "string.h"

enum 
{
    CNT_NONE = -1,
    CNT_PART = 0,
    CNT_CHAPTER,
    CNT_SECTION,
    CNT_SUBSEC,
    CNT_SUBSEC2,
    CNT_SUBSEC3,
    CNT_SUBSEC4,
    NUM_COUNTS
};

class StructureCounter
{
public:
    StructureCounter();

    int indexForName(const String& strucName) const;
    int counter(int index) const;
    void increment(int index);
    void reset(int index = CNT_PART);
    String text(int index) const;

private:
    int _counts[NUM_COUNTS];
};

#endif
