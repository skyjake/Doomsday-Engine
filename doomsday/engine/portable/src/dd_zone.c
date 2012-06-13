/**
 * @file dd_zone.c
 * Memory zone implementation. @ingroup memzone
 *
 * The zone is composed of multiple memory volumes. New volumes get created on
 * the fly when needed. This guarantees that all allocation requests will
 * succeed.
 *
 * There is never any space between memblocks, and there will never be two
 * contiguous free memblocks.
 *
 * Each volume employs two rovers that are used to locate free blocks for new
 * allocations. When an allocation succeeds, the rover is left pointing to the
 * block after the new allocation. The rover can be left pointing at a
 * non-empty block. One of the rovers is for the STATIC purgelevels while the
 * other is for all other purgelevels. The purpose of this is to prevent memory
 * fragmentation: when longer-lifespan allocations get mixed with
 * short-lifespan ones, the end result is more longer-lifespan allocations with
 * small amounts of potentially unusable free space between them. The static
 * rover attempts to place all the longer-lifespan allocation near the start of
 * the volume.
 *
 * You should not explicitly call Z_Free() on PU_CACHE blocks because they
 * will get automatically freed if necessary.
 *
 * @par Block Sequences
 * The PU_MAPSTATIC purge tag has a special purpose. It works like PU_MAP so
 * that it is purged on a per map basis, but blocks allocated as PU_MAPSTATIC
 * should not be freed at any time when the map is being used. Internally, the
 * map-static blocks are linked into sequences so that Z_Malloc knows to skip
 * all of them efficiently. This is possible because no block inside the
 * sequence could be purged by Z_Malloc() anyway.
 *
 * @authors Copyright © 1999-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <stdlib.h>
#include <assert.h> // Define NDEBUG in release builds.

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#ifdef _DEBUG
#  include "de_graphics.h"
#  include "de_render.h"
#endif

// Size of one memory zone volume.
#define MEMORY_VOLUME_SIZE  0x2000000   // 32 Mb

#define ZONEID  0x1d4a11

#define MINFRAGMENT (sizeof(memblock_t)+32)

#define ALIGNED(x) (((x) + sizeof(void*) - 1)&(~(sizeof(void*) - 1)))

/// Special user pointer for blocks that are in use but have no single owner.
#define MEMBLOCK_USER_ANONYMOUS    ((void*) 2)

/**
 * The memory is composed of multiple volumes.  New volumes are
 * allocated when necessary.
 */
typedef struct memvolume_s {
    memzone_t  *zone;
    size_t size;
    size_t allocatedBytes;  ///< Total number of allocated bytes.
    struct memvolume_s *next;
} memvolume_t;

// Used for block allocation of memory from the zone.
typedef struct zblockset_block_s {
    /// Maximum number of elements.
    unsigned int max;

    /// Number of used elements.
    unsigned int count;

    /// Size of a single element.
    size_t elementSize;

    /// Block of memory where elements are.
    void* elements;
} zblockset_block_t;

static memvolume_t *volumeRoot;
static memvolume_t *volumeLast;

static mutex_t zoneMutex = 0;

static size_t Z_AllocatedMemory(void);
static size_t allocatedMemoryInVolume(memvolume_t* volume);

static __inline void lockZone(void)
{
    assert(zoneMutex != 0);
    Sys_Lock(zoneMutex);
}

static __inline void unlockZone(void)
{
    Sys_Unlock(zoneMutex);
}

/**
 * Conversion from string to long, with the "k" and "m" suffixes.
 */
long superatol(char *s)
{
    char           *endptr;
    long            val = strtol(s, &endptr, 0);

    if(*endptr == 'k' || *endptr == 'K')
        val *= 1024;
    else if(*endptr == 'm' || *endptr == 'M')
        val *= 1048576;
    return val;
}

/**
 * Create a new memory volume.  The new volume is added to the list of
 * memory volumes.
 */
