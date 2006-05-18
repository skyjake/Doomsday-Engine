/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�en <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 *
 * Based on Hexen by Raven Software, and Doom by id Software.
 */

/*
 * dd_zone.c: Memory Zone
 *
 * There is never any space between memblocks, and there will never be 
 * two contiguous free memblocks.
 * 
 * The rover can be left pointing at a non-empty block.
 * 
 * It is of no value to free a cachable block, because it will get 
 * overwritten automatically if needed.
 *
 * The zone is composed of multiple memory volumes.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Size of one memory zone volume.
#define MEMORY_VOLUME_SIZE  0x2000000	// 32 Mb

#define	ZONEID	0x1d4a11

#define MINFRAGMENT	64

// TYPES -------------------------------------------------------------------

/*
 * The memory is composed of multiple volumes.  New volumes are
 * allocated when necessary.
 */
typedef struct memvolume_s {
    memzone_t  *zone;
    size_t      size;
    struct memvolume_s *next;
} memvolume_t;    

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static memvolume_t *volumeRoot;

// CODE --------------------------------------------------------------------

/*
 * Conversion from string to long, with the "k" and "m" suffixes.
 */
long superatol(char *s)
{
	char   *endptr;
	long    val = strtol(s, &endptr, 0);

	if(*endptr == 'k' || *endptr == 'K')
		val *= 1024;
	else if(*endptr == 'm' || *endptr == 'M')
		val *= 1048576;
	return val;
}

/*
 * Create a new memory volume.  The new volume is added to the list of
 * memory volumes.
 */
memvolume_t *Z_Create(size_t volumeSize)
{
    memblock_t *block;
    memvolume_t *vol = calloc(1, sizeof(memvolume_t));
    
    vol->next = volumeRoot;
    volumeRoot = vol;
    vol->size = volumeSize;

    // Allocate memory for the zone volume.
    vol->zone = malloc(vol->size);

    // Clear the start of the zone.
    memset(vol->zone, 0, sizeof(memzone_t) + sizeof(memblock_t));
    vol->zone->size = vol->size;

	// Set the entire zone to one free block.
	vol->zone->blocklist.next
        = vol->zone->blocklist.prev
        = block
        = (memblock_t *) ((byte *) vol->zone + sizeof(memzone_t));
    
	vol->zone->blocklist.user = (void *) vol->zone;
	vol->zone->blocklist.volume = vol;
	vol->zone->blocklist.tag = PU_STATIC;
	vol->zone->rover = block;

	block->prev = block->next = &vol->zone->blocklist;
	block->user = NULL;			// free block
	block->size = vol->zone->size - sizeof(memzone_t);

    printf("Z_Create: New %.1f MB memory volume.\n",
           vol->size / 1024.0 / 1024.0);

    return vol;
}

/*
 * Initialize the memory zone.
 */   
void Z_Init(void)
{
    // Create the first volume.
    Z_Create(MEMORY_VOLUME_SIZE);
}

/*
 * Shut down the memory zone by destroying all the volumes.
 */   
void Z_Shutdown(void)
{
    size_t totalMemory = 0;
    int numVolumes = 0;
    
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
        
        free(vol->zone);
        free(vol);
    }

    printf("Z_Shutdown: Used %i volumes, total %lu bytes.\n",
           numVolumes, totalMemory);
}

