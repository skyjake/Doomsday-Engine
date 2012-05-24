/**\file p_iterlist.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_ITERLIST_H
#define LIBCOMMON_ITERLIST_H

#include "dd_api.h"

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
typedef struct iterlist_s {
    /// Direction of traversal.
    iterlist_iterator_direction_t _direction;

    /// Index of the current object being pointed at by the iterator.
    int _currentObject;

    /// Current maximum number of objects that can be pointed at in _objects.
    int _maxObjects;

    /// List of objects present.
    int _objectsCount;
    void** _objects;
} iterlist_t;

iterlist_t* IterList_ConstructDefault(void);

void IterList_Destruct(iterlist_t* list);

/**
 * Push a new pointer onto the top of the stack.
 * @param ptr  Pointer to be added.
 * @return  Index associated to the newly added object.
 */
int IterList_Push(iterlist_t* list, void* ptr);

void* IterList_Pop(iterlist_t* list);

void IterList_Empty(iterlist_t* list);

int IterList_Size(iterlist_t* list);

/// @return  Current pointer being pointed at.
void* IterList_MoveIterator(iterlist_t* list);

void IterList_RewindIterator(iterlist_t* list);

void IterList_SetIteratorDirection(iterlist_t* list, iterlist_iterator_direction_t direction);

#endif /* LIBCOMMON_ITERLIST_H */
