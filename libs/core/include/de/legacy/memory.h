/**
 * @file memory.h
 * Memory allocations.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_SYSTEM_MEMORY_H
#define DE_SYSTEM_MEMORY_H

#include <de/liblegacy.h>
#include <string.h> // memcpy

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacy
/// @{

DE_PUBLIC void *M_Malloc(size_t size);

DE_PUBLIC void *M_Calloc(size_t size);

/**
 * Changes the size of a previously allocated memory block, allocates new
 * memory, or frees previously allocated memory.
 *
 * - If @a ptr is NULL and @a size is zero, nothing happens and NULL is returned.
 * - If @a ptr is NULL and @a size is not zero, new memory is allocated
 *   and a pointer to it is returned.
 * - If @a ptr is not NULL and @a size is not zero, the memory pointed to
 *   by @a ptr is resized, and the allocated memory's (possible) new address
 *   is returned.
 * - If @a ptr is not NULL and @a size is zero, the memory pointed to by
 *   @a ptr is freed (using M_Free()) and NULL is returned.
 *
 * @param ptr   Previously allocated memory, or NULL.
 * @param size  New size for the allocated memory.
 *
 * @return Allocated memory, or NULL.
 */
DE_PUBLIC void *M_Realloc(void *ptr, size_t size);

DE_PUBLIC void *M_MemDup(const void *ptr, size_t size);

DE_PUBLIC void M_Free(void *ptr);

DE_PUBLIC char *M_StrDup(const char *str);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_SYSTEM_MEMORY_H