memvolume_t *Z_Create(size_t volumeSize)
{
    memblock_t     *block;
    memvolume_t    *vol = M_Calloc(sizeof(memvolume_t));

    lockZone();

    // Append to the end of the volume list.
    if(volumeLast)
        volumeLast->next = vol;
    volumeLast = vol;
    vol->next = 0;
    if(!volumeRoot)
        volumeRoot = vol;

    // Allocate memory for the zone volume.
    vol->size = volumeSize;
    vol->zone = M_Malloc(vol->size);
    vol->allocatedBytes = 0;

    // Clear the start of the zone.
    memset(vol->zone, 0, sizeof(memzone_t) + sizeof(memblock_t));
    vol->zone->size = vol->size;

    // Set the entire zone to one free block.
    vol->zone->blockList.next
        = vol->zone->blockList.prev
        = block
        = (memblock_t *) ((byte *) vol->zone + sizeof(memzone_t));

    vol->zone->blockList.user = (void *) vol->zone;
    vol->zone->blockList.volume = vol;
    vol->zone->blockList.tag = PU_APPSTATIC;
    vol->zone->rover = vol->zone->staticRover = block;

    block->prev = block->next = &vol->zone->blockList;
    block->user = NULL;         // free block
    block->seqFirst = block->seqLast = NULL;
    block->size = vol->zone->size - sizeof(memzone_t);

    unlockZone();

    VERBOSE(Con_Message("Z_Create: New %.1f MB memory volume.\n", vol->size / 1024.0 / 1024.0));

    Z_CheckHeap();

    return vol;
}

boolean Z_IsInited(void)
{
    return zoneMutex != 0;
}

int Z_Init(void)
{
    zoneMutex = Sys_CreateMutex("ZONE_MUTEX");

    // Create the first volume.
    Z_Create(MEMORY_VOLUME_SIZE);
    return true;
}

/**
 * Shut down the memory zone by destroying all the volumes.
 */
void Z_Shutdown(void)
{
    int             numVolumes = 0;
    size_t          totalMemory = 0;

    // Destroy all the memory volumes.
    while(volumeRoot)
    {
        memvolume_t *vol = volumeRoot;
        volumeRoot = vol->next;

        // Calculate stats.
        numVolumes++;
        totalMemory += vol->size;

#ifdef FAKE_MEMORY_ZONE
        Z_FreeTags(0, DDMAXINT);
#endif

        M_Free(vol->zone);
        M_Free(vol);
    }

    LegacyCore_PrintfLogFragmentAtLevel(de2LegacyCore, DE2_LOG_INFO,
            "Z_Shutdown: Used %i volumes, total %u bytes.\n", numVolumes, totalMemory);

    Sys_DestroyMutex(zoneMutex);
    zoneMutex = 0;
}

#ifdef FAKE_MEMORY_ZONE
memblock_t *Z_GetBlock(void *ptr)
{
    memvolume_t    *volume;
    memblock_t     *block;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if(block->area == ptr)
            {
                return block;
            }
        }
    }
    Con_Error("Z_GetBlock: There is no memory block for %p.\n", ptr);
    return NULL;
}
#endif

/**
 * Frees a block of memory allocated with Z_Malloc.
 *
 * @param ptr      Memory area to free.
 * @param tracked  Pointer to a tracked memory block. Will be updated
 *                 if affected by the operation.
 */
static void freeBlock(void* ptr, memblock_t** tracked)
{
    memblock_t     *block, *other;
    memvolume_t    *volume;

    if(!ptr)
    {
        VERBOSE(Con_Message("Z_Free: Warning: Attempt to free NULL ignored.\n") );
        return;
    }

    lockZone();

    block = Z_GetBlock(ptr);
    if(block->id != ZONEID)
    {
        unlockZone();
        Con_Error("Z_Free: Attempt to free pointer without ZONEID.");
    }

    // The block was allocated from this volume.
    volume = block->volume;

    if(block->user > (void **) 0x100) // Smaller values are not pointers.
        *block->user = 0; // Clear the user's mark.
    block->user = NULL; // Mark as free.
    block->tag = 0;
    block->volume = NULL;
    block->id = 0;

#ifdef FAKE_MEMORY_ZONE
    M_Free(block->area);
    block->area = NULL;
    block->areaSize = 0;
#endif

    /**
     * Erase the entire sequence, if there is one.
     * It would also be possible to carefully break the sequence in two
     * parts, but since PU_LEVELSTATICs aren't supposed to be freed one by
     * one, this this sufficient.
     */
    if(block->seqFirst)
    {
        memblock_t* first = block->seqFirst;
        memblock_t* iter = first;
        while(iter->seqFirst == first)
        {
            iter->seqFirst = iter->seqLast = NULL;
            iter = iter->next;
        }
    }

    // Keep tabs on how much memory is used.
    volume->allocatedBytes -= block->size;

    other = block->prev;
    if(!other->user)
    {
        // Merge with previous free block.
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;
        if(block == volume->zone->rover)
            volume->zone->rover = other;
        if(block == volume->zone->staticRover)
            volume->zone->staticRover = other;
        block = other;

        // Keep track of what happens to the referenced block.
        if(tracked && *tracked == block)
        {
            *tracked = other;
        }
    }

    other = block->next;
    if(!other->user)
    {
        // Merge the next free block onto the end.
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;
        if(other == volume->zone->rover)
            volume->zone->rover = block;
        if(other == volume->zone->staticRover)
            volume->zone->staticRover = block;

        // Keep track of what happens to the referenced block.
        if(tracked && *tracked == other)
        {
            *tracked = block;
        }
    }

    unlockZone();
}

