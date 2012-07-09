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

#include <de/libdeng.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Puts a region of allocated memory up for garbage collection in the current
 * thread. The memory will be available for use until Garbage_Recycle() is
 * called.
 *
 * @param ptr  Pointer to memory. Can be allocated with malloc() or Z_Malloc().
 */
DENG_PUBLIC void Garbage_Trash(void* ptr);

/**
 * Pointer to an instance destructor;
 */
typedef void (*GarbageDestructor)(void*);

/**
 * Puts an object up for garbage collection in the current thread. The object
 * will be available for use until Garbage_Recycle() is called.
 *
 * @param ptr  Pointer to the object.
 * @param destructor  Function that destroys the object.
 */
DENG_PUBLIC void Garbage_TrashInstance(void* ptr, GarbageDestructor destructor);

/**
 * Determines whether a memory pointer has been trashed. Only the current
 * thread's collector is searched.
 *
 * @param ptr  Pointer to memory.
 *
 * @return @c true if the pointer is in the trash.
 */
DENG_PUBLIC boolean Garbage_IsTrashed(const void* ptr);

/**
 * Removes a region from the current thread's collector, if it is still there.
 * @warning Do not call this if there is a chance that the pointer has already
 * been freed.
 *
 * @param ptr  Pointer to memory allocated from the @ref memzone.
 */
DENG_PUBLIC void Garbage_Untrash(void* ptr);

/**
 * Removes a pointer from the garbage. This needs to be called if the
 * previously trashed memory was manually freed.
 *
 * @param ptr  Pointer to memory.
 */
DENG_PUBLIC void Garbage_RemoveIfTrashed(void* ptr);

/**
 * Frees all pointers given over to the current thread's garbage collector.
 * Every thread that uses garbage collection must call this periodically
 * or the trashed memory will not be freed.
 */
DENG_PUBLIC void Garbage_Recycle(void);

/**
 * Recycles all garbage of the current thread and deletes the thread's garbage
 * collector. Should be called right before the thread ends.
 *
 * This differs from Garbage_Recycle() in that all the thread-specific internal
 * resources are freed, not just the thread's trashed memory. The thread can
 * safely end after this.
 */
DENG_PUBLIC void Garbage_ClearForThread(void);

void Garbage_Init(void);

/**
 * Recycles all collected garbage and deletes the collectors. Called at engine
 * shutdown. Should be called in the main thread.
 */
void Garbage_Shutdown(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_GARBAGE_COLLECTOR_H
