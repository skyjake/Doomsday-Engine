/**
 * @file garbage.h
 * Garbage collector. @ingroup base
 *
 * Stores pointers to unnecessary areas of memory and frees them later. Garbage
 * collection must be requested manually, e.g., at the end of the frame once
 * per second. Garbage is also thread-specific; recycling must be done
 * separately in each thread.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_GARBAGE_COLLECTOR_H
#define LIBDENG_GARBAGE_COLLECTOR_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void Garbage_Shutdown(void);

void Garbage_ClearForThread(void);

/**
 * Puts a region of allocated memory up for garbage collection. The memory
 * will be available for use until Garbage_Recycle() is called.
 *
 * @param ptr  Pointer to memory. Can be allocated with malloc() or Z_Malloc().
 */
void Garbage_Trash(void* ptr);

/**
 * Determines whether a memory pointer has been trashed.
 *
 * @param ptr  Pointer to memory.
 *
 * @return @c true if the pointer is in the trash.
 */
boolean Garbage_IsTrashed(const void* ptr);

/**
 * Removes a region from the trash, if it is still there.
 * @warning Do not call this if there is a chance that the pointer has
 * already been freed.
 *
 * @param ptr  Pointer to memory allocated from the @ref memzone.
 */
void Garbage_Untrash(void* ptr);

/**
 * Frees all pointers given over to the garbage collector.
 */
void Garbage_Recycle(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_GARBAGE_COLLECTOR_H