void Z_Free(void *ptr)
{
    freeBlock(ptr, 0);
}

static __inline boolean isFreeBlock(memblock_t* block)
{
    return !block->user;
}

static __inline boolean isRootBlock(memvolume_t* vol, memblock_t* block)
{
    return block == &vol->zone->blockList;
}

static __inline memblock_t* advanceBlock(memvolume_t* vol, memblock_t* block)
{
    block = block->next;
    if(isRootBlock(vol, block))
    {
        // Continue from the beginning.
        block = vol->zone->blockList.next;
    }
    return block;
}

static __inline memblock_t* rewindRover(memvolume_t* vol, memblock_t* rover, int maxSteps, size_t optimal)
{
    memblock_t* base = rover;
    size_t prevBest = 0;
    int i;

    rover = rover->prev;
    for(i = 0; i < maxSteps && !isRootBlock(vol, rover); ++i)
    {
        // Looking for the smallest suitable free block.
        if(isFreeBlock(rover) && rover->size >= optimal && (!prevBest || rover->size < prevBest))
        {
            // Let's use this one.
            prevBest = rover->size;
            base = rover;
        }
        rover = rover->prev;
    }
    return base;
}

static __inline boolean isVolumeTooFull(memvolume_t* vol)
{
    return vol->allocatedBytes > vol->size * .95f;
}

/**
 * The static rovers should be rewound back near the beginning of the volume
 * periodically in order for them to be effective. Currently this is done
 * whenever tag ranges are purged (e.g., before map changes).
 */
static void rewindStaticRovers(void)
{
    memvolume_t* volume;
    for(volume = volumeRoot; volume; volume = volume->next)
    {
        memblock_t* block;
        for(block = volume->zone->blockList.next;
            !isRootBlock(volume, block); block = block->next)
        {
            // Let's find the first free block at the beginning of the volume.
            if(isFreeBlock(block))
            {
                volume->zone->staticRover = block;
                break;
            }
        }
    }
}

static void splitFreeBlock(memblock_t* block, size_t size)
{
    // There will be a new free fragment after the block.
    memblock_t* newBlock = (memblock_t *) ((byte*) block + size);
    newBlock->size = block->size - size;
    newBlock->user = NULL;       // free block
    newBlock->tag = 0;
    newBlock->volume = NULL;
    newBlock->prev = block;
    newBlock->next = block->next;
    newBlock->next->prev = newBlock;
    newBlock->seqFirst = newBlock->seqLast = NULL;
#ifdef FAKE_MEMORY_ZONE
    newBlock->area = 0;
    newBlock->areaSize = 0;
#endif
    block->next = newBlock;
    block->size = size;
}

/**
 * You can pass a NULL user if the tag is < PU_PURGELEVEL.
 */
