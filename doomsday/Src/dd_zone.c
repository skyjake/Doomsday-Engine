/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define	ZONEID	0x1d4a11

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

memzone_t	*mainzone;
size_t		zoneSize;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

It is of no value to free a cachable block, because it will get overwritten
automatically if needed

==============================================================================
*/

//===========================================================================
// superatol
//===========================================================================
long superatol(char *s)
{
	char *endptr;
	long val = strtol(s, &endptr, 0);

	if(*endptr == 'k' || *endptr == 'K')
		val *= 1024;
	else if(*endptr == 'm' || *endptr == 'M')
		val *= 1048576;
	return val;
}

void *Z_Create(size_t *size)
{
#define RETRY_STEP	0x80000	// Half a meg.
	byte *ptr;

	// Check for the -maxzone option.
	if(ArgCheckWith("-maxzone", 1)) maxzone = superatol(ArgNext());

	zoneSize = maxzone;
	if(zoneSize < MINIMUM_HEAP_SIZE) zoneSize = MINIMUM_HEAP_SIZE;
	if(zoneSize > MAXIMUM_HEAP_SIZE) zoneSize = MAXIMUM_HEAP_SIZE;
	zoneSize += RETRY_STEP;
	
	do { // Until we get the memory (usually succeeds on the first try).
		zoneSize -= RETRY_STEP;		// leave some memory alone
		ptr = malloc(zoneSize);
	} while(!ptr);

	*size = zoneSize;
	return ptr;
}

//==========================================================================
// Z_PrintStatus
//==========================================================================
void Z_PrintStatus(void)
{
	Con_Message("Memory zone: %.1f Mb.\n", zoneSize/1024.0/1024.0);
	
	if(zoneSize < (size_t) maxzone)
	{
		Con_Message("  The requested amount was %.1f Mb.\n",
					maxzone/1024.0/1024.0);
	}

	if(zoneSize < 0x180000)
	{
		// FIXME: This is a strange place to check this...
		Con_Error("  Insufficient memory!");
	}
}

//===========================================================================
// Z_Init
//===========================================================================
void Z_Init (void)
{
	memblock_t	*block;
	size_t		size;

	mainzone = (memzone_t*) Z_Create(&size);
	mainzone->size = size;

// set the entire zone to one free block

	mainzone->blocklist.next = mainzone->blocklist.prev = block =
		(memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );
	mainzone->blocklist.user = (void *)mainzone;
	mainzone->blocklist.tag = PU_STATIC;
	mainzone->rover = block;
	
	block->prev = block->next = &mainzone->blocklist;
	block->user = NULL;	// free block
	block->size = mainzone->size - sizeof(memzone_t);
}

//===========================================================================
// Z_Free
//===========================================================================
void Z_Free (void *ptr)
{
	memblock_t	*block, *other;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Con_Error ("Z_Free: freed a pointer without ZONEID");
		
	if (block->user > (void **)0x100)	// smaller values are not pointers
		*block->user = 0;		// clear the user's mark
	block->user = NULL;	// mark as free
	block->tag = 0;
	block->id = 0;
	
	other = block->prev;
	if (!other->user)
	{	// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == mainzone->rover)
			mainzone->rover = other;
		block = other;
	}
	
	other = block->next;
	if (!other->user)
	{	// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == mainzone->rover)
			mainzone->rover = block;
	}
}

//===========================================================================
// Z_Malloc
//	You can pass a NULL user if the tag is < PU_PURGELEVEL.
//===========================================================================
#define MINFRAGMENT	64

void * Z_Malloc (size_t size, int tag, void *user)
{
	size_t		extra;
	memblock_t	*start, *rover, *new, *base;

	if(!size) 
	{
		// You can't allocate 'nothing'.
		return NULL;
	}

//
// scan through the block list looking for the first free block
// of sufficient size, throwing out any purgable blocks along the way
//
	size += sizeof(memblock_t);	// account for size of block header
	
//
// if there is a free block behind the rover, back up over them
//
	base = mainzone->rover;
	if (!base->prev->user)
		base = base->prev;
	
	rover = base;
	start = base->prev;
	
	do
	{
		if (rover == start)	// scanned all the way around the list
		{
			Con_Error("Z_Malloc: Failed on allocation of %i bytes.\n"
				"  Please increase the size of the memory zone "
				"with the -maxzone option.", size);
		}
		if (rover->user)
		{
			if (rover->tag < PU_PURGELEVEL)
			// hit a block that can't be purged, so move base past it
				base = rover = rover->next;
			else
			{
			// free the rover block (adding the size to base)
				base = base->prev;	// the rover can be the base block
				Z_Free ((byte *)rover+sizeof(memblock_t));
				base = base->next;
				rover = base->next;
			}
		}
		else
			rover = rover->next;
	} while (base->user || base->size < size);
	
//
// found a block big enough
//
	extra = base->size - size;
	if (extra >  MINFRAGMENT)
	{	// there will be a free fragment after the allocated block
		new = (memblock_t *) ((byte *)base + size );
		new->size = extra;
		new->user = NULL;		// free block
		new->tag = 0;
		new->prev = base;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->size = size;
	}
	
	if (user)
	{
		base->user = user;			// mark as an in use block
		*(void **)user = (void *) ((byte *)base + sizeof(memblock_t));
	}
	else
	{
		if (tag >= PU_PURGELEVEL)
			Con_Error ("Z_Malloc: an owner is required for purgable blocks");
		base->user = (void *)2;		// mark as in use, but unowned	
	}
	base->tag = tag;
	
	mainzone->rover = base->next;	// next allocation will start looking here
	
	base->id = ZONEID;
	return (void *) ((byte *)base + sizeof(memblock_t));
}

