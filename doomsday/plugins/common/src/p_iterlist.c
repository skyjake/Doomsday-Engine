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

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_iterlist.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Allocate and initialize a new iterlist.
 *
 * @return          Ptr to the new list.
 */
iterlist_t *P_CreateIterList(void)
{
    iterlist_t *list = malloc(sizeof(iterlist_t));

    list->list = NULL;
    list->count = list->max = list->rover = 0;
    list->forward = false;

    return list;
}

/**
 * Free any memory used by the iterlist.
 *
 * @param list      Ptr to the list to be destroyed.
 */
void P_DestroyIterList(iterlist_t *list)
{
    if(!list)
        return;

    if(list->count > 0)
    {
        free(list->list);
        list->list = NULL;
    }

    free(list);
    list = NULL;
}

/**
 * Add the given object to iterlist.
 *
 * @param list      Ptr to the list to add 'obj' too.
 * @param obj       Ptr to the object to be added to the list.
 * @return          The index of the object within 'list' once added,
 *                  ELSE @c -1.
 */
int P_AddObjectToIterList(iterlist_t *list, void *obj)
{
    if(!list || !obj)
        return -1;

    if(++list->count > list->max)
    {
         list->max = (list->max? list->max * 2 : 8);
         list->list = realloc(list->list, sizeof(void*) * list->max);
    }

    list->list[list->count - 1] = obj;

    return list->count - 1;
}

/**
 * Pop the top of the iterlist and return the next element.
 *
 * @param list      Ptr to the list to be pop.
 * @return          Ptr to the next object in 'list'.
 */
void* P_PopIterList(iterlist_t *list)
{
    if(!list)
        return NULL;

    if(list->count > 0)
        return list->list[--list->count];
    else
        return NULL;
}

/**
 * Returns the next element in the iterlist.
 *
 * @param list      Ptr to the list to iterate.
 * @return          The next object in the iterlist.
 */
void* P_IterListIterator(iterlist_t *list)
{
    if(!list || !list->count)
        return NULL;

    if(list->forward)
    {
        if(list->rover < list->count - 1)
            return list->list[++list->rover];
        else
            return NULL;
    }
    else
    {
        if(list->rover > 0)
            return list->list[--list->rover];
        else
            return NULL;
    }
}

/**
 * Returns the iterlist iterator to the beginning (the end).
 *
 * @param list      Ptr to the list whoose iterator to reset.
 * @param forward   @c true = iteration will move forwards.
 */
void P_IterListResetIterator(iterlist_t *list, boolean forward)
{
    if(!list)
        return;

    list->forward = forward;
    if(list->forward)
        list->rover = -1;
    else
        list->rover = list->count;
}

/**
 * Empty the iterlist.
 *
 * @param list      Ptr to the list to empty.
 */
void P_EmptyIterList(iterlist_t *list)
{
    if(!list)
        return;

    list->count = list->max = list->rover = 0;
}

/**
 * Return the size of the iterlist.
 *
 * @param list      Ptr to the list to return the size of.
 * @return          The size of the iterlist.
 */
int P_IterListSize(iterlist_t *list)
{
    if(!list)
        return 0;

    return list->count;
}