void *Z_Malloc(size_t size, int tag, void *user)
{
    memblock_t* start, *iter;
    memvolume_t* volume;

    if(tag < PU_APPSTATIC || tag > PU_CACHE)
    {
        Con_Error("Z_Malloc: Invalid purgelevel %i.", tag);
    }

    if(!size)
    {
        // You can't allocate "nothing."
        return NULL;
    }

    lockZone();

    // Align to pointer size.
    size = ALIGNED(size);

    // Account for size of block header.
    size += sizeof(memblock_t);

    // Iterate through memory volumes until we can find one with enough free
    // memory. (Note: we *will* find one that's large enough.)
    for(volume = volumeRoot; ; volume = volume->next)
    {
        uint numChecked = 0;
        boolean gotoNextVolume = false;

        if(volume == NULL)
        {
            // We've run out of volumes.  Let's allocate a new one
            // with enough memory.
            size_t newVolumeSize = MEMORY_VOLUME_SIZE;

            if(newVolumeSize < size + 0x1000)
                newVolumeSize = size + 0x1000; // with some spare memory

            volume = Z_Create(newVolumeSize);
        }

        if(isVolumeTooFull(volume))
        {
            // We should skip this one.
            continue;
        }

        if(!volume->zone)
        {
            unlockZone();
            Con_Error("Z_Malloc: Volume without zone.");
        }

        // Scan through the block list looking for the first free block of
        // sufficient size, throwing out any purgable blocks along the
        // way.

        if(tag == PU_APPSTATIC || tag == PU_GAMESTATIC)
        {
            // Appstatic allocations may be around for a long time so make sure
            // they don't litter the volume. Their own rover will keep them as
            // tightly packed as possible.
            iter = volume->zone->staticRover;
        }
        else
        {
            // Everything else is allocated using the rover.
            iter = volume->zone->rover;
        }
        assert(iter->prev);

        // Back up a little to see if we have some space available nearby.
        start = iter = rewindRover(volume, iter, 3, size);
        numChecked = 0;

        // If the start is in a sequence, move it to the beginning of the
        // entire sequence. Sequences are handled as a single unpurgable entity,
        // so we can stop checking at its start.
        if(start->seqFirst)
        {
            start = start->seqFirst;
        }

        // We will scan ahead until we find something big enough.
        for( ; !(isFreeBlock(iter) && iter->size >= size); numChecked++)
        {
            // Check for purgable blocks we can dispose of.
            if(!isFreeBlock(iter))
            {
                if(iter->tag >= PU_PURGELEVEL)
                {
                    memblock_t* old = iter;
                    iter = iter->prev; // Step back.
#ifdef FAKE_MEMORY_ZONE
                    freeBlock(old->area, &start);
#else
                    freeBlock((byte *) old + sizeof(memblock_t), &start);
#endif
                }
                else
                {
                    if(iter->seqFirst)
                    {
                        // This block is part of a sequence of blocks, none of
                        // which can be purged. Skip the entire sequence.
                        iter = iter->seqFirst->seqLast;
                    }
                }
            }

            // Move to the next block.
            iter = advanceBlock(volume, iter);

            // Ensure that iter will eventually touch start.
            assert(!start->seqFirst || start->seqFirst == start ||
                   !start->seqFirst->prev->seqFirst ||
                   start->seqFirst->prev->seqFirst == start->seqFirst->prev->seqLast);

            if(iter == start && numChecked > 0)
            {
                // Scanned all the way through, no suitable space found.
                gotoNextVolume = true;
                LegacyCore_PrintfLogFragmentAtLevel(de2LegacyCore, DE2_LOG_DEBUG,
                        "Z_Malloc: gave up on volume after %i checks\n", numChecked);
                break;
            }
        }

        // At this point we've found/created a big enough block or we are
        // skipping this volume entirely.

        if(gotoNextVolume) continue;

        // Found a block big enough.
        if(iter->size - size > MINFRAGMENT)
        {
            splitFreeBlock(iter, size);
        }

#ifdef FAKE_MEMORY_ZONE
        iter->areaSize = size - sizeof(memblock_t);
        iter->area = M_Malloc(iter->areaSize);
#endif

        if(user)
        {
            iter->user = user;      // mark as an in use block
#ifdef FAKE_MEMORY_ZONE
            *(void **) user = iter->area;
#else
            *(void **) user = (void *) ((byte *) iter + sizeof(memblock_t));
#endif
        }
        else
        {
            if(tag >= PU_PURGELEVEL)
            {
                unlockZone();
                Con_Error("Z_Malloc: an owner is required for "
                          "purgable blocks.\n");
            }
            iter->user = MEMBLOCK_USER_ANONYMOUS; // mark as in use, but unowned
        }
        iter->tag = tag;

        if(tag == PU_MAPSTATIC)
        {
            // Level-statics are linked into unpurgable sequences so they can
            // be skipped en masse.
            iter->seqFirst = iter;
            iter->seqLast = iter;
            if(iter->prev->seqFirst)
            {
                iter->seqFirst = iter->prev->seqFirst;
                iter->seqFirst->seqLast = iter;
            }
        }
        else
        {
            // Not part of a sequence.
            iter->seqLast = iter->seqFirst = NULL;
        }

        // Next allocation will start looking here, at the rover.
        if(tag == PU_APPSTATIC || tag == PU_GAMESTATIC)
        {
            volume->zone->staticRover = advanceBlock(volume, iter);
        }
        else
        {
            volume->zone->rover = advanceBlock(volume, iter);
        }

        // Keep tabs on how much memory is used.
        volume->allocatedBytes += iter->size;

        iter->volume = volume;
        iter->id = ZONEID;

        unlockZone();

#ifdef FAKE_MEMORY_ZONE
        return iter->area;
#else
        return (void *) ((byte *) iter + sizeof(memblock_t));
#endif
    }
}

