/** @file defs/dedarray.h  Definition struct (POD) array.
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

#ifndef LIBDOOMSDAY_DEFINITION_ARRAY_H
#define LIBDOOMSDAY_DEFINITION_ARRAY_H

#include "../libdoomsday.h"
#include <de/Vector>

struct ded_count_s
{
    int num;
    int max;
};

typedef struct ded_count_s ded_count_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @return  Pointer to the new block of memory.
 */
LIBDOOMSDAY_PUBLIC void *DED_NewEntries(void** ptr, ded_count_t* cnt, size_t elemSize, int count);

/**
 * @return  Pointer to the new block of memory.
 */
LIBDOOMSDAY_PUBLIC void *DED_NewEntry(void** ptr, ded_count_t* cnt, size_t elemSize);

LIBDOOMSDAY_PUBLIC void DED_DelEntry(int index, void** ptr, ded_count_t* cnt, size_t elemSize);

LIBDOOMSDAY_PUBLIC void DED_DelArray(void** ptr, ded_count_t* cnt);

LIBDOOMSDAY_PUBLIC void DED_ZCount(ded_count_t* c);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_DEFINITION_ARRAY_H
