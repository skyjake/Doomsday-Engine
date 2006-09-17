/* DE1: $Id: def_data.c 3285 2006-06-11 08:01:30Z skyjake $
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * Compiles for jDoom/jHeretic/jHexen/WolfTC
 */

#ifndef __COMMON_LINELIST_H__
#define __COMMON_LINELIST_H__

#include "dd_api.h"

typedef struct linelist_s {
    line_t    **list;
    int         max;
    int         count;
    int         rover; // used during iteration
} linelist_t;

linelist_t *P_CreateLineList(void);
void        P_DestroyLineList(linelist_t *list);

int         P_AddLineToLineList(linelist_t *list, line_t *ld);
line_t     *P_PopLineList(linelist_t *list);

line_t     *P_LineListIterator(linelist_t *list);
void        P_LineListResetIterator(linelist_t *list);

void        P_EmptyLineList(linelist_t *list);
int         P_LineListSize(linelist_t *list);
#endif
