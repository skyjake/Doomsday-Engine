/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * dd_zone.h: Memory Zone
 */

#ifndef __DOOMSDAY_ZONE_H__
#define __DOOMSDAY_ZONE_H__

#define MINIMUM_HEAP_SIZE	0x1000000		// 16 Mb
#define MAXIMUM_HEAP_SIZE	0x10000000		// 256 Mb

// tags < 50 are not overwritten until freed
#define	PU_STATIC		1			// static entire execution time
#define	PU_SOUND		2			// static while playing
#define	PU_MUSIC		3			// static while playing
#define	PU_DAVE			4			// anything else Dave wants static

#define PU_OPENGL		10			// OpenGL allocated stuff. 
#define PU_REFRESHTEX	11			// Textures/refresh.
#define PU_REFRESHCM	12			// Colormap.
#define PU_REFRESHTRANS	13
#define PU_REFRESHSPR	14
#define PU_FLAT			15
#define PU_MODEL		16
#define PU_SPRITE		20

#define	PU_LEVEL		50			// static until level exited
#define	PU_LEVSPEC		51			// a special thinker in a level
// tags >= 100 are purgable whenever needed
#define	PU_PURGELEVEL	100
#define	PU_CACHE		101


void	Z_Init (void);
void 	Z_PrintStatus(void);
void *	Z_Malloc (size_t size, int tag, void *ptr);
void	Z_Free (void *ptr);
void	Z_FreeTags (int lowtag, int hightag);
void	Z_CheckHeap (void);
void	Z_ChangeTag2 (void *ptr, int tag);
void	Z_ChangeUser(void *ptr, void *newuser); //-jk
void *	Z_GetUser(void *ptr);
int		Z_GetTag(void *ptr);
void *	Z_Realloc(void *ptr, size_t n, int malloctag);
void *	Z_Calloc(size_t size, int tag, void *user);
void *	Z_Recalloc(void *ptr, size_t n, int calloctag);
int		Z_FreeMemory(void);


typedef struct memblock_s
{
	size_t 				size;			// including the header and possibly tiny fragments
	void				**user; 		// NULL if a free block
	int 				tag;			// purgelevel
	int 				id; 			// should be ZONEID
	struct memblock_s	*next, *prev;
} memblock_t;

typedef struct
{
	size_t		size;		// total bytes malloced, including header
	memblock_t	blocklist;	// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;


#define Z_ChangeTag(p,t) \
{ \
if (( (memblock_t *)( (byte *)(p) - sizeof(memblock_t)))->id!=0x1d4a11) \
	Con_Error("Z_CT at "__FILE__":%i",__LINE__); \
Z_ChangeTag2(p,t); \
};


#endif
