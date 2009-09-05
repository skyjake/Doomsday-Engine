/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 1999-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright (c) 2006-2009 Daniel Swanson <danij@dengine.net>
 * Copyright (c) 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * Copyright (c) 1993-1996 by id Software, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Zone"
#include "de/Log"
#include "de/math.h"

#include <cstdlib>
#include <cstring>

using namespace de;

/**
 * Size of one memory volume.
 */
#define MEMORY_VOLUME_SIZE  0x2000000   // 32 Mb

/**
 * Identifier for memory blocks.
 */
#define ZONEID  0x1d4a11

#define MINFRAGMENT (sizeof(MemBlock)+32)

struct Zone::MemBlock 
{
    dsize               size; // Including header and possibly tiny fragments.
    void**              user; // 0 if a free block.
    Zone::PurgeTag      tag; // Purge level.
    int                 id; // Should be ZONEID.
    Zone::MemVolume*    volume; // Volume this block belongs to.
    Zone::MemBlock*     next;
    Zone::MemBlock*     prev;
    Zone::MemBlock*     seqLast;
    Zone::MemBlock*     seqFirst;
#ifdef LIBDENG2_FAKE_MEMORY_ZONE
    void*               area; // The real memory area.
#endif
};

struct Zone::MemZone {
    dsize               size; // Total bytes malloced, including header.
    Zone::MemBlock      blockList; // Start / end cap for linked list.
    Zone::MemBlock*     rover;
};

/**
 * The memory is composed of multiple volumes.  New volumes are
 * allocated when necessary.
 */
struct Zone::MemVolume {
    Zone::MemZone*      zone;
    dsize               size;
    Zone::MemVolume*    next;
};

static void* M_Malloc(dsize amount)
{
    return new dbyte[amount];
}

static void* M_Calloc(dsize amount)
{
    void* ptr = M_Malloc(amount);
    std::memset(ptr, 0, amount);
    return ptr;
}

static void M_Free(void* ptr)
{
    delete [] static_cast<dbyte*>(ptr);
}

/*
 * Conversion from string to long, with the "k" and "m" suffixes.
 */
/*static long superatol(char *s)
{
    char           *endptr;
    long            val = strtol(s, &endptr, 0);

    if(*endptr == 'k' || *endptr == 'K')
        val *= 1024;
    else if(*endptr == 'm' || *endptr == 'M')
        val *= 1048576;
    return val;
}*/

Zone::Zone() : _volumeRoot(0), _fastMalloc(false)
{
    // Create the first volume.
    newVolume(MEMORY_VOLUME_SIZE);
}

Zone::~Zone()
{        
    LOG_AS("Zone::~Zone");
    
    // Delete the batches.
    while(!_batches.empty())
    {
        deleteBatch(_batches.front());
    }
    
    int numVolumes = 0;
    dsize totalMemory = 0;

    // Destroy all the memory volumes.
    while(_volumeRoot)
    {
        MemVolume *vol = _volumeRoot;
        _volumeRoot = vol->next;

        // Calculate stats.
        numVolumes++;
        totalMemory += vol->size;

#ifdef LIBDENG2_FAKE_MEMORY_ZONE
        Z_FreeTags(0, DDMAXINT);
#endif

        M_Free(vol->zone);
        M_Free(vol);
    }

    LOG_VERBOSE("Used %i volume(s), total %i bytes.") << numVolumes << totalMemory;
}

void Zone::enableFastMalloc(bool enabled)
{
    _fastMalloc = enabled;
}

