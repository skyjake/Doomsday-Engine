/** @file dedarray.cpp  Definition struct (POD) array.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/dedarray.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <de/memory.h>
#include <de/strutil.h>

void* DED_NewEntries(void** ptr, ded_count_t* cnt, size_t elemSize, int count)
{
    void* np;

    cnt->num += count;
    if(cnt->num > cnt->max)
    {
        cnt->max *= 2; // Double the size of the array.
        if(cnt->num > cnt->max)
            cnt->max = cnt->num;
        *ptr = M_Realloc(*ptr, elemSize * cnt->max);
    }

    np = (char*) *ptr + (cnt->num - count) * elemSize;
    memset(np, 0, elemSize * count); // Clear the new entries.
    return np;
}

void* DED_NewEntry(void** ptr, ded_count_t* cnt, size_t elemSize)
{
    return DED_NewEntries(ptr, cnt, elemSize, 1);
}

void DED_DelEntry(int index, void** ptr, ded_count_t* cnt, size_t elemSize)
{
    if(index < 0 || index >= cnt->num) return;

    memmove((char*) *ptr + elemSize * index,
            (char*) *ptr + elemSize * (index + 1),
            elemSize * (cnt->num - index - 1));

    if(--cnt->num < cnt->max / 2)
    {
        cnt->max /= 2;
        *ptr = M_Realloc(*ptr, elemSize * cnt->max);
    }
}

void DED_DelArray(void** ptr, ded_count_t* cnt)
{
    if(*ptr) M_Free(*ptr);
    *ptr = 0;

    cnt->num = cnt->max = 0;
}

void DED_ZCount(ded_count_t* c)
{
    c->num = c->max = 0;
}
