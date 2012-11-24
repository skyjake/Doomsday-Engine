/**
 * @file memory.h
 * Memory allocations. @ingroup system
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SYSTEM_MEMORY_H
#define LIBDENG_SYSTEM_MEMORY_H

#include <de/libdeng.h>
#include <string.h> // memcpy

#ifdef __cplusplus
extern "C" {
#endif

DENG_PUBLIC void *M_Malloc(size_t size);

DENG_PUBLIC void *M_Calloc(size_t size);

DENG_PUBLIC void *M_Realloc(void *ptr, size_t size);

DENG_PUBLIC void *M_MemDup(void const *ptr, size_t size);

DENG_PUBLIC void M_Free(void *ptr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_SYSTEM_MEMORY_H