void* Zone::alloc(dsize size, PurgeTag tag, void* user)
{
    dsize         extra;
    MemBlock     *start, *rover, *newb, *base;
    MemVolume    *volume;
    bool          gotoNextVolume;

    if(tag < STATIC || tag > CACHE)
    {
        /// @throw TagError @a tag is not one of the allowed purge tags.
        throw TagError("Zone::allocate", "Invalid purge tag");
    }

    if(!size)
    {
        // You can't allocate "nothing."
        return 0;
    }

    // Account for size of block header.
    size += sizeof(MemBlock);

    // Iterate through memory volumes until we can find one with
    // enough free memory.  (Note: we *will* find one that's large
    // enough.)
    for(volume = _volumeRoot; ; volume = volume->next)
    {
        if(volume == 0)
        {
            // We've run out of volumes.  Let's allocate a new one
            // with enough memory.
            dsize newVolumeSize = MEMORY_VOLUME_SIZE;

            if(newVolumeSize < size + 0x1000)
                newVolumeSize = size + 0x1000; // with some spare memory

            volume = newVolume(newVolumeSize);
        }

        assert(volume->zone != 0);

        // Scan through the block list looking for the first free block of
        // sufficient size, throwing out any purgable blocks along the
        // way.

        // If there is a free block behind the rover, back up over them.
        base = volume->zone->rover;
        assert(base->prev);
        if(!base->prev->user)
            base = base->prev;

        gotoNextVolume = false;
        if(_fastMalloc)
        {
            // In fast malloc mode, if the rover block isn't large enough,
            // just give up and move to the next volume right away.
            if(base->user || base->size < size)
            {
                gotoNextVolume = true;
            }
        }

        if(!gotoNextVolume)
        {
            bool isDone;

            rover = base;
            start = base->prev;

            // If the start is in a sequence, move it to the beginning of the
            // entire sequence. Sequences are handled as a single unpurgable entity,
            // so we can stop checking at its start.
            if(start->seqFirst)
            {
                start = start->seqFirst;
            }

            isDone = false;
            do
            {
                if(rover != start)
                {
                    if(rover->user)
                    {
                        if(rover->tag < PURGE_LEVEL)
                        {
                            if(rover->seqFirst)
                            {
                                // This block is part of a sequence of blocks, none of
                                // which can be purged. Skip the entire sequence.
                                base = rover = rover->seqFirst->seqLast->next;
                            }
                            else
                            {
                                // Hit a block that can't be purged, so move base
                                // past it.
                                base = rover = rover->next;
                            }
                        }
                        else
                        {
                            // Free the rover block (adding the size to base).
                            base = base->prev;  // the rover can be the base block
#ifdef LIBDENG2_FAKE_MEMORY_ZONE
                            free(rover->area);
#else
                            free((dbyte *) rover + sizeof(MemBlock));
#endif
                            base = base->next;
                            rover = base->next;
                        }
                    }
                    else
                    {
                        rover = rover->next;
                    }
                }
                else
                {
                    // Scanned all the way around the list.
                    // Move over to the next volume.
                    gotoNextVolume = true;
                }

                // Have we finished?
                if(gotoNextVolume)
                    isDone = true;
                else
                    isDone = !(base->user || base->size < size);
            } while(!isDone);

            // At this point we've found/created a big enough block or we are
            // skipping this volume entirely.

            if(!gotoNextVolume)
            {
                // Found a block big enough.
                extra = base->size - size;
                if(extra > MINFRAGMENT)
                {
                    // There will be a free fragment after the allocated
                    // block.
                    newb = (MemBlock *) ((dbyte *) base + size);
                    newb->size = extra;
                    newb->user = 0;       // free block
                    newb->tag = UNDEFINED;
                    newb->volume = 0;
                    newb->prev = base;
                    newb->next = base->next;
                    newb->next->prev = newb;
                    newb->seqFirst = newb->seqLast = 0;
                    base->next = newb;
                    base->size = size;
                }

#ifdef LIBDENG2_FAKE_MEMORY_ZONE
                base->area = M_Malloc(size - sizeof(MemBlock));
#endif

                if(user)
                {
                    base->user = (void**) user;      // mark as an in use block
#ifdef LIBDENG2_FAKE_MEMORY_ZONE
                    *(void **) user = base->area;
#else
                    *(void **) user = (void *) ((dbyte *) base + sizeof(MemBlock));
#endif
                }
                else
                {
                    if(tag >= PURGE_LEVEL)
                    {
                        /// @throw OwnerError Purgable memory blocks must have an owner.
                        throw OwnerError("Zone::allocate", "Owner is required for purgable blocks");
                    }
                    base->user = (void**) 2;    // mark as in use, but unowned
                }
                base->tag = tag;

                if(tag == MAP_STATIC)
                {
                    // Level-statics are linked into unpurgable sequences so they can
                    // be skipped en masse.
                    base->seqFirst = base;
                    base->seqLast = base;
                    if(base->prev->seqFirst)
                    {
                        base->seqFirst = base->prev->seqFirst;
                        base->seqFirst->seqLast = base;
                    }
                }
                else
                {
                    // Not part of a sequence.
                    base->seqLast = base->seqFirst = 0;
                }

                // next allocation will start looking here
                volume->zone->rover = base->next;

                base->volume = volume;
                base->id = ZONEID;

#ifdef LIBDENG2_FAKE_MEMORY_ZONE
                return base->area;
#else
                return (void *) ((dbyte *) base + sizeof(MemBlock));
#endif
            }
        }
        // Move to the next volume.
    }
}

