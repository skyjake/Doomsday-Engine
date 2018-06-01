/**
 * @file p_iterlist.c
 *
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>
#include <de/liblegacy.h>

#include "doomsday.h"
#include "common.h"

#include "p_iterlist.h"

struct iterlist_s
{
    /// Direction of traversal.
    iterlist_iterator_direction_t direction;

    /// Index of the current element being pointed at by the iterator.
    int iter;

    /// Current maximum number of elements that can be pointed at in elements.
    int maxElements;

    /// List of elements present.
    int elementsCount;
    void** elements;
};

iterlist_t* IterList_New(void)
{
    iterlist_t* list = (iterlist_t*) malloc(sizeof(*list));
    if (!list) Libdeng_BadAlloc();

    list->elements = NULL;
    list->elementsCount = list->maxElements = list->iter = 0;
    list->direction = ITERLIST_BACKWARD;
    return list;
}

void IterList_Delete(iterlist_t* list)
{
    if(!list) return;
    if(list->elements)
    {
        free(list->elements); list->elements = NULL;
    }
    free(list);
}

int IterList_Size(iterlist_t* list)
{
    DE_ASSERT(list);
    return list->elementsCount;
}

dd_bool IterList_Empty(iterlist_t* list)
{
    return IterList_Size(list) == 0;
}

int IterList_PushBack(iterlist_t* list, void* data)
{
    DE_ASSERT(list);

    if(++list->elementsCount > list->maxElements)
    {
        list->maxElements = (list->maxElements? list->maxElements * 2 : 8);
        list->elements = (void**) realloc(list->elements, sizeof(*list->elements) * list->maxElements);
        if (!list->elements) Libdeng_BadAlloc();
    }

    list->elements[list->elementsCount - 1] = data;
    if(1 == list->elementsCount)
    {
        if(ITERLIST_FORWARD == list->direction)
            list->iter = -1;
        else // Backward iteration.
            list->iter = list->elementsCount;
    }

    return list->elementsCount - 1;
}

void* IterList_Pop(iterlist_t* list)
{
    DE_ASSERT(list);
    if(list->elementsCount > 0)
        return list->elements[--list->elementsCount];
    return NULL;
}

void IterList_Clear(iterlist_t* list)
{
    DE_ASSERT(list);
    list->elementsCount = list->maxElements = list->iter = 0;
}

void* IterList_MoveIterator(iterlist_t* list)
{
    DE_ASSERT(list);

    if(!list->elementsCount)
        return NULL;

    if(ITERLIST_FORWARD == list->direction)
    {
        if(list->iter < list->elementsCount - 1)
            return list->elements[++list->iter];
        return NULL;
    }
    // Backward iteration.
    if(list->iter > 0)
        return list->elements[--list->iter];
    return NULL;
}

void IterList_RewindIterator(iterlist_t* list)
{
    DE_ASSERT(list);
    if(ITERLIST_FORWARD == list->direction)
    {
        list->iter = -1;
        return;
    }
    // Backward iteration.
    list->iter = list->elementsCount;
}

void IterList_SetIteratorDirection(iterlist_t* list, iterlist_iterator_direction_t direction)
{
    DE_ASSERT(list);

    list->direction = direction;
    if(0 == list->elementsCount)
        return;

    // Do we need to reposition?
    if(-1 == list->iter)
    {
        list->iter = list->elementsCount;
        return;
    }
    if(list->elementsCount == list->iter)
        list->iter = -1;
}
