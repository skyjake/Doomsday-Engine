/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 */

/**
 * dd_zone.h: Memory Zone
 */

#ifndef __DOOMSDAY_ZONE_H__
#define __DOOMSDAY_ZONE_H__

/**
 * Define this to force all memory blocks to be allocated from
 * the real heap. Useful when debugging memory-related problems.
 */
//#define FAKE_MEMORY_ZONE 1

#define PU_REFRESHTEX   11 // Textures/Flats/refresh.
#define PU_REFRESHCM    12 // Colormap.
#define PU_REFRESHTRANS 13
#define PU_REFRESHSPR   14
#define PU_PATCH        15
#define PU_MODEL        16
#define PU_SPRITE       20

int             Z_Init(void);
void            Z_Shutdown(void);
void            Z_EnableFastMalloc(boolean isEnabled);
//void            Z_PrintStatus(void);
void*           Z_Malloc(size_t size, int tag, void* ptr);
void            Z_Free(void* ptr);
void            Z_FreeTags(int lowTag, int highTag);
void            Z_CheckHeap(void);
void            Z_ChangeTag2(void* ptr, int tag);
void            Z_ChangeUser(void* ptr, void* newUser);
void*           Z_GetUser(void* ptr);
int             Z_GetTag(void* ptr);
void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
void*           Z_Calloc(size_t size, int tag, void* user);
void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
size_t          Z_FreeMemory(void);

typedef struct memblock_s {
    size_t          size; // Including header and possibly tiny fragments.
    void**          user; // NULL if a free block.
    int             tag; // Purge level.
    int             id; // Should be ZONEID.
    struct memvolume_s* volume; // Volume this block belongs to.
    struct memblock_s* next, *prev;
    struct memblock_s* seqLast, *seqFirst;
#ifdef FAKE_MEMORY_ZONE
    void*           area; // The real memory area.
#endif
} memblock_t;

typedef struct {
    size_t          size; // Total bytes malloced, including header.
    memblock_t      blockList; // Start / end cap for linked list.
    memblock_t*     rover;
} memzone_t;

typedef struct zblockset_s {
    unsigned int    elementsPerBlock;
    size_t          elementSize;
    int             tag; // All blocks in a blockset have the same tag.
    unsigned int    count;
    struct zblock_s* blocks;
} zblockset_t;

zblockset_t*    Z_BlockCreate(size_t sizeOfElement, uint batchSize, int tag);
void            Z_BlockDestroy(zblockset_t* set);
void*           Z_BlockNewElement(zblockset_t* set);

#ifdef FAKE_MEMORY_ZONE
// Fake memory zone allocates memory from the real heap.
#define Z_ChangeTag(p,t) Z_ChangeTag2(p,t)

memblock_t*     Z_GetBlock(void* ptr);

#else
// Real memory zone allocates memory from a custom heap.
#define Z_GetBlock(ptr) ((memblock_t*) ((byte*)(ptr) - sizeof(memblock_t)))
#define Z_ChangeTag(p,t)                      \
{ \
if (( (memblock_t *)( (byte *)(p) - sizeof(memblock_t)))->id!=0x1d4a11) \
    Con_Error("Z_CT at "__FILE__":%i",__LINE__); \
Z_ChangeTag2(p,t); \
};
#endif


#endif
