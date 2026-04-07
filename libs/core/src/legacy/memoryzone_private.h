/**
 * @file memoryzone_private.h
 * Memory zone (internal header).
 *
 * This header file is separate from the public one because it defines the data
 * structures used internally by the memory zone module. The private data
 * should not be accessed under normal circumstances. In a debug build, the
 * memzone_private_s struct can be used to examine the contents of the zone for
 * debugging purposes -- however, doing so requires directly including this
 * internal header file.
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

#ifndef DE_MEMORY_ZONE_PRIVATE_H
#define DE_MEMORY_ZONE_PRIVATE_H

#include <de/liblegacy.h>
#include <de/legacy/memoryzone.h> // public API

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the memory zone.
 */
int Z_Init(void);

/**
 * Shut down the memory zone by destroying all the volumes.
 */
void Z_Shutdown(void);

size_t Z_FreeMemory(void);

typedef struct memblock_s {
    size_t          size; // Including header and possibly tiny fragments.
    void **         user; // NULL if a free block.
    int             tag; // Purge level.
    int             id; // Should be DE_ZONEID.
    struct memvolume_s *volume; // Volume this block belongs to.
    struct memblock_s *next, *prev;
    struct memblock_s *seqLast, *seqFirst;
#ifdef DE_FAKE_MEMORY_ZONE
    void *          area; // The real memory area.
    size_t          areaSize; // Size of the allocated memory area.
#endif
} memblock_t;

typedef struct {
    size_t          size; // Total bytes malloced, including header.
    memblock_t      blockList; // Start / end cap for linked list.
    memblock_t *    rover;
    memblock_t *    staticRover;
} memzone_t;

/**
 * The memory is composed of multiple volumes. New volumes are allocated when
 * necessary.
 */
typedef struct memvolume_s {
    memzone_t *zone;
    size_t size;
    size_t allocatedBytes;  ///< Total number of allocated bytes.
    struct memvolume_s *next;
} memvolume_t;

struct zblockset_block_s;

/**
 * ZBlockSet. Block memory allocator.
 *
 * These are used instead of many calls to Z_Malloc when the number of
 * required elements is unknown and when linear allocation would be too
 * slow.
 *
 * Memory is allocated as needed in blocks of "batchSize" elements. When
 * a new element is required we simply reserve a ptr in the previously
 * allocated block of elements or create a new block just in time.
 *
 * The internal state of a blockset is managed automatically.
 */
struct zblockset_s {
    unsigned int _elementsPerBlock;
    size_t _elementSize;
    int _tag; /// All blocks in a blockset have the same tag.
    unsigned int _blockCount;
    struct zblockset_block_s *_blocks;
};

#ifdef DE_FAKE_MEMORY_ZONE
memblock_t *Z_GetBlock(void *ptr);
#else
// Real memory zone allocates memory from a custom heap.
#define Z_GetBlock(ptr) ((memblock_t *) ((byte *)(ptr) - sizeof(memblock_t)))
#endif

#ifdef DE_DEBUG
/**
 * Provides access to the internal data of the memory zone module.
 * This is only needed for debugging purposes.
 */
struct memzone_private_s {
    void (*lock)(void);
    void (*unlock)(void);
    dd_bool (*isVolumeTooFull)(memvolume_t *);
    int volumeCount;
    memvolume_t *volumeRoot;
};
#endif // DE_DEBUG

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_MEMORY_ZONE_PRIVATE_H */