void* Zone::allocClear(dsize size, PurgeTag tag, void* user)
{
    void* ptr = alloc(size, tag, user);
    std::memset(ptr, 0, size);
    return ptr;
}

void* Zone::resize(void* ptr, dsize n, PurgeTag tagForNewAlloc)
{
    PurgeTag tag = ptr ? getTag(ptr) : tagForNewAlloc;
    void* p = alloc(n, tag);    // User always 0

    if(ptr)
    {
        dsize  bsize;

        // Has old data; copy it.
        MemBlock *block = getBlock(ptr);

        bsize = block->size - sizeof(MemBlock);
        std::memcpy(p, ptr, min(n, bsize));
        free(ptr);
    }
    return p;
}

void* Zone::resizeClear(void* ptr, dsize newSize, PurgeTag tagForNewAlloc)
{
    MemBlock* block;
    void* p;
    dsize bsize;

    if(ptr)                     // Has old data.
    {
        p = alloc(newSize, getTag(ptr));
        block = getBlock(ptr);
        bsize = block->size - sizeof(MemBlock);
        if(bsize <= newSize)
        {
            std::memcpy(p, ptr, bsize);
            std::memset(static_cast<dbyte*>(p) + bsize, 0, newSize - bsize);
        }
        else
        {
            // New block is smaller.
            std::memcpy(p, ptr, newSize);
        }
        free(ptr);
    }
    else
    {   // Totally new allocation.
        p = allocClear(newSize, tagForNewAlloc);
    }

    return p;
}

void Zone::free(void *ptr)
{
    if(!ptr)
    {
        return;
    }

    MemBlock     *block, *other;
    MemVolume    *volume;

    block = getBlock(ptr);

    // The block was allocated from this volume.
    volume = block->volume;

    if(block->user > (void **) 0x100) // Smaller values are not pointers.
        *block->user = 0; // Clear the user's mark.
    block->user = 0; // Mark as free.
    block->tag = UNDEFINED;
    block->volume = 0;
    block->id = 0;

#ifdef LIBDENG2_FAKE_MEMORY_ZONE
    M_Free(block->area);
    block->area = 0;
#endif

    /**
     * Erase the entire sequence, if there is one.
     * It would also be possible to carefully break the sequence in two
     * parts, but since PU_LEVELSTATICs aren't supposed to be freed one by
     * one, this this sufficient.
     */
    if(block->seqFirst)
    {
        MemBlock* first = block->seqFirst;
        MemBlock* iter = first;
        while(iter->seqFirst == first)
        {
            iter->seqFirst = iter->seqLast = 0;
            iter = iter->next;
        }
    }

    other = block->prev;
    if(!other->user)
    {   
        // Merge with previous free block.
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;
        if(block == volume->zone->rover)
            volume->zone->rover = other;
        block = other;
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
    }
}

void Zone::purgeRange(PurgeTag lowTag, PurgeTag highTag)
{
    // Check batch allocator tags and free as called for.
    for(Batches::iterator i = _batches.begin(); i != _batches.end(); )
    {
        if((*i)->tag() >= lowTag && (*i)->tag() <= highTag)
        {
            delete *i;
            _batches.erase(i++);
        }
        else
        {
            ++i;
        }
    }
    
    // Check the volumes.
    MemVolume *volume;
    MemBlock *block, *next;
    for(volume = _volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = next)
        {
            next = block->next;     // Get link before freeing.
            if(block->user)
            {
                if(block->tag >= lowTag && block->tag <= highTag)
#ifdef LIBDENG2_FAKE_MEMORY_ZONE
                    free(block->area);
#else
                    free((dbyte *) block + sizeof(MemBlock));
#endif
            }
        }
    }
}

Zone::PurgeTag Zone::getTag(void* ptr) const
{
    return getBlock(ptr)->tag;
}

void Zone::setTag(void* ptr, PurgeTag tag)
{
    MemBlock *block = getBlock(ptr);
    if(tag >= PURGE_LEVEL && duint64(block->user) < 0x100)
    {
        /// @throw OwnerError Purgable memory blocks must have an owner.
        throw OwnerError("Zone::changeTag", "Owner is required for purgable blocks");
    }
    block->tag = tag;
}

void* Zone::getUser(void *ptr) const
{
    return getBlock(ptr)->user;
}