#ifdef FAKE_MEMORY_ZONE
memblock_t *Z_GetBlock(void *ptr)
{
    memvolume_t *volume;
	memblock_t *block;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blocklist.next;
            block != &volume->zone->blocklist;
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

/*
 * Free memory that was allocated with Z_Malloc.
 */
void Z_Free(void *ptr)
{
	memblock_t *block, *other;
    memvolume_t *volume;

	block = Z_GetBlock(ptr);
	if(block->id != ZONEID)
    {
		Con_Error("Z_Free: attempt to free pointer without ZONEID");
    }

    // The block was allocated from this volume.
    volume = block->volume;

	if(block->user > (void **) 0x100)	// smaller values are not pointers
		*block->user = 0;		// clear the user's mark
	block->user = NULL;			// mark as free
	block->tag = 0;
    block->volume = NULL;
	block->id = 0;

#ifdef FAKE_MEMORY_ZONE
    free(block->area);
    block->area = NULL;
#endif

	other = block->prev;
	if(!other->user)
	{							// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if(block == volume->zone->rover)
			volume->zone->rover = other;
		block = other;
	}

	other = block->next;
	if(!other->user)
	{							// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if(other == volume->zone->rover)
			volume->zone->rover = block;
	}
}

/*
 * You can pass a NULL user if the tag is < PU_PURGELEVEL.
 */
void *Z_Malloc(size_t size, int tag, void *user)
{
	size_t  extra;
	memblock_t *start, *rover, *new, *base;
    memvolume_t *volume;

	if(!size)
	{
		// You can't allocate "nothing."
		return NULL;
	}

    // Account for size of block header.
    size += sizeof(memblock_t);	
   
    // Iterate through memory volumes until we can find one with
    // enough free memory.  (Note: we *will* find one that's large
    // enough.)
    for(volume = volumeRoot; ; volume = volume->next)
    {
        if(volume == NULL)
        {
            // We've run out of volumes.  Let's allocate a new one
            // with enough memory.
            size_t newVolumeSize = MEMORY_VOLUME_SIZE;

            if(newVolumeSize < size + 0x1000)
                newVolumeSize = size + 0x1000; // with some spare memory

            volume = Z_Create(newVolumeSize);
        }
        
        // Scan through the block list looking for the first free block of
        // sufficient size, throwing out any purgable blocks along the
        // way.

        // If there is a free block behind the rover, back up over them.
        base = volume->zone->rover;
        if(!base->prev->user)
            base = base->prev;

        rover = base;
        start = base->prev;

        do
        {
            if(rover == start)		
            {
                // Scanned all the way around the list.
                // Move over to the next volume.
                goto nextVolume;
            }
            if(rover->user)
            {
                if(rover->tag < PU_PURGELEVEL)
                {
                    // Hit a block that can't be purged, so move base
                    // past it.
                    base = rover = rover->next;
                }
                else
                {
                    // Free the rover block (adding the size to base).
                    base = base->prev;	// the rover can be the base block
#ifdef FAKE_MEMORY_ZONE
                    Z_Free(rover->area);
#else                    
                    Z_Free((byte *) rover + sizeof(memblock_t));
#endif                    
                    base = base->next;
                    rover = base->next;
                }
            }
            else
            {
                rover = rover->next;
            }
        } while(base->user || base->size < size);

        // Found a block big enough.
        extra = base->size - size;
        if(extra > MINFRAGMENT)
        {
            // There will be a free fragment after the allocated
            // block.
            new = (memblock_t *) ((byte *) base + size);
            new->size = extra;
            new->user = NULL;		// free block
            new->tag = 0;
            new->volume = NULL;
            new->prev = base;
            new->next = base->next;
            new->next->prev = new;
            base->next = new;
            base->size = size;
        }

#ifdef FAKE_MEMORY_ZONE
        base->area = malloc(size - sizeof(memblock_t));	        
#endif
               
        if(user)
        {
            base->user = user;		// mark as an in use block
#ifdef FAKE_MEMORY_ZONE
            *(void **) user = base->area;
#else
            *(void **) user = (void *) ((byte *) base + sizeof(memblock_t));
#endif
        }
        else
        {
            if(tag >= PU_PURGELEVEL)
                Con_Error("Z_Malloc: an owner is required for "
                          "purgable blocks.\n");
            base->user = (void *) 2;	// mark as in use, but unowned  
        }
        base->tag = tag;

        // next allocation will start looking here
        volume->zone->rover = base->next;
        
        base->volume = volume;
        base->id = ZONEID;

#ifdef FAKE_MEMORY_ZONE
        return base->area;
#else
        return (void *) ((byte *) base + sizeof(memblock_t));
#endif

    nextVolume:;
        // Move to the next volume.
    }
}

/*
 * Only resizes blocks with no user. If a block with a user is
 * reallocated, the user will lose its current block and be set to
 * NULL. Does not change the tag of existing blocks.
 */
void *Z_Realloc(void *ptr, size_t n, int mallocTag)
{
	int     tag = ptr ? Z_GetTag(ptr) : mallocTag;
	void   *p = Z_Malloc(n, tag, 0);	// User always 0

	if(ptr)
	{
		size_t  bsize;

		// Has old data; copy it.
		memblock_t *block = Z_GetBlock(ptr);

		bsize = block->size - sizeof(memblock_t);
		memcpy(p, ptr, MIN_OF(n, bsize));
		Z_Free(ptr);
	}
	return p;
}

/*
 * Free memory blocks in all volumes with a tag in the specified range.
 */
void Z_FreeTags(int lowTag, int highTag)
{
    memvolume_t *volume;
	memblock_t *block, *next;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blocklist.next;
            block != &volume->zone->blocklist;
            block = next)
        {
            next = block->next;		// get link before freeing
            if(!block->user)
                continue;			// free block
            if(block->tag >= lowTag && block->tag <= highTag)
#ifdef FAKE_MEMORY_ZONE
                Z_Free(block->area);
#else
                Z_Free((byte *) block + sizeof(memblock_t));
#endif
        }
    }
}

