/**
 * @file memoryzone.c
 * Memory zone implementation.
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
 * It is not necessary to explicitly call Z_Free() on >= PU_PURGELEVEL blocks
 * because they will be automatically freed when the rover encounters them.
 *
 * @par Block Sequences
 * The PU_MAPSTATIC purge tag has a special purpose. It works like PU_MAP so
 * that it is purged on a per map basis, but blocks allocated as PU_MAPSTATIC
 * should not be freed at any time when the map is being used. Internally, the
 * map-static blocks are linked into sequences so that Z_Malloc knows to skip
 * all of them efficiently. This is possible because no block inside the
 * sequence could be purged by Z_Malloc() anyway.
 *
 * @author Copyright &copy; 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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
#include <string.h>
#include "de/garbage.h"
#include "de/legacy/memory.h"
#include "de/legacy/concurrency.h"
#include "de/c_wrapper.h"
#include "../src/legacy/memoryzone_private.h"

// Size of one memory zone volume.
#define MEMORY_VOLUME_SIZE  0x2000000   // 32 Mb

#define MINFRAGMENT (sizeof(memblock_t)+32)

#define ALIGNED(x) (((x) + sizeof(void *) - 1)&(~(sizeof(void *) - 1)))

/// Special user pointer for blocks that are in use but have no single owner.
#define MEMBLOCK_USER_ANONYMOUS    ((void *) 2)

// Used for block allocation of memory from the zone.
typedef struct zblockset_block_s {
    /// Maximum number of elements.
    unsigned int max;

    /// Number of used elements.
    unsigned int count;

    /// Size of a single element.
    size_t elementSize;

    /// Block of memory where elements are.
    void *elements;
} zblockset_block_t;

static memvolume_t *volumeRoot;
static memvolume_t *volumeLast;

static mutex_t zoneMutex = 0;

static size_t Z_AllocatedMemory(void);
static size_t allocatedMemoryInVolume(memvolume_t *volume);

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

    if (*endptr == 'k' || *endptr == 'K')
        val *= 1024;
    else if (*endptr == 'm' || *endptr == 'M')
        val *= 1048576;
    return val;
}

/**
 * Create a new memory volume.  The new volume is added to the list of
 * memory volumes.
 */
static memvolume_t *createVolume(size_t volumeSize)
{
    memblock_t     *block;
    memvolume_t    *vol = M_Calloc(sizeof(memvolume_t));

    lockZone();

    // Append to the end of the volume list.
    if (volumeLast)
        volumeLast->next = vol;
    volumeLast = vol;
    vol->next = 0;
    if (!volumeRoot)
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

    App_Log(DE2_LOG_MESSAGE,
            "Created a new %.1f MB memory volume.", vol->size / 1024.0 / 1024.0);

    Z_CheckHeap();

    return vol;
}

dd_bool Z_IsInited(void)
{
    return zoneMutex != 0;
}

int Z_Init(void)
{
    zoneMutex = Sys_CreateMutex("ZONE_MUTEX");

    // Create the first volume.
    createVolume(MEMORY_VOLUME_SIZE);
    return true;
}

void Z_Shutdown(void)
{
    int             numVolumes = 0;
    size_t          totalMemory = 0;

    // Get rid of possible zone-allocated memory in the garbage.
    Garbage_RecycleAllWithDestructor(Z_Free);

    // Destroy all the memory volumes.
    while (volumeRoot)
    {
        memvolume_t *vol = volumeRoot;
        volumeRoot = vol->next;

        // Calculate stats.
        numVolumes++;
        totalMemory += vol->size;

#ifdef DE_FAKE_MEMORY_ZONE
        Z_FreeTags(0, DDMAXINT);
#endif

        M_Free(vol->zone);
        M_Free(vol);
    }

    App_Log(DE2_LOG_NOTE,
            "Z_Shutdown: Used %i volumes, total %u bytes.", numVolumes, totalMemory);

    Sys_DestroyMutex(zoneMutex);
    zoneMutex = 0;
}

