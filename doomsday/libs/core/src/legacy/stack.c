/** @file stack.c Stack of void* elements.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include "de/liblegacy.h"
#include "de/legacy/memory.h"
#include "de/legacy/stack.h"
#include <de/c_wrapper.h>

struct ddstack_s {
    size_t height; // Height of the stack.
    void **data;
};

ddstack_t *Stack_New(void)
{
    ddstack_t *stack = M_Malloc(sizeof(ddstack_t));

    stack->height = 0;
    stack->data = NULL;

    return stack;
}

void Stack_Delete(ddstack_t *s)
{
    if (!s) return;

    // Clear the stack first.
    while (Stack_Height(s) > 0)
    {
        Stack_Pop(s);
    }

    if (s->data) M_Free(s->data);
    M_Free(s);
}

size_t Stack_Height(ddstack_t *s)
{
    if (!s) return 0;
    return s->height;
}

void Stack_Push(ddstack_t *s, void *data)
{
    if (!s) return;

    s->data = M_Realloc(s->data, sizeof(void *) * ++s->height);
    s->data[s->height-1] = data;
}

void *Stack_Pop(ddstack_t *s)
{
    void *retVal;

    if (!s) return NULL;

    DE_ASSERT(s->height > 0);

    if (!s->height)
    {
        App_Log(DE2_LOG_DEBUG, "Stack::Pop: Underflow.");
        return NULL;
    }

    retVal = s->data[--s->height];
    s->data[s->height] = NULL;

    return retVal;
}