/*
 * Check all zone volumes for consistency.
 */
void Z_CheckHeap(void)
{
    memvolume_t *volume;
	memblock_t *block;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blocklist.next; ; block = block->next)
        {
            if(block->next == &volume->zone->blocklist)
                break;				// all blocks have been hit 
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
        }
    }
}

/*
 * Change the tag of a memory block.
 */
void Z_ChangeTag2(void *ptr, int tag)
{
	memblock_t *block = Z_GetBlock(ptr);
    
	if(block->id != ZONEID)
		Con_Error("Z_ChangeTag: modifying a block without ZONEID");
	if(tag >= PU_PURGELEVEL && (unsigned) block->user < 0x100)
		Con_Error("Z_ChangeTag: an owner is required for purgable blocks");
	block->tag = tag;
}

/*
 * Change the user of a memory block.
 */
void Z_ChangeUser(void *ptr, void *newUser)
{
	memblock_t *block = Z_GetBlock(ptr);
    
	if(block->id != ZONEID)
		Con_Error("Z_ChangeUser: block without ZONEID");
	block->user = newUser;
}

/*
 * Get the user of a memory block.
 */
void *Z_GetUser(void *ptr)
{
	memblock_t *block = Z_GetBlock(ptr);
    
	if(block->id != ZONEID)
		Con_Error("Z_GetUser: block without ZONEID");
	return block->user;
}

/*
 * Get the tag of a memory block.
 */
int Z_GetTag(void *ptr)
{
	memblock_t *block = Z_GetBlock(ptr);

	if(block->id != ZONEID)
		Con_Error("Z_GetTag: block without ZONEID");
	return block->tag;
}

/*
 * Memory allocation utility: malloc and clear.
 */
void *Z_Calloc(size_t size, int tag, void *user)
{
	void   *ptr = Z_Malloc(size, tag, user);

	memset(ptr, 0, size);
	return ptr;
}

/*
 * Realloc and set possible new memory to zero.
 */
void *Z_Recalloc(void *ptr, size_t n, int callocTag)
{
	memblock_t *block;
	void   *p;
	size_t  bsize;

	if(ptr)						// Has old data.
	{
		p = Z_Malloc(n, Z_GetTag(ptr), NULL);
		block = Z_GetBlock(ptr);
		bsize = block->size - sizeof(memblock_t);
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
	else						// Totally new allocation.
	{
		p = Z_Calloc(n, callocTag, NULL);
	}
	return p;
}

/*
 * Calculate the amount of unused memory in all volumes combined.
 */
size_t Z_FreeMemory(void)
{
    memvolume_t *volume;
	memblock_t *block;
	size_t  free = 0;

    Z_CheckHeap();

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blocklist.next;
            block != &volume->zone->blocklist;
            block = block->next)
        {
            if(!block->user) 
            {
                free += block->size;
            }
        }
    }
	return free;
}
