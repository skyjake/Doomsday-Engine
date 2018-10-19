/** @file stack.h Stack of void* elements.
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

#ifndef DE_STACK_H
#define DE_STACK_H

#include <de/liblegacy.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacyData
/// @{

struct ddstack_s; // opaque

typedef struct ddstack_s ddstack_t;

DE_PUBLIC ddstack_t *Stack_New(void);

DE_PUBLIC void Stack_Delete(ddstack_t *stack);

DE_PUBLIC size_t Stack_Height(ddstack_t *stack);

DE_PUBLIC void Stack_Push(ddstack_t *stack, void *data);

DE_PUBLIC void *Stack_Pop(ddstack_t *stack);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_STACK_H