#ifdef DE_FAKE_MEMORY_ZONE
memblock_t *Z_GetBlock(void *ptr)
{
    memvolume_t    *volume;
    memblock_t     *block;

    for (volume = volumeRoot; volume; volume = volume->next)
    {
        for (block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if (block->area == ptr)
            {
                return block;
            }
        }
    }
    DE_ASSERT(false); // There is no memory block for this.
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
static void freeBlock(void *ptr, memblock_t **tracked)
{
    memblock_t     *block, *other;
    memvolume_t    *volume;

    if (!ptr) return;

    lockZone();

    block = Z_GetBlock(ptr);
    if (block->id != DE_ZONEID)
    {
        unlockZone();
        DE_ASSERT(block->id == DE_ZONEID);
        App_Log(DE2_LOG_WARNING,
                "Attempted to free pointer without ZONEID.");
        return;
    }

    // The block was allocated from this volume.
    volume = block->volume;

    if (block->user > (void **) 0x100) // Smaller values are not pointers.
        *block->user = 0; // Clear the user's mark.
    block->user = NULL; // Mark as free.
    block->tag = 0;
    block->volume = NULL;
    block->id = 0;

#ifdef DE_FAKE_MEMORY_ZONE
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
    if (block->seqFirst)
    {
        memblock_t *first = block->seqFirst;
        memblock_t *iter = first;
        while (iter->seqFirst == first)
        {
            iter->seqFirst = iter->seqLast = NULL;
            iter = iter->next;
        }
    }

    // Keep tabs on how much memory is used.
    volume->allocatedBytes -= block->size;

    other = block->prev;
    if (!other->user)
    {
        // Merge with previous free block.
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;
        if (block == volume->zone->rover)
            volume->zone->rover = other;
        if (block == volume->zone->staticRover)
            volume->zone->staticRover = other;
        block = other;

        // Keep track of what happens to the referenced block.
        if (tracked && *tracked == block)
        {
            *tracked = other;
        }
    }

    other = block->next;
    if (!other->user)
    {
        // Merge the next free block onto the end.
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;
        if (other == volume->zone->rover)
            volume->zone->rover = block;
        if (other == volume->zone->staticRover)
            volume->zone->staticRover = block;

        // Keep track of what happens to the referenced block.
        if (tracked && *tracked == other)
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

static __inline dd_bool isFreeBlock(memblock_t *block)
{
    return !block->user;
}

static __inline dd_bool isRootBlock(memvolume_t *vol, memblock_t *block)
{
    return block == &vol->zone->blockList;
}

static __inline memblock_t *advanceBlock(memvolume_t *vol, memblock_t *block)
{
    block = block->next;
    if (isRootBlock(vol, block))
    {
        // Continue from the beginning.
        block = vol->zone->blockList.next;
    }
    return block;
}

static __inline memblock_t *rewindRover(memvolume_t *vol, memblock_t *rover, int maxSteps, size_t optimal)
{
    memblock_t *base = rover;
    size_t prevBest = 0;
    int i;

    rover = rover->prev;
    for (i = 0; i < maxSteps && !isRootBlock(vol, rover); ++i)
    {
        // Looking for the smallest suitable free block.
        if (isFreeBlock(rover) && rover->size >= optimal && (!prevBest || rover->size < prevBest))
        {
            // Let's use this one.
            prevBest = rover->size;
            base = rover;
        }
        rover = rover->prev;
    }
    return base;
}

static dd_bool isVolumeTooFull(memvolume_t *vol)
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
    memvolume_t *volume;
    for (volume = volumeRoot; volume; volume = volume->next)
    {
        memblock_t *block;
        for (block = volume->zone->blockList.next;
            !isRootBlock(volume, block); block = block->next)
        {
            // Let's find the first free block at the beginning of the volume.
            if (isFreeBlock(block))
            {
                volume->zone->staticRover = block;
                break;
            }
        }
    }
}

static void splitFreeBlock(memblock_t *block, size_t size)
{
    // There will be a new free fragment after the block.
    memblock_t *newBlock = (memblock_t *) ((byte *) block + size);
    newBlock->size = block->size - size;
    newBlock->user = NULL;       // free block
    newBlock->tag = 0;
    newBlock->volume = NULL;
    newBlock->prev = block;
    newBlock->next = block->next;
    newBlock->next->prev = newBlock;
    newBlock->seqFirst = newBlock->seqLast = NULL;
#ifdef DE_FAKE_MEMORY_ZONE
    newBlock->area = 0;
    newBlock->areaSize = 0;
#endif
    block->next = newBlock;
    block->size = size;
}

void *Z_Malloc(size_t size, int tag, void *user)
{
    memblock_t *start, *iter;
    memvolume_t *volume;

    if (tag < PU_APPSTATIC || tag > PU_PURGELEVEL)
    {
        App_Log(DE2_LOG_WARNING, "Z_Malloc: Invalid purgelevel %i, cannot allocate memory.", tag);
        return NULL;
    }
    if (!size)
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
    // memory. (Note: we *will *find one that's large enough.)
    for (volume = volumeRoot; ; volume = volume->next)
    {
        uint numChecked = 0;
        dd_bool gotoNextVolume = false;

        if (volume == NULL)
        {
            // We've run out of volumes.  Let's allocate a new one
            // with enough memory.
            size_t newVolumeSize = MEMORY_VOLUME_SIZE;

            if (newVolumeSize < size + 0x1000)
                newVolumeSize = size + 0x1000; // with some spare memory

            volume = createVolume(newVolumeSize);
        }

        if (isVolumeTooFull(volume))
        {
            // We should skip this one.
            continue;
        }

        DE_ASSERT(volume->zone);

        // Scan through the block list looking for the first free block of
        // sufficient size, throwing out any purgable blocks along the
        // way.

        if (tag == PU_APPSTATIC || tag == PU_GAMESTATIC)
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
        if (start->seqFirst)
        {
            start = start->seqFirst;
        }

        // We will scan ahead until we find something big enough.
        for ( ; !(isFreeBlock(iter) && iter->size >= size); numChecked++)
        {
            // Check for purgable blocks we can dispose of.
            if (!isFreeBlock(iter))
            {
                if (iter->tag >= PU_PURGELEVEL)
                {
                    memblock_t *old = iter;
                    iter = iter->prev; // Step back.
#ifdef DE_FAKE_MEMORY_ZONE
                    freeBlock(old->area, &start);
#else
                    freeBlock((byte *) old + sizeof(memblock_t), &start);
#endif
                }
                else
                {
                    if (iter->seqFirst)
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

            if (iter == start && numChecked > 0)
            {
                // Scanned all the way through, no suitable space found.
                gotoNextVolume = true;
                App_Log(DE2_LOG_DEBUG,
                        "Z_Malloc: gave up on volume after %i checks", numChecked);
                break;
            }
        }

        // At this point we've found/created a big enough block or we are
        // skipping this volume entirely.

        if (gotoNextVolume) continue;

        // Found a block big enough.
        if (iter->size - size > MINFRAGMENT)
        {
            splitFreeBlock(iter, size);
        }

#ifdef DE_FAKE_MEMORY_ZONE
        iter->areaSize = size - sizeof(memblock_t);
        iter->area = M_Malloc(iter->areaSize);
#endif

        if (user)
        {
            iter->user = user;      // mark as an in use block
#ifdef DE_FAKE_MEMORY_ZONE
            *(void **) user = iter->area;
#else
            *(void **) user = (void *) ((byte *) iter + sizeof(memblock_t));
#endif
        }
        else
        {
            // An owner is required for purgable blocks.
            DE_ASSERT(tag < PU_PURGELEVEL);

            iter->user = MEMBLOCK_USER_ANONYMOUS; // mark as in use, but unowned
        }
        iter->tag = tag;

        if (tag == PU_MAPSTATIC)
        {
            // Level-statics are linked into unpurgable sequences so they can
            // be skipped en masse.
            iter->seqFirst = iter;
            iter->seqLast = iter;
            if (iter->prev->seqFirst)
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
        if (tag == PU_APPSTATIC || tag == PU_GAMESTATIC)
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
        iter->id = DE_ZONEID;

        unlockZone();

#ifdef DE_FAKE_MEMORY_ZONE
        return iter->area;
#else
        return (void *) ((byte *) iter + sizeof(memblock_t));
#endif
    }
}

void *Z_Realloc(void *ptr, size_t n, int mallocTag)
{
    int     tag = ptr ? Z_GetTag(ptr) : mallocTag;
    void   *p;

    lockZone();

    n = ALIGNED(n);
    p = Z_Malloc(n, tag, 0);    // User always 0;

    if (ptr)
    {
        size_t bsize;

        // Has old data; copy it.
        memblock_t *block = Z_GetBlock(ptr);
#ifdef DE_FAKE_MEMORY_ZONE
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

void Z_FreeTags(int lowTag, int highTag)
{
    memvolume_t *volume;
    memblock_t *block, *next;

    App_Log(DE2_LOG_DEBUG,
            "MemoryZone: Freeing all blocks in tag range:[%i, %i)",
            lowTag, highTag+1);

    for (volume = volumeRoot; volume; volume = volume->next)
    {
        for (block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = next)
        {
            next = block->next;

            if (block->user) // An allocated block?
            {
                if (block->tag >= lowTag && block->tag <= highTag)
#ifdef DE_FAKE_MEMORY_ZONE
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

void Z_CheckHeap(void)
{
    memvolume_t *volume;
    memblock_t *block;
    dd_bool     isDone;

    App_Log(DE2_LOG_TRACE, "Z_CheckHeap");

    lockZone();

    for (volume = volumeRoot; volume; volume = volume->next)
    {
        size_t total = 0;

        // Validate the counter.
        if (allocatedMemoryInVolume(volume) != volume->allocatedBytes)
        {
            App_Log(DE2_LOG_CRITICAL,
                "Z_CheckHeap: allocated bytes counter is off (counter:%u != actual:%u)",
                volume->allocatedBytes, allocatedMemoryInVolume(volume));
            App_FatalError("Z_CheckHeap: zone book-keeping is wrong");
        }

        // Does the memory in the blocks sum up to the total volume size?
        for (block = volume->zone->blockList.next;
            block != &volume->zone->blockList; block = block->next)
        {
            total += block->size;
        }
        if (total != volume->size - sizeof(memzone_t))
        {
            App_Log(DE2_LOG_CRITICAL,
                    "Z_CheckHeap: invalid total size of blocks (%u != %u)",
                    total, volume->size - sizeof(memzone_t));
            App_FatalError("Z_CheckHeap: zone book-keeping is wrong");
        }

        // Does the last block extend all the way to the end?
        block = volume->zone->blockList.prev;
        if ((byte *)block - ((byte *)volume->zone + sizeof(memzone_t)) + block->size != volume->size - sizeof(memzone_t))
        {
            App_Log(DE2_LOG_CRITICAL,
                    "Z_CheckHeap: last block does not cover the end (%u != %u)",
                     (byte *)block - ((byte *)volume->zone + sizeof(memzone_t)) + block->size,
                     volume->size - sizeof(memzone_t));
            App_FatalError("Z_CheckHeap: zone is corrupted");
        }

        block = volume->zone->blockList.next;
        isDone = false;

        while (!isDone)
        {
            if (block->next != &volume->zone->blockList)
            {
                if (block->size == 0)
                    App_FatalError("Z_CheckHeap: zero-size block");
                if ((byte *) block + block->size != (byte *) block->next)
                    App_FatalError("Z_CheckHeap: block size does not touch the "
                              "next block");
                if (block->next->prev != block)
                    App_FatalError("Z_CheckHeap: next block doesn't have proper "
                              "back link");
                if (!block->user && !block->next->user)
                    App_FatalError("Z_CheckHeap: two consecutive free blocks");
                if (block->user == (void **) -1)
                {
                    DE_ASSERT(block->user != (void **) -1);
                    App_FatalError("Z_CheckHeap: bad user pointer");
                }

                /*
                if (block->seqFirst == block)
                {
                    // This is the first.
                    printf("sequence begins at (%p): start=%p, end=%p\n", block,
                           block->seqFirst, block->seqLast);
                }
                 */
                if (block->seqFirst)
                {
                    //printf("  seq member (%p): start=%p\n", block, block->seqFirst);
                    if (block->seqFirst->seqLast == block)
                    {
                        //printf("  -=- last member of seq %p -=-\n", block->seqFirst);
                    }
                    else
                    {
                        if (block->next->seqFirst != block->seqFirst)
                        {
                            App_FatalError("Z_CheckHeap: disconnected sequence");
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

void Z_ChangeTag2(void *ptr, int tag)
{
    lockZone();
    {
        memblock_t *block = Z_GetBlock(ptr);

        DE_ASSERT(block->id == DE_ZONEID);

        if (tag >= PU_PURGELEVEL && PTR2INT(block->user) < 0x100)
        {
            App_Log(DE2_LOG_ERROR,
                "Z_ChangeTag: An owner is required for purgable blocks.");
        }
        else
        {
            block->tag = tag;
        }
    }
    unlockZone();
}

void Z_ChangeUser(void *ptr, void *newUser)
{
    lockZone();
    {
        memblock_t *block = Z_GetBlock(ptr);
        DE_ASSERT(block->id == DE_ZONEID);
        block->user = newUser;
    }
    unlockZone();
}

uint Z_GetId(void *ptr)
{
    return ((memblock_t *) ((byte *)(ptr) - sizeof(memblock_t)))->id;
}

void *Z_GetUser(void *ptr)
{
    memblock_t *block = Z_GetBlock(ptr);

    DE_ASSERT(block->id == DE_ZONEID);
    return block->user;
}

/**
 * Get the tag of a memory block.
 */
int Z_GetTag(void *ptr)
{
    memblock_t *block = Z_GetBlock(ptr);

    DE_ASSERT(block->id == DE_ZONEID);
    return block->tag;
}

dd_bool Z_Contains(void *ptr)
{
    memvolume_t *volume;
    memblock_t *block = Z_GetBlock(ptr);
    DE_ASSERT(Z_IsInited());
    if (block->id != DE_ZONEID)
    {
        // Could be in the zone, but does not look like an allocated block.
        return false;
    }
    // Check which volume is it.
    for (volume = volumeRoot; volume; volume = volume->next)
    {
        if ((char *)ptr > (char *)volume->zone && (char *)ptr < (char *)volume->zone + volume->size)
        {
            // There it is.
            return true;
        }
    }
    return false;
}

void *Z_Calloc(size_t size, int tag, void *user)
{
    void *ptr = Z_Malloc(size, tag, user);

    memset(ptr, 0, ALIGNED(size));
    return ptr;
}

void *Z_Recalloc(void *ptr, size_t n, int callocTag)
{
    memblock_t     *block;
    void           *p;
    size_t          bsize;

    lockZone();

    n = ALIGNED(n);

    if (ptr)                     // Has old data.
    {
        p = Z_Malloc(n, Z_GetTag(ptr), NULL);
        block = Z_GetBlock(ptr);
#ifdef DE_FAKE_MEMORY_ZONE
        bsize = block->areaSize;
#else
        bsize = block->size - sizeof(memblock_t);
#endif
        if (bsize <= n)
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

char *Z_StrDup(char const *text)
{
    if (!text) return 0;
    {
    size_t len = strlen(text);
    char *buf = Z_Malloc(len + 1, PU_APPSTATIC, 0);
    strcpy(buf, text);
    return buf;
    }
}

void *Z_MemDup(void const *ptr, size_t size)
{
    void *copy = Z_Malloc(size, PU_APPSTATIC, 0);
    memcpy(copy, ptr, size);
    return copy;
}

uint Z_VolumeCount(void)
{
    memvolume_t    *volume;
    size_t          count = 0;

    lockZone();
    for (volume = volumeRoot; volume; volume = volume->next)
    {
        count++;
    }
    unlockZone();

    return count;
}

static size_t allocatedMemoryInVolume(memvolume_t *volume)
{
    memblock_t *block;
    size_t total = 0;

    for (block = volume->zone->blockList.next; !isRootBlock(volume, block);
        block = block->next)
    {
        if (!isFreeBlock(block))
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
    memvolume_t *volume;
    size_t total = 0;

    lockZone();

    for (volume = volumeRoot; volume; volume = volume->next)
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
    for (volume = volumeRoot; volume; volume = volume->next)
    {
        for (block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if (!block->user)
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
    size_t allocated = Z_AllocatedMemory();
    size_t wasted    = Z_FreeMemory();

    App_Log(DE2_LOG_DEBUG,
            "Memory zone status: %u volumes, %u bytes allocated, %u bytes free (%f%% in use)",
            Z_VolumeCount(), (uint)allocated, (uint)wasted, (float)allocated/(float)(allocated+wasted)*100.f);
}

void Garbage_Trash(void *ptr)
{
    Garbage_TrashInstance(ptr, Z_Contains(ptr)? Z_Free : free);
}

/**
 * Allocate a new block of memory to be used for linear object allocations.
 * A "zblock" (its from the zone).
 *
 * @param set   Block set into which the new block is added.
 */
static void addBlockToSet(zblockset_t *set)
{
    assert(set);
    {
    zblockset_block_t *block = 0;

    // Get a new block by resizing the blocks array. This is done relatively
    // seldom, since there is a large number of elements per each block.
    set->_blockCount++;
    set->_blocks = Z_Recalloc(set->_blocks, sizeof(zblockset_block_t) * set->_blockCount, set->_tag);

    App_Log(DE2_LOG_DEBUG,
            "addBlockToSet: set=%p blockCount=%u elemSize=%u elemCount=%u (total=%u)",
             set, set->_blockCount, (uint)set->_elementSize, set->_elementsPerBlock,
            (uint)(set->_blockCount * set->_elementSize * set->_elementsPerBlock));

    // Initialize the block's data.
    block = &set->_blocks[set->_blockCount - 1];
    block->max = set->_elementsPerBlock;
    block->elementSize = set->_elementSize;
    block->elements = Z_Malloc(block->elementSize * block->max, set->_tag, NULL);
    block->count = 0;
    }
}

void *ZBlockSet_Allocate(zblockset_t *set)
{
    zblockset_block_t *block = 0;
    void *element = 0;

    assert(set);
    lockZone();

    block = &set->_blocks[set->_blockCount - 1];

    // When this is called, there is always an available element in the topmost
    // block. We will return it.
    element = ((byte *)block->elements) + (block->elementSize * block->count);

    // Reserve the element.
    block->count++;

    // If we run out of space in the topmost block, add a new one.
    if (block->count == block->max)
    {
        // Just being cautious: adding a new block invalidates existing
        // pointers to the blocks.
        block = 0;

        addBlockToSet(set);
    }

    unlockZone();
    return element;
}

zblockset_t *ZBlockSet_New(size_t sizeOfElement, uint32_t batchSize, int tag)
{
    zblockset_t *set;

    DE_ASSERT(sizeOfElement > 0);
    DE_ASSERT(batchSize > 0);

    // Allocate the blockset.
    set = Z_Calloc(sizeof(*set), tag, NULL);
    set->_elementsPerBlock = batchSize;
    set->_elementSize = sizeOfElement;
    set->_tag = tag;

    // Allocate the first block right away.
    addBlockToSet(set);

    return set;
}

void ZBlockSet_Delete(zblockset_t *set)
{
    lockZone();
    assert(set);

    // Free the elements from each block.
    { uint i;
    for (i = 0; i < set->_blockCount; ++i)
        Z_Free(set->_blocks[i].elements);
    }

    Z_Free(set->_blocks);
    Z_Free(set);
    unlockZone();
}

#ifdef DE_DEBUG
void Z_GetPrivateData(MemoryZonePrivateData *pd)
{
    pd->volumeCount     = Z_VolumeCount();
    pd->volumeRoot      = volumeRoot;
    pd->lock            = lockZone;
    pd->unlock          = unlockZone;
    pd->isVolumeTooFull = isVolumeTooFull;
}
#endif
