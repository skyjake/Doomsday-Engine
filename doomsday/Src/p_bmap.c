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
 */

/*
 * p_bmap.c: Blockmaps
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define X_TO_BLOCK(x) ( ((x)-subMapOrigin[VX]) / blockSize[VX] )
#define Y_TO_BLOCK(y) ( ((y)-subMapOrigin[VY]) / blockSize[VY] )

// TYPES -------------------------------------------------------------------

typedef struct subsecnode_s {
	struct subsecnode_s *next;
	subsector_t *subsector;
} subsecnode_t;

typedef struct subsecmap_s {
	subsecnode_t *nodes;
	uint count;
} subsecmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static subsector_t	***subMap;	// array of subsector_t* arrays
static int subMapWidth, subMapHeight; // in blocks
static vec2_t subMapOrigin;
static vec2_t blockSize;

// CODE --------------------------------------------------------------------

void P_InitSubsectorBlockMap(void)
{
	int i, xl, xh, yl, yh, x, y;
	subsector_t *sub, **ptr;
	uint startTime = Sys_GetRealTime();
	subsecnode_t *iter, *next;
	subsecmap_t *map, *block;
	vec2_t bounds[2], point, dims;
	vertex_t *vtx;

	// Figure out the dimensions of the blockmap.
	for(i = 0; i < numvertexes; i++)
	{
		vtx = VERTEX_PTR(i);
		V2_Set(point, FIX2FLT(vtx->x), FIX2FLT(vtx->y));
		if(!i)
			V2_InitBox(bounds, point);
		else
			V2_AddToBox(bounds, point);
	}

	// Select a good size for the blocks.
	V2_Set(blockSize, 128, 128);
	V2_Copy(subMapOrigin, bounds[0]); // min point
	V2_Subtract(dims, bounds[1], bounds[0]);

	subMapWidth = ceil(dims[VX] / blockSize[VX]) + 1;
	subMapHeight = ceil(dims[VY] / blockSize[VY]) + 1;
			
	// The subsector blockmap is tagged as PU_LEVEL.
	subMap = Z_Calloc(subMapWidth * subMapHeight
		* sizeof(subsector_t**), PU_LEVEL, 0);

	// We'll construct the links using nodes.
	map = calloc(subMapWidth * subMapHeight, sizeof(subsecmap_t));

	// Process all the subsectors in the map.
	for(i = 0; i < numsubsectors; i++)
	{
		sub = SUBSECTOR_PTR(i);
		if(!sub->sector) continue;
		
		// Blockcoords to link to.
		xl = X_TO_BLOCK( sub->bbox[0].x );
		xh = X_TO_BLOCK( sub->bbox[1].x );
		yl = Y_TO_BLOCK( sub->bbox[0].y );
		yh = Y_TO_BLOCK( sub->bbox[1].y );

		for(x = xl; x <= xh; x++)
			for(y = yl; y <= yh; y++)		
			{
				if(x < 0 || y < 0 || x >= subMapWidth || y >= subMapHeight)
				{
					Con_Printf("sub%i: outside block x=%i, y=%i\n", i, x, y);
					continue;
				}

				// Create a new node.
				iter = malloc(sizeof(subsecnode_t));
				iter->subsector = sub;

				// Link to the temporary map.
				block = &map[x + y * subMapWidth];
				iter->next = block->nodes;
				block->nodes = iter;
				block->count++;
			}
	}

	// Create the actual links by 'hardening' the lists into arrays.
	for(i = 0, block = map; i < subMapWidth * subMapHeight; i++, block++)
		if(block->count > 0)
		{
			// An NULL-terminated array of pointers to subsectors.
			ptr = subMap[i] = Z_Malloc((block->count + 1) 
				* sizeof(subsector_t*),	PU_LEVEL, NULL);
			
			// Copy pointers to the array, delete the nodes.
			for(iter = block->nodes; iter; iter = next) 
			{
				*ptr++ = iter->subsector;
				// Kill the node.
				next = iter->next;
				free(iter);
			}

			// Terminate.
			*ptr = NULL;
		}

	// Free the temporary link map.
	free(map);

	// How much time did we spend?
	VERBOSE( Con_Message("P_InitSubsectorBlockMap: Done in %.2f seconds.\n", 
		(Sys_GetRealTime() - startTime) / 1000.0f) );
	VERBOSE( Con_Message("  (bs=%.0f/%.0f w=%i h=%i)\n",
						 blockSize[VX], blockSize[VY],
						 subMapWidth, subMapHeight) );
}

//==========================================================================
// P_InitPolyBlockMap
//	Allocates and clears the polyobj blockmap.
//	Normal blockmap must already be initialized when this is called.
//==========================================================================
void P_InitPolyBlockMap(void)
{
	if(verbose)
	{
		Con_Message("P_InitPolyBlockMap: w=%i h=%i\n", bmapwidth, bmapheight);
	}

	polyblockmap = Z_Malloc(bmapwidth * bmapheight * sizeof(polyblock_t*),
		PU_LEVEL, 0);
	memset(polyblockmap, 0, bmapwidth * bmapheight * sizeof(polyblock_t*));
}


//===========================================================================
// P_BlockLinesIterator
//	The validcount flags are used to avoid checking lines that are marked 
//	in multiple mapblocks, so increment validcount before the first call 
//	to P_BlockLinesIterator, then make one or more calls to it.
//===========================================================================
boolean P_BlockLinesIterator
	(int x, int y, boolean(*func)(line_t*,void*), void *data)
{
	int			offset;
	short		*list;
	line_t		*ld;
	int			i;
	polyblock_t *polyLink, *polyNext;
	seg_t		**tempSeg;
	
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
		return true;

	offset = y*bmapwidth+x;

	polyLink = polyblockmap[offset];
	while(polyLink)
	{
		polyNext = polyLink->next;
		if(polyLink->polyobj)
		{
			if(polyLink->polyobj->validcount != validcount)
			{
				polyLink->polyobj->validcount = validcount;
				tempSeg = polyLink->polyobj->segs;
				for(i = 0; i < polyLink->polyobj->numsegs; i++, tempSeg++)
				{
					if((*tempSeg)->linedef->validcount == validcount)
					{
						continue;
					}
					(*tempSeg)->linedef->validcount = validcount;
					if(!func((*tempSeg)->linedef, data))
					{
						return false;
					}
				}
			}
		}
		polyLink = polyNext;
	}

	offset = *(blockmap+offset);

	for ( list = blockmaplump+offset ; *list != -1 ; list++)
	{
		ld = LINE_PTR(*list);
		if (ld->validcount == validcount)
			continue;		// line has already been checked
		ld->validcount = validcount;
		
		if(!func(ld, data)) return false;
	}
	
	return true;		// everything was checked
}

//===========================================================================
// P_BlockPolyobjsIterator
//	The validcount flags are used to avoid checking polys
//	that are marked in multiple mapblocks, so increment validcount 
//	before the first call, then make one or more calls to it.
//===========================================================================
boolean P_BlockPolyobjsIterator
	(int x, int y, boolean(*func)(polyobj_t*,void*), void *data)
{
	polyblock_t *polyLink, *polyNext;
	
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
		return true;

	polyLink = polyblockmap[y*bmapwidth + x];
	while(polyLink)
	{
		polyNext = polyLink->next;
		if(polyLink->polyobj)
		{
			if(polyLink->polyobj->validcount != validcount)
			{
				polyLink->polyobj->validcount = validcount;
				if(!func(polyLink->polyobj, data)) return false;
			}
		}
		polyLink = polyNext;
	}
	return true;
}

//===========================================================================
// P_SubsectorBoxIteratorv
//	Same as the fixed-point version of this routine, but the bounding box
//	is specified using an vec2_t array (see m_vector.c).
//===========================================================================
boolean P_SubsectorBoxIteratorv(arvec2_t box, sector_t *sector,
								boolean (*func)(subsector_t*, void*),
								void *parm)
{
	subsector_t *sub, **iter;
	subsectorinfo_t *info;
	static int localValidCount = 0;
	int xl, xh, yl, yh, x, y;

	// This is only used here.
	localValidCount++;

	// Blockcoords to check.
	xl = X_TO_BLOCK( box[0][VX] );
	xh = X_TO_BLOCK( box[1][VX] );
	yl = Y_TO_BLOCK( box[0][VY] );
	yh = Y_TO_BLOCK( box[1][VY] );

	for(x = xl; x <= xh; x++)
	{
		for(y = yl; y <= yh; y++)
		{
			if(x < 0 || y < 0 || x >= subMapWidth || y >= subMapHeight)
				continue;

			if((iter = subMap[x + y * subMapWidth]) == NULL)
				continue;

			for(; *iter; iter++)
			{
				sub = *iter;
				info = SUBSECT_INFO(sub);

				if(info->validcount != localValidCount)
				{
					info->validcount = localValidCount;

					// Check the sector restriction.
					if(sector && sub->sector != sector) continue;
					
					// Check the bounds.
					if(sub->bbox[1].x < box[0][VX]
						|| sub->bbox[0].x > box[1][VX]
						|| sub->bbox[1].y < box[0][VY]
						|| sub->bbox[0].y > box[1][VY]) continue;

					if(!func(sub, parm)) return false;
				}
			}
		}
	}
	return true;
}

//===========================================================================
// P_SubsectorBoxIterator
//	Returns false only if the iterator func returns false.
//===========================================================================
boolean P_SubsectorBoxIterator
	(fixed_t *box, sector_t *sector, boolean (*func)(subsector_t*, void*), 
	 void *parm)
{
	vec2_t bounds[2];

	bounds[0][VX] = FIX2FLT( box[BOXLEFT] );
	bounds[0][VY] = FIX2FLT( box[BOXBOTTOM] );
	bounds[1][VX] = FIX2FLT( box[BOXRIGHT] );
	bounds[1][VY] = FIX2FLT( box[BOXTOP] );

	return P_SubsectorBoxIteratorv(bounds, sector, func, parm);
}
