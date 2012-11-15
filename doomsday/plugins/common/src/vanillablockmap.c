/**
 * @file vanillablockmap.c
 * Blockmap functionality guaranteed to work exactly like in the original DOOM.
 * @ingroup vanilla
 *
 * This code has been taken from linuxdoom-1.10 with as few modifications as
 * possible.
 *
 * @authors Copyright (c) 1993-1996 by id Software, Inc.
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "common.h"
#include "vanillablockmap.h"

#define MAPBLOCKSHIFT	(FRACBITS+7)
#define ML_BLOCKMAP     10 // DOOM format maps

static short* blockmap;
static short* blockmaplump;
static int bmapwidth;
static int bmapheight;
static fixed_t bmaporgx;
static fixed_t bmaporgy;

static void resetData(void)
{
    blockmap = blockmaplump = NULL;
}

static boolean doom_LoadBlockMap(int lump)
{
    int		i;
    int		count;
    size_t  size = W_LumpLength(lump);

    resetData();

    if(!size) return false;

    blockmaplump = Z_Malloc(size, PU_MAP, 0);
    W_ReadLump(lump, (uint8_t*) blockmaplump);

    blockmap = blockmaplump+4;
    count = size/2;

    for (i=0 ; i<count ; i++)
        blockmaplump[i] = SHORT(blockmaplump[i]);

    bmaporgx = blockmaplump[0]<<FRACBITS;
    bmaporgy = blockmaplump[1]<<FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];

    /// @todo Verify that the data looks valid; if not, kill it.

    return true;
}

//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//

static boolean doom_BlockLinesIterator
( int			x,
  int			y,
  boolean(*func)(LineDef*, void*) )
{
    int			offset;
    short*		list;
    LineDef*	ld;

    if (x<0
        || y<0
        || x>=bmapwidth
        || y>=bmapheight)
    {
        return true;
    }

    offset = y*bmapwidth+x;

    offset = *(blockmap+offset);

    for ( list = blockmaplump+offset ; *list != -1 ; list++)
    {
        ld = P_ToPtr(DMU_LINEDEF, *list);

        if (P_GetIntp(ld, DMU_VALID_COUNT) == VALIDCOUNT)
            continue; 	// line has already been checked

        P_SetIntp(ld, DMU_VALID_COUNT, VALIDCOUNT);

        if ( func(ld, NULL) )
            return false;
    }
    return true;	// everything was checked
}

boolean Vanilla_BlockLinesIterator(const AABoxd* box, int (*func)(LineDef*, void*))
{
    int xl = (FLT2FIX(box->minX) - bmaporgx)>>MAPBLOCKSHIFT;
    int xh = (FLT2FIX(box->maxX) - bmaporgx)>>MAPBLOCKSHIFT;
    int yl = (FLT2FIX(box->minY) - bmaporgy)>>MAPBLOCKSHIFT;
    int yh = (FLT2FIX(box->maxY) - bmaporgy)>>MAPBLOCKSHIFT;

    int bx;
    int by;

    DENG_ASSERT(Vanilla_IsBlockmapAvailable());

    for (bx=xl ; bx<=xh ; bx++)
        for (by=yl ; by<=yh ; by++)
            if (!doom_BlockLinesIterator (bx,by,func))
                return false;

    return true;
}

boolean Vanilla_LoadBlockmap(const char* mapId)
{
    int lump;

    resetData();

    lump = W_CheckLumpNumForName(mapId);
    if(lump < 0) return false;

    return doom_LoadBlockMap(lump + ML_BLOCKMAP);
}

boolean Vanilla_IsBlockmapAvailable()
{
    return blockmap != NULL;
}