void Zone::setUser(void* ptr, void* newUser)
{
    getBlock(ptr)->user = (void**) newUser;
}

dsize Zone::availableMemory() const
{
    MemVolume* volume;
    MemBlock* block;
    dsize amount = 0;

    verify();

    for(volume = _volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if(!block->user)
            {
                amount += block->size;
            }
        }
    }

    return amount;
}

void Zone::verify() const
{
    MemVolume *volume;
    MemBlock *block;
    bool isDone;

    for(volume = _volumeRoot; volume; volume = volume->next)
    {
        block = volume->zone->blockList.next;
        isDone = false;
        while(!isDone)
        {
            if(block->next != &volume->zone->blockList)
            {
                if(block->size == 0)
                    /// @throw ConsistencyError A zero-sized memory block was encountered.
                    throw ConsistencyError("Zone::verify", "Zero-size block");
                if((dbyte *) block + block->size != (dbyte *) block->next)
                    /// @throw ConsistencyError Memory block's size does not touch the next block.
                    throw ConsistencyError("Zone::verify", "Block size does not touch the next block");
                if(block->next->prev != block)
                    /// @throw ConsistencyError The next block does not have a proper back link.
                    throw ConsistencyError("Zone::verify", "Next block doesn't have proper back link");
                if(!block->user && !block->next->user)
                    /// @throw ConsistencyError Two consecutive free blocks encountered.
                    throw ConsistencyError("Zone::verify", "Two consecutive free blocks");
                if(block->user == (void **) -1)
                    /// @throw ConsistencyError The user pointer of a memory block is invalid.
                    throw ConsistencyError("Zone::verify", "Bad user pointer");

                if(block->seqFirst)
                {
                    if(block->seqFirst->seqLast != block)
                    {
                        if(block->next->seqFirst != block->seqFirst)
                            /// @throw ConsistencyError A sequence of memory blocks is broken.
                            throw ConsistencyError("Zone::verify", "Disconnected sequence");
                    }
                }

                block = block->next;
            }
            else
            {
                isDone = true; // all blocks have been hit
            }
        }
    }
}

Zone::MemBlock *Zone::getBlock(void *ptr) const
{
    MemBlock     *block;

#ifdef LIBDENG2_FAKE_MEMORY_ZONE
    MemVolume    *volume;

    for(volume = _volumeRoot; volume; volume = volume->next)
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
    /// @throw ForeignError @a ptr does not point to a block within the zone.
    throw ForeignError("Zone::getBlock: Address not allocated within the zone");

#else

    block = ((MemBlock*) ((dbyte*)(ptr) - sizeof(MemBlock)));
#if _DEBUG
    if(block->id != ZONEID)
    {
        throw ForeignError("Zone::getBlock: Address not allocated within the zone");
    }
#endif
    return block;
#endif
}

Zone::MemVolume* Zone::newVolume(dsize volumeSize)
{
    LOG_AS("Zone::newVolume");
    
    MemBlock     *block;
    MemVolume    *vol = static_cast<MemVolume*>(M_Calloc(sizeof(MemVolume)));

    vol->next = _volumeRoot;
    _volumeRoot = vol;
    vol->size = volumeSize;

    // Allocate memory for the zone volume.
    vol->zone = static_cast<MemZone*>(M_Malloc(vol->size));

    // Clear the start of the zone.
    memset(vol->zone, 0, sizeof(MemZone) + sizeof(MemBlock));
    vol->zone->size = vol->size;

    // Set the entire zone to one free block.
    vol->zone->blockList.next
        = vol->zone->blockList.prev
        = block
        = (MemBlock *) ((dbyte *) vol->zone + sizeof(MemZone));

    vol->zone->blockList.user = (void **) vol->zone;
    vol->zone->blockList.volume = vol;
    vol->zone->blockList.tag = STATIC;
    vol->zone->rover = block;

    block->prev = block->next = &vol->zone->blockList;
    block->user = 0;         // free block
    block->seqFirst = block->seqLast = 0;
    block->size = vol->zone->size - sizeof(MemZone);

    LOG_DEBUG("New ") << vol->size / 1024.0 / 1024.0 << " MB memory volume.";

    return vol;
}

void Zone::deleteBatch(Batch* batch)
{
    for(Batches::iterator i = _batches.begin(); i != _batches.end(); ++i)
    {
        if(*i == batch)
        {
            _batches.erase(i);
            delete batch;
            return;
        }
    }
}
