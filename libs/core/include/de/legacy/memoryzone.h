/**
 * @file memoryzone.h
 * Memory zone.
 *
 * @par Build Options
 * Define the macro @c DE_FAKE_MEMORY_ZONE to force all memory blocks to be
 * allocated from the real heap. Useful when debugging memory-related problems.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef DE_MEMORY_ZONE_H
#define DE_MEMORY_ZONE_H

/**
 * @defgroup memzone Memory Zone
 * @ingroup legacy
 */

#include <de/liblegacy.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup purgeLevels Purge Levels
 * @ingroup memzone
 */
///@{
#define PU_APPSTATIC         1 ///< Static entire execution time.

#define PU_GAMESTATIC       40 ///< Static until the game plugin which allocated it is unloaded.
#define PU_MAP              50 ///< Static until map exited (may still be freed during the map, though).
#define PU_MAPSTATIC        52 ///< Not freed until map exited.

#define PU_PURGELEVEL       100 ///< Tags >= 100 are purgable whenever needed.
///@}

#define DE_ZONEID      0x1d4a11

/// @addtogroup memzone
///@{

/**
 * Determines if the memory zone is available for allocations.
 */
DE_PUBLIC dd_bool Z_IsInited(void);

/**
 * You can pass a NULL user if the tag is < PU_PURGELEVEL.
 */
DE_PUBLIC void *Z_Malloc(size_t size, int tag, void *ptr);

/**
 * Memory allocation utility: malloc and clear.
 */
DE_PUBLIC void *Z_Calloc(size_t size, int tag, void *user);

/**
 * Only resizes blocks with no user. If a block with a user is
 * reallocated, the user will lose its current block and be set to
 * NULL. Does not change the tag of existing blocks.
 */
DE_PUBLIC void *Z_Realloc(void *ptr, size_t n, int mallocTag);

/**
 * Realloc and set possible new memory to zero.
 */
DE_PUBLIC void *Z_Recalloc(void *ptr, size_t n, int callocTag);

/**
 * Free memory that was allocated with Z_Malloc.
 */
DE_PUBLIC void Z_Free(void *ptr);

/**
 * Free memory blocks in all volumes with a tag in the specified range.
 */
DE_PUBLIC void Z_FreeTags(int lowTag, int highTag);

/**
 * Check all zone volumes for consistency.
 */
DE_PUBLIC void Z_CheckHeap(void);

/**
 * Change the tag of a memory block.
 */
DE_PUBLIC void Z_ChangeTag2(void *ptr, int tag);

/**
 * Change the user of a memory block.
 */
DE_PUBLIC void Z_ChangeUser(void *ptr, void *newUser);

DE_PUBLIC uint32_t Z_GetId(void *ptr);

/**
 * Get the user of a memory block.
 */
DE_PUBLIC void *Z_GetUser(void *ptr);

DE_PUBLIC int Z_GetTag(void *ptr);

/**
 * Checks if @a ptr points to memory inside the memory zone.
 * @param ptr  Pointer.
 * @return @c true, if @a ptr points to a valid allocated memory block
 * inside the zone.
 */
DE_PUBLIC dd_bool Z_Contains(void *ptr);

/**
 * Copies @a text into a buffer allocated from the zone.
 * Similar to strdup().
 *
 * @param text  Null-terminated C string.
 *
 * @return  Copy of the string (in the zone).
 */
DE_PUBLIC char *Z_StrDup(const char *text);

DE_PUBLIC void *Z_MemDup(const void *ptr, size_t size);

struct zblockset_s;
typedef struct zblockset_s zblockset_t;

/**
 * Creates a new block memory allocator in the Zone.
 *
 * @param sizeOfElement  Required size of each element.
 * @param batchSize  Number of elements in each block of the set.
 * @param tag  Purge level for the allocation.
 *
 * @return  Ptr to the newly created blockset.
 */
DE_PUBLIC zblockset_t *ZBlockSet_New(size_t sizeOfElement, uint32_t batchSize, int tag);

/**
 * Destroy the entire blockset.
 * All memory allocated is released for all elements in all blocks and any
 * used for the blockset itself.
 *
 * @param set  The blockset to be freed.
 */
DE_PUBLIC void ZBlockSet_Delete(zblockset_t *set);

/**
 * Return a ptr to the next unused element in the blockset.
 *
 * @param set  The blockset to return the next element from.
 *
 * @return  Ptr to the next unused element in the blockset.
 */
DE_PUBLIC void *ZBlockSet_Allocate(zblockset_t *set);

#define Z_ChangeTag(p,t) { \
    if (Z_GetId(p) != DE_ZONEID) \
        Con_Error("Z_ChangeTag at " __FILE__ ":%i", __LINE__); \
    Z_ChangeTag2(p, t); }

DE_PUBLIC void Z_PrintStatus(void);

/**
 * Puts a region of memory allocated with Z_Malloc() or malloc() up for garbage
 * collection.
 *
 * @param ptr  Allocated memory (not previously trashed).
 */
DE_PUBLIC void Garbage_Trash(void *ptr);

///@}

#ifdef DE_DEBUG
struct memzone_private_s;
typedef struct memzone_private_s MemoryZonePrivateData;
DE_PUBLIC void Z_GetPrivateData(MemoryZonePrivateData *pd);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_MEMORY_ZONE_H