/**
 * Only resizes blocks with no user. If a block with a user is
 * reallocated, the user will lose its current block and be set to
 * NULL. Does not change the tag of existing blocks.
 */
void *Z_Realloc(void *ptr, size_t n, int mallocTag)
{
    int     tag = ptr ? Z_GetTag(ptr) : mallocTag;
    void   *p;

    lockZone();

    n = ALIGNED(n);
    p = Z_Malloc(n, tag, 0);    // User always 0;

    if(ptr)
    {
        size_t bsize;

        // Has old data; copy it.
        memblock_t *block = Z_GetBlock(ptr);
#ifdef FAKE_MEMORY_ZONE
        bsize = block->areaSize;
#else
        bsize = block->size - sizeof(memblock_t);
#endif
        memcpy(p, ptr, MIN_OF(n, bsize));
        Z_Free(ptr);
    }

    unlockZone();
    return p;
}

/**
 * Free memory blocks in all volumes with a tag in the specified range.
 */
void Z_FreeTags(int lowTag, int highTag)
{
    memvolume_t *volume;
    memblock_t *block, *next;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = next)
        {
            next = block->next;

            if(block->user) // An allocated block?
            {
                if(block->tag >= lowTag && block->tag <= highTag)
#ifdef FAKE_MEMORY_ZONE
                    Z_Free(block->area);
#else
                    Z_Free((byte *) block + sizeof(memblock_t));
#endif
            }
        }
    }

    // Now that there's plenty of new free space, let's keep the static
    // rover near the beginning of the volume.
    rewindStaticRovers();
}

/**
 * Check all zone volumes for consistency.
 */
void Z_CheckHeap(void)
{
    memvolume_t *volume;
    memblock_t *block;
    boolean     isDone;

#ifdef _DEBUG
    VERBOSE2( Con_Message("Z_CheckHeap\n") );
#endif

    lockZone();

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        size_t total = 0;

        // Validate the counter.
        if(allocatedMemoryInVolume(volume) != volume->allocatedBytes)
            Con_Error("Z_CheckHeap: allocated bytes counter is off (counter:%u != actual:%u)\n",
                      volume->allocatedBytes, allocatedMemoryInVolume(volume));

        // Does the memory in the blocks sum up to the total volume size?
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList; block = block->next)
        {
            total += block->size;
        }
        if(total != volume->size - sizeof(memzone_t))
            Con_Error("Z_CheckHeap: invalid total size of blocks (%u != %u)\n",
                      total, volume->size - sizeof(memzone_t));

        // Does the last block extend all the way to the end?
        block = volume->zone->blockList.prev;
        if((byte*)block - ((byte*)volume->zone + sizeof(memzone_t)) + block->size != volume->size - sizeof(memzone_t))
            Con_Error("Z_CheckHeap: last block does not cover the end (%u != %u)\n",
                      (byte*)block - ((byte*)volume->zone + sizeof(memzone_t)) + block->size,
                      volume->size - sizeof(memzone_t));

        block = volume->zone->blockList.next;
        isDone = false;

        while(!isDone)
        {
            if(block->next != &volume->zone->blockList)
            {
                if(block->size == 0)
                    Con_Error("Z_CheckHeap: zero-size block\n");
                if((byte *) block + block->size != (byte *) block->next)
                    Con_Error("Z_CheckHeap: block size does not touch the "
                              "next block\n");
                if(block->next->prev != block)
                    Con_Error("Z_CheckHeap: next block doesn't have proper "
                              "back link\n");
                if(!block->user && !block->next->user)
                    Con_Error("Z_CheckHeap: two consecutive free blocks\n");
                if(block->user == (void **) -1)
                    Con_Error("Z_CheckHeap: bad user pointer %p\n", block->user);

                /*
                if(block->seqFirst == block)
                {
                    // This is the first.
                    printf("sequence begins at (%p): start=%p, end=%p\n", block,
                           block->seqFirst, block->seqLast);
                }
                 */
                if(block->seqFirst)
                {
                    //printf("  seq member (%p): start=%p\n", block, block->seqFirst);
                    if(block->seqFirst->seqLast == block)
                    {
                        //printf("  -=- last member of seq %p -=-\n", block->seqFirst);
                    }
                    else
                    {
                        if(block->next->seqFirst != block->seqFirst)
                        {
                            Con_Error("Z_CheckHeap: disconnected sequence\n");
                        }
                    }
                }

                block = block->next;
            }
            else
                isDone = true; // all blocks have been hit
        }
    }

    unlockZone();
}

