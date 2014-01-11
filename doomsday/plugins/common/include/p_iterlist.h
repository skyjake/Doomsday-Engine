/** @file p_iterlist.h
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_ITERLIST_H
#define LIBCOMMON_ITERLIST_H

typedef enum {
    ITERLIST_BACKWARD = 0, /// Top to bottom.
    ITERLIST_FORWARD       /// Bottom to top.
} iterlist_iterator_direction_t;

/**
 * IterList. A LIFO stack of pointers with facilities for bidirectional
 * iteration through the use of an integral iterator (thus scopeless).
 *
 * @warning Not thread safe!
 */
struct iterlist_s;
typedef struct iterlist_s iterlist_t;

#ifdef __cplusplus
extern "C" {
#endif

iterlist_t *IterList_New(void);

void IterList_Delete(iterlist_t *list);

/**
 * Push a new pointer onto the top of the stack.
 * @param data  User data pointer to be added.
 * @return  Index associated to the newly added object.
 */
int IterList_PushBack(iterlist_t *list, void *data);

void* IterList_Pop(iterlist_t *list);

void IterList_Clear(iterlist_t *list);

int IterList_Size(iterlist_t *list);

dd_bool IterList_Empty(iterlist_t *list);

/// @return  Current pointer being pointed at.
void* IterList_MoveIterator(iterlist_t *list);

void IterList_RewindIterator(iterlist_t *list);

void IterList_SetIteratorDirection(iterlist_t *list, iterlist_iterator_direction_t direction);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_ITERLIST_H */
