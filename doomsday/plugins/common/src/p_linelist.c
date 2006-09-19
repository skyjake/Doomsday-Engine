/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright Â© 2006 Daniel Swanson <danij@dengine.net>
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
 * p_linelist.c : Line lists.
 * The lists can be traversed through iteration but otherwise act like a
 * LIFO stack. Used for things like spechits, linespecials etc.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_linelist.h"

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
 * Allocate and initialize a new linelist.
 *
 * @return      Ptr to the new list.
 */
linelist_t *P_CreateLineList(void)
{
    linelist_t *list = malloc(sizeof(linelist_t));

    list->list = NULL;
    list->count = list->max = list->rover = 0;

    return list;
}

/**
 * Free any memory used by the linelist.
 * @param   list    Ptr to the list to be destroyed.
 */
void P_DestroyLineList(linelist_t *list)
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
 * Add the given line to linelist.
 *
 * @param   list    Ptr to the list to add <code>ld</code> too.
 * @param   ld      The line_t to be added to the list.
 * @return          The index of the line within <codelist</code> once
 *                  added, ELSE <code>-1</code>.
 */
int P_AddLineToLineList(linelist_t *list, line_t *ld)
{
    if(!list || !ld)
        return -1;

    if(++list->count > list->max)
    {
         list->max = (list->max? list->max * 2 : 8);
         list->list = realloc(list->list, sizeof(line_t*) * list->max);
    }

    list->list[list->count - 1] = ld;

    return list->count - 1;
}

/**
 * Pop the top of the linelist and return the next element.
 *
 * @param   list    Ptr to the list to be pop.
 * @return          Ptr to the next line in <code>list</code>.
 */
line_t* P_PopLineList(linelist_t *list)
{
    if(!list)
        return NULL;

    if(list->count > 0)
        return list->list[--list->count];
    else
        return NULL;
}

/**
 * Returns the next element in the linelist.
 *
 * @param   list    Ptr to the list to iterate.
 * @return          The next line_t in the linelist.
 */
line_t* P_LineListIterator(linelist_t *list)
{
    if(!list)
        return NULL;

    if(list->count > 0 && list->rover > 0)
        return list->list[--list->rover];
    else
        return NULL;
}

/**
 * Returns the linelist iterator to the beginning (the end).
 *
 * @param   list    Ptr to the list whoose iterator to reset.
 */
void P_LineListResetIterator(linelist_t *list)
{
    if(!list)
        return;

    list->rover = list->count;
}

/**
 * Empty the linelist.
 *
 * @param   list    Ptr to the list to empty.
 */
void P_EmptyLineList(linelist_t *list)
{
    if(!list)
        return;

    list->count = list->max = list->rover = 0;
}

/**
 * Return the size of the linelist.
 *
 * @param   list    Ptr to the list to return the size of.
 * @return          The size of the linelist.
 */
int P_LineListSize(linelist_t *list)
{
    if(!list)
        return 0;

    return list->count;
}