/**
 * Change the tag of a memory block.
 */
void Z_ChangeTag2(void *ptr, int tag)
{
    lockZone();
    {
        memblock_t *block = Z_GetBlock(ptr);

        if(block->id != ZONEID)
            Con_Error("Z_ChangeTag: Modifying a block without ZONEID.");

        if(tag >= PU_PURGELEVEL && (unsigned long) block->user < 0x100)
            Con_Error("Z_ChangeTag: An owner is required for purgable blocks.");
        block->tag = tag;
    }
    unlockZone();
}

/**
 * Change the user of a memory block.
 */
void Z_ChangeUser(void *ptr, void *newUser)
{
    lockZone();
    {
        memblock_t *block = Z_GetBlock(ptr);

        if(block->id != ZONEID)
            Con_Error("Z_ChangeUser: Block without ZONEID.");
        block->user = newUser;
    }
    unlockZone();
}

/**
 * Get the user of a memory block.
 */
void *Z_GetUser(void *ptr)
{
    memblock_t     *block = Z_GetBlock(ptr);

    if(block->id != ZONEID)
        Con_Error("Z_GetUser: Block without ZONEID.");
    return block->user;
}

/**
 * Get the tag of a memory block.
 */
int Z_GetTag(void *ptr)
{
    memblock_t     *block = Z_GetBlock(ptr);

    if(block->id != ZONEID)
        Con_Error("Z_GetTag: Block without ZONEID.");
    return block->tag;
}

boolean Z_Contains(void* ptr)
{
    memvolume_t* volume;
    memblock_t* block = Z_GetBlock(ptr);
    if(block->id != ZONEID)
    {
        // Could be in the zone, but does not look like an allocated block.
        return false;
    }
    // Check which volume is it.
    for(volume = volumeRoot; volume; volume = volume->next)
    {
        if((char*)ptr > (char*)volume->zone && (char*)ptr < (char*)volume->zone + volume->size)
        {
            // There it is.
            return true;
        }
    }
    return false;
}

/**
 * Memory allocation utility: malloc and clear.
 */
void *Z_Calloc(size_t size, int tag, void *user)
{
    void *ptr = Z_Malloc(size, tag, user);

    memset(ptr, 0, ALIGNED(size));
    return ptr;
}

/**
 * Realloc and set possible new memory to zero.
 */
void *Z_Recalloc(void *ptr, size_t n, int callocTag)
{
    memblock_t     *block;
    void           *p;
    size_t          bsize;

    lockZone();

    n = ALIGNED(n);

    if(ptr)                     // Has old data.
    {
        p = Z_Malloc(n, Z_GetTag(ptr), NULL);
        block = Z_GetBlock(ptr);
#ifdef FAKE_MEMORY_ZONE
        bsize = block->areaSize;
#else
        bsize = block->size - sizeof(memblock_t);
#endif
        if(bsize <= n)
        {
            memcpy(p, ptr, bsize);
            memset((char *) p + bsize, 0, n - bsize);
        }
        else
        {
            // New block is smaller.
            memcpy(p, ptr, n);
        }
        Z_Free(ptr);
    }
    else
    {   // Totally new allocation.
        p = Z_Calloc(n, callocTag, NULL);
    }

    unlockZone();

    return p;
}

char* Z_StrDup(const char* text)
{
    if(!text) return 0;
    {
    size_t len = strlen(text);
    char* buf = Z_Malloc(len + 1, PU_APPSTATIC, 0);
    strcpy(buf, text);
    return buf;
    }
}

