/**\file p_iterlist.c
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

#include <assert.h>
#include <stdlib.h>

#include "doomsday.h"

#include "p_iterlist.h"

iterlist_t* IterList_ConstructDefault(void)
{
    iterlist_t* list = (iterlist_t*) malloc(sizeof(*list));
    if(NULL == list)
        Con_Error("IterList::ConstructDefault: Failed on allocation of %u bytes for "
            "new IterList.", (unsigned int) sizeof(*list));
    list->_objects = NULL;
    list->_objectsCount = list->_maxObjects = list->_currentObject = 0;
    list->_direction = ITERLIST_BACKWARD;
    return list;
}

void IterList_Destruct(iterlist_t* list)
{
    assert(NULL != list);
    if(NULL != list->_objects)
    {
        free(list->_objects);
        list->_objects = NULL;
    }
    free(list);
}

int IterList_Push(iterlist_t* list, void* obj)
{
    assert(NULL != list);
    if(++list->_objectsCount > list->_maxObjects)
    {
         list->_maxObjects = (list->_maxObjects? list->_maxObjects * 2 : 8);
         list->_objects = (void**) realloc(list->_objects, sizeof(*list->_objects) * list->_maxObjects);
         if(NULL == list->_objects)
             Con_Error("IterList::Push: Failed on (re)allocation of %u bytes for "
                "object list.", (unsigned int) sizeof(*list->_objects));
    }

    list->_objects[list->_objectsCount - 1] = obj;
    if(1 == list->_objectsCount)
    {
        if(ITERLIST_FORWARD == list->_direction)
            list->_currentObject = -1;
        else // Backward iteration.
            list->_currentObject = list->_objectsCount;
    }

    return list->_objectsCount - 1;
}

void* IterList_Pop(iterlist_t* list)
{
    assert(NULL != list);
    if(list->_objectsCount > 0)
        return list->_objects[--list->_objectsCount];
    return NULL;
}

void IterList_Empty(iterlist_t* list)
{
    assert(NULL != list);
    list->_objectsCount = list->_maxObjects = list->_currentObject = 0;
}

int IterList_Size(iterlist_t* list)
{
    assert(NULL != list);
    return list->_objectsCount;
}

void* IterList_MoveIterator(iterlist_t* list)
{
    assert(NULL != list);

    if(!list->_objectsCount)
        return NULL;

    if(ITERLIST_FORWARD == list->_direction)
    {
        if(list->_currentObject < list->_objectsCount - 1)
            return list->_objects[++list->_currentObject];
        return NULL;
    }
    // Backward iteration.
    if(list->_currentObject > 0)
        return list->_objects[--list->_currentObject];
    return NULL;
}

void IterList_RewindIterator(iterlist_t* list)
{
    assert(NULL != list);
    if(ITERLIST_FORWARD == list->_direction)
    {
        list->_currentObject = -1;
        return;
    }
    // Backward iteration.
    list->_currentObject = list->_objectsCount;
}

void IterList_SetIteratorDirection(iterlist_t* list, iterlist_iterator_direction_t direction)
{
    assert(NULL != list);

    list->_direction = direction;
    if(0 == list->_objectsCount)
        return;

    // Do we need to reposition?
    if(-1 == list->_currentObject)
    {
        list->_currentObject = list->_objectsCount;
        return;
    }
    if(list->_objectsCount == list->_currentObject)
        list->_currentObject = -1;
}
