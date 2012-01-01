/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * A standard, basic stack.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#if _DEBUG
# include "de_console.h"
#endif

#include "m_stack.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    size_t              height; // Height of the stack.
    void**              data;
} stackdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

ddstack_t* Stack_New(void)
{
    stackdata_t*        stack = malloc(sizeof(stackdata_t));

    stack->height = 0;
    stack->data = NULL;

    return (ddstack_t*) stack;
}

void Stack_Delete(ddstack_t* s)
{
    stackdata_t*        stack;

    if(!s)
        return;
    stack = (stackdata_t*) s;

    if(stack->height)
    {
#if _DEBUG
Con_Message("Stack::Delete: Stack not empty!\n");
#endif
    }

    if(stack->data)
        free(stack->data);
    free(stack);
}

size_t Stack_Height(ddstack_t* s)
{
    if(!s)
        return 0;

    return ((stackdata_t*) s)->height;
}

void Stack_Push(ddstack_t* s, void* data)
{
    stackdata_t*        stack;

    if(!s)
        return;
    stack = (stackdata_t*) s;

    stack->data = realloc(stack->data, sizeof(void*) * ++stack->height);
    stack->data[stack->height-1] = data;
}

void* Stack_Pop(ddstack_t* s)
{
    stackdata_t*        stack;
    void*               retVal;

    if(!s)
        return NULL;
    stack = (stackdata_t*) s;

    if(!(stack->height > 0))
    {
#if _DEBUG
Con_Message("Stack::Pop: Underflow.\n");
#endif
        return NULL;
    }

    retVal = stack->data[--stack->height];
    stack->data[stack->height] = NULL;

    return retVal;
}