void* Z_MemDup(const void* ptr, size_t size)
{
    void* copy = Z_Malloc(size, PU_APPSTATIC, 0);
    memcpy(copy, ptr, size);
    return copy;
}

uint Z_VolumeCount(void)
{
    memvolume_t    *volume;
    size_t          count = 0;

    lockZone();
    for(volume = volumeRoot; volume; volume = volume->next)
    {
        count++;
    }
    unlockZone();

    return count;
}

static size_t allocatedMemoryInVolume(memvolume_t* volume)
{
    memblock_t* block;
    size_t total = 0;

    for(block = volume->zone->blockList.next; !isRootBlock(volume, block);
        block = block->next)
    {
        if(!isFreeBlock(block))
        {
            total += block->size;
        }
    }
    return total;
}

/**
 * Calculate the size of allocated memory blocks in all volumes combined.
 */
static size_t Z_AllocatedMemory(void)
{
    memvolume_t* volume;
    size_t total = 0;

    lockZone();

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        total += allocatedMemoryInVolume(volume);
    }

    unlockZone();
    return total;
}

/**
 * Calculate the amount of unused memory in all volumes combined.
 */
size_t Z_FreeMemory(void)
{
    memvolume_t    *volume;
    memblock_t     *block;
    size_t          free = 0;

    lockZone();

    Z_CheckHeap();
    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if(!block->user)
            {
                free += block->size;
            }
        }
    }

    unlockZone();
    return free;
}

void Z_PrintStatus(void)
{
#ifdef _DEBUG
    size_t allocated = Z_AllocatedMemory();
    size_t wasted = Z_FreeMemory();

    Con_Message("Memory zone status: %u volumes, %u bytes allocated, %u bytes free (%f%% in use)\n",
                Z_VolumeCount(), (uint)allocated, (uint)wasted, (float)allocated/(float)(allocated+wasted)*100.f);
#endif
}

/**
 * Allocate a new block of memory to be used for linear object allocations.
 * A "zblock" (its from the zone).
 *
 * @param zblockset_t*  Block set into which the new block is added.
 */
static void addBlockToSet(zblockset_t* set)
{
    assert(set);
    {
    zblockset_block_t* block = 0;

    // Get a new block by resizing the blocks array. This is done relatively
    // seldom, since there is a large number of elements per each block.
    set->_blockCount++;
    set->_blocks = Z_Recalloc(set->_blocks, sizeof(zblockset_block_t) * set->_blockCount, set->_tag);

    DEBUG_VERBOSE_Message(("addBlockToSet: set=%p blockCount=%u elemSize=%u elemCount=%u (total=%u)\n",
                           set, set->_blockCount, (uint)set->_elementSize, set->_elementsPerBlock,
                           (uint)(set->_blockCount * set->_elementSize * set->_elementsPerBlock)));

    // Initialize the block's data.
    block = &set->_blocks[set->_blockCount - 1];
    block->max = set->_elementsPerBlock;
    block->elementSize = set->_elementSize;
    block->elements = Z_Malloc(block->elementSize * block->max, set->_tag, NULL);
    block->count = 0;
    }
}

void* ZBlockSet_Allocate(zblockset_t* set)
{
    zblockset_block_t* block = 0;
    void* element = 0;

    assert(set);
    lockZone();

    block = &set->_blocks[set->_blockCount - 1];

    // When this is called, there is always an available element in the topmost
    // block. We will return it.
    element = ((byte*)block->elements) + (block->elementSize * block->count);

    // Reserve the element.
    block->count++;

    // If we run out of space in the topmost block, add a new one.
    if(block->count == block->max)
    {
        // Just being cautious: adding a new block invalidates existing
        // pointers to the blocks.
        block = 0;

        addBlockToSet(set);
    }

    unlockZone();
    return element;
}

zblockset_t* ZBlockSet_New(size_t sizeOfElement, unsigned int batchSize, int tag)
{
    zblockset_t* set;

    if(sizeOfElement == 0)
        Con_Error("Attempted ZBlockSet_New with invalid sizeOfElement (==0).");

    if(batchSize == 0)
        Con_Error("Attempted ZBlockSet_New with invalid batchSize (==0).");

    // Allocate the blockset.
    set = Z_Calloc(sizeof(*set), tag, NULL);
    set->_elementsPerBlock = batchSize;
    set->_elementSize = sizeOfElement;
    set->_tag = tag;

    // Allocate the first block right away.
    addBlockToSet(set);

    return set;
}

