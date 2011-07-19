/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_iterlist.c : Object lists.
 * The lists can be traversed through iteration but otherwise act like a
 * LIFO stack. Used for things like spechits, linespecials etc.
 */

#ifndef __COMMON_ITERLIST_H__
#define __COMMON_ITERLIST_H__

#include "dd_api.h"

typedef struct iterlist_s {
    void      **list;
    int         max;
    int         count;
    int         rover; // used during iteration
    boolean     forward; // if true iteration moves forward instead.
} iterlist_t;

iterlist_t *P_CreateIterList(void);
void        P_DestroyIterList(iterlist_t *list);

int         P_AddObjectToIterList(iterlist_t *list, void *obj);
void       *P_PopIterList(iterlist_t *list);

void       *P_IterListIterator(iterlist_t *list);
void        P_IterListResetIterator(iterlist_t *list, boolean forward);

void        P_EmptyIterList(iterlist_t *list);
int         P_IterListSize(iterlist_t *list);
#endif