//===========================================================================
// Z_Realloc
//	Only resizes blocks with no user. If a block with a user is 
//	reallocated, the user will lose its current block and be set to
//	NULL. Does not change the tag of existing blocks.
//===========================================================================
void *Z_Realloc(void *ptr, size_t n, int malloctag)
{
	int tag = ptr? Z_GetTag(ptr) : malloctag;
	void *p = Z_Malloc(n, tag, 0); // User always 0
	if(ptr)
	{
		size_t bsize;
		// Has old data; copy it.
		memblock_t *block = (memblock_t*) ((char*)ptr - sizeof(memblock_t));
		bsize = block->size - sizeof(memblock_t);
		memcpy(p, ptr, MIN_OF(n, bsize));
		Z_Free(ptr);
	}
	return p;
}

//===========================================================================
// Z_FreeTags
//===========================================================================
void Z_FreeTags (int lowtag, int hightag)
{
	memblock_t	*block, *next;
	
	for(block = mainzone->blocklist.next; block != &mainzone->blocklist; 
		block = next)
	{
		next = block->next;		// get link before freeing
		if (!block->user)
			continue;			// free block
		if (block->tag >= lowtag && block->tag <= hightag)
			Z_Free ( (byte *)block+sizeof(memblock_t));
	}
}

//===========================================================================
// Z_CheckHeap
//===========================================================================
void Z_CheckHeap (void)
{
	memblock_t	*block;
	
	for (block = mainzone->blocklist.next ; ; block = block->next)
	{
		if (block->next == &mainzone->blocklist)
			break;			// all blocks have been hit	
		if (block->size == 0)
			Con_Error ("Z_CheckHeap: zero-size block\n");
		if ( (byte *)block + block->size != (byte *)block->next)
			Con_Error ("Z_CheckHeap: block size does not touch the next block\n");
		if ( block->next->prev != block)
			Con_Error ("Z_CheckHeap: next block doesn't have proper back link\n");
		if (!block->user && !block->next->user)
			Con_Error ("Z_CheckHeap: two consecutive free blocks\n");
		if (block->user == (void**) -1)
			Con_Error("Z_CheckHeap: bad user pointer %p\n", block->user);
	}
}

//===========================================================================
// Z_ChangeTag2
//===========================================================================
void Z_ChangeTag2 (void *ptr, int tag)
{
	memblock_t	*block;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Con_Error ("Z_ChangeTag: freed a pointer without ZONEID");
	if (tag >= PU_PURGELEVEL && (unsigned)block->user < 0x100)
		Con_Error ("Z_ChangeTag: an owner is required for purgable blocks");
	block->tag = tag;
}

//===========================================================================
// Z_ChangeUser
//===========================================================================
void Z_ChangeUser(void *ptr, void *newuser)
{
	memblock_t	*block;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Con_Error ("Z_ChangeUser: block without ZONEID");
	block->user = newuser;
}

//===========================================================================
// Z_GetUser
//===========================================================================
void *Z_GetUser(void *ptr)
{
	memblock_t	*block;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Con_Error ("Z_GetUser: block without ZONEID");
	return block->user;
}

//===========================================================================
// Z_GetTag
//===========================================================================
int Z_GetTag(void *ptr)
{
	memblock_t	*block;
	
	block = (memblock_t*) ( (byte*) ptr - sizeof(memblock_t));
	if(block->id != ZONEID)
		Con_Error("Z_GetTag: block without ZONEID");
	return block->tag;
}

//===========================================================================
// Z_Calloc
//	Memory allocation utility: malloc and clear.
//===========================================================================
void *Z_Calloc(size_t size, int tag, void *user)
{
	void *ptr = Z_Malloc(size, tag, user);
	// Failing a malloc is a fatal error.
	memset(ptr, 0, size);
	return ptr;
}

//===========================================================================
// Z_Recalloc
//	Realloc and set possible new memory to zero.
//===========================================================================
void *Z_Recalloc(void *ptr, size_t n, int calloctag)
{
	memblock_t *block;
	void *p;
	size_t bsize;

	if(ptr)	// Has old data.
	{
		p = Z_Malloc(n, Z_GetTag(ptr), NULL);
		block = (memblock_t*) ((char*)ptr - sizeof(memblock_t));
		bsize = block->size - sizeof(memblock_t);
		if(bsize <= n)
		{
			memcpy(p, ptr, bsize);
			memset((char*)p + bsize, 0, n - bsize);
		}
		else
		{
			// New block is smaller.
			memcpy(p, ptr, n);
		}
		Z_Free(ptr);
	}
	else	// Totally new allocation.
	{
		p = Z_Calloc(n, calloctag, NULL); 
	}
	return p;
}

//===========================================================================
// Z_FreeMemory
//===========================================================================
int Z_FreeMemory(void)
{
	memblock_t *block;
	int	free = 0;
	
	for(block = mainzone->blocklist.next; block != &mainzone->blocklist;
		block = block->next)
	{
		if(!block->user || block->tag >= PU_PURGELEVEL)
		{
			free += block->size;
		}
	}
	return free;
}