void ZBlockSet_Delete(zblockset_t* set)
{
    lockZone();
    assert(set);

    // Free the elements from each block.
    { uint i;
    for(i = 0; i < set->_blockCount; ++i)
        Z_Free(set->_blocks[i].elements);
    }

    Z_Free(set->_blocks);
    Z_Free(set);
    unlockZone();
}

#ifdef _DEBUG
void Z_DrawRegion(memvolume_t* volume, RectRaw* rect, size_t start, size_t size, const float* color)
{
    const int bytesPerRow = (volume->size - sizeof(memzone_t)) / rect->size.height;
    const float toPixelScale = (float)rect->size.width / (float)bytesPerRow;
    const size_t edge = rect->origin.x + rect->size.width;
    int x = (start % bytesPerRow)*toPixelScale + rect->origin.x;
    int y = start / bytesPerRow + rect->origin.y;
    int pixels = MAX_OF(1, ceil(size * toPixelScale));

    assert(start + size <= volume->size);

    while(pixels > 0)
    {
        const int availPixels = edge - x;
        const int usedPixels = MIN_OF(availPixels, pixels);

        glColor4fv(color);
        glVertex2i(x, y);
        glVertex2i(x + usedPixels, y);

        pixels -= usedPixels;

        // Move to the next row.
        y++;
        x = rect->origin.x;
    }
}

void Z_DebugDrawVolume(memvolume_t* volume, RectRaw* rect)
{
    memblock_t* block;
    char* base = ((char*)volume->zone) + sizeof(memzone_t);
    float opacity = .85f;
    float colAppStatic[4]   = { 1, 1, 1, .65f };
    float colGameStatic[4]  = { 1, 0, 0, .65f };
    float colMap[4]         = { 0, 1, 0, .65f };
    float colMapStatic[4]   = { 0, .5f, 0, .65f };
    float colCache[4]       = { 1, 0, 1, .65f };
    float colOther[4]       = { 0, 0, 1, .65f };

    // Clear the background.
    glColor4f(0, 0, 0, opacity);
    GL_DrawRect(rect);

    // Outline.
    glLineWidth(1);
    glColor4f(1, 1, 1, opacity/2);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_DrawRect(rect);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBegin(GL_LINES);

    // Visualize each block.
    for(block = volume->zone->blockList.next;
        block != &volume->zone->blockList;
        block = block->next)
    {
        const float* color = colOther;
        if(!block->user) continue; // Free is black.

        // Choose the color for this block.
        switch(block->tag)
        {
        case PU_GAMESTATIC: color = colGameStatic; break;
        case PU_MAP:        color = colMap; break;
        case PU_MAPSTATIC:  color = colMapStatic; break;
        case PU_APPSTATIC:  color = colAppStatic; break;
        case PU_CACHE:      color = colCache; break;
        default:
            break;
        }

        Z_DrawRegion(volume, rect, (char*)block - base, block->size, color);
    }

    glEnd();

    if(isVolumeTooFull(volume))
    {
        glLineWidth(2);
        glColor4f(1, 0, 0, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        GL_DrawRect(rect);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void Z_DebugDrawer(void)
{
    memvolume_t* volume;
    int i, volCount, h;

    if(!CommandLine_Exists("-zonedebug")) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Draw each volume.
    lockZone();

    // Make sure all the volumes fit vertically.
    volCount = Z_VolumeCount();
    h = 200;
    if(h * volCount + 10*(volCount - 1) > Window_Height(theWindow))
    {
        h = (Window_Height(theWindow) - 10*(volCount - 1))/volCount;
    }

    i = 0;
    for(volume = volumeRoot; volume; volume = volume->next, ++i)
    {
        RectRaw rect;
        rect.size.width = MIN_OF(400, Window_Width(theWindow));
        rect.size.height = h;
        rect.origin.x = Window_Width(theWindow) - rect.size.width - 1;
        rect.origin.y = Window_Height(theWindow) - rect.size.height*(i+1) - 10*i - 1;
        Z_DebugDrawVolume(volume, &rect);
    }

    unlockZone();

    // Cleanup.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
#endif
