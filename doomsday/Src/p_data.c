/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Based on Hexen by Raven Software.
 */

/*
 * p_data.c: Playsim Data Structures, Macros and Constants
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

#include "rend_bias.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     numvertexes;
byte   *vertexes;

int     numsegs;
byte   *segs;

int     numsectors;
byte   *sectors;

int     numsubsectors;
byte   *subsectors;

int     numnodes;
byte   *nodes;

int     numlines;
byte   *lines;

int     numsides;
byte   *sides;

short  *blockmaplump;			// offsets in blockmap are from here
short  *blockmap;
int     bmapwidth, bmapheight;	// in mapblocks
fixed_t bmaporgx, bmaporgy;		// origin of block map
linkmobj_t *blockrings;			// for thing rings
byte   *rejectmatrix;			// for fast sight rejection
polyblock_t **polyblockmap;		// polyobj blockmap
nodepile_t thingnodes, linenodes;	// all kinds of wacky links

ded_mapinfo_t *mapinfo = 0;		// Current mapinfo.
fixed_t mapgravity;				// Gravity for the current map.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// P_ValidateLevel
//  Make sure all texture references in the level data are good.
//===========================================================================
void P_ValidateLevel(void)
{
	int     i;

	for(i = 0; i < numsides; i++)
	{
		side_t *side = SIDE_PTR(i);

		if(side->toptexture > numtextures - 1)
			side->toptexture = 0;
		if(side->midtexture > numtextures - 1)
			side->midtexture = 0;
		if(side->bottomtexture > numtextures - 1)
			side->bottomtexture = 0;
	}
}

//==========================================================================
// P_LoadBlockMap
//==========================================================================
void P_LoadBlockMap(int lump)
{
	int     i, count;

	/*
	   // Disabled for now: Plutonia MAP28.
	   if(W_LumpLength(lump) > 0x7fff)  // From GMJ.
	   Con_Error("Invalid map - Try using this with Boomsday.\n");
	 */

	blockmaplump = W_CacheLumpNum(lump, PU_LEVEL);
	blockmap = blockmaplump + 4;
	count = W_LumpLength(lump) / 2;
	for(i = 0; i < count; i++)
		blockmaplump[i] = SHORT(blockmaplump[i]);

	bmaporgx = blockmaplump[0] << FRACBITS;
	bmaporgy = blockmaplump[1] << FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

	// Clear out mobj rings.
	count = sizeof(*blockrings) * bmapwidth * bmapheight;
	blockrings = Z_Malloc(count, PU_LEVEL, 0);
	memset(blockrings, 0, count);
	for(i = 0; i < bmapwidth * bmapheight; i++)
		blockrings[i].next = blockrings[i].prev = (mobj_t *) &blockrings[i];
}

/* 
   //==========================================================================
   // P_LoadBlockMap
   //   Modified for long blockmap/blockmaplump defs - GMJ Sep 2001
   //   I want to think about this a bit more... -jk
   //==========================================================================
   void P_LoadBlockMap(int lump)
   {
   long i, count = W_LumpLength(lump)/2;
   short *wadblockmaplump;

   if (count >= 0x10000)
   Con_Error("Map exceeds limits of +/- 32767 map units");

   wadblockmaplump = W_CacheLumpNum (lump, PU_LEVEL);
   blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

   // killough 3/1/98: Expand wad blockmap into larger internal one,
   // by treating all offsets except -1 as unsigned and zero-extending
   // them. This potentially doubles the size of blockmaps allowed,
   // because Doom originally considered the offsets as always signed.

   blockmaplump[0] = SHORT(wadblockmaplump[0]);
   blockmaplump[1] = SHORT(wadblockmaplump[1]);
   blockmaplump[2] = (long)(SHORT(wadblockmaplump[2])) & 0xffff;
   blockmaplump[3] = (long)(SHORT(wadblockmaplump[3])) & 0xffff;

   for (i=4 ; i<count ; i++)
   {
   short t = SHORT(wadblockmaplump[i]);          // killough 3/1/98
   blockmaplump[i] = t == -1 ? -1l : (long) t & 0xffff;
   }

   Z_Free(wadblockmaplump);

   bmaporgx = blockmaplump[0]<<FRACBITS;
   bmaporgy = blockmaplump[1]<<FRACBITS;
   bmapwidth = blockmaplump[2];
   bmapheight = blockmaplump[3];

   // Clear out mobj rings.
   count = sizeof(*blockrings) * bmapwidth * bmapheight;
   blockrings = Z_Malloc(count, PU_LEVEL, 0);
   memset(blockrings, 0, count);
   blockmap = blockmaplump + 4;
   for(i = 0; i < bmapwidth * bmapheight; i++)
   blockrings[i].next = blockrings[i].prev = (mobj_t*) &blockrings[i];
   } */

/*
 * Load the REJECT data lump.
 */
void P_LoadReject(int lump)
{
	rejectmatrix = W_CacheLumpNum(lump, PU_LEVEL);

	// If no reject matrix is found, issue a warning.
	if(rejectmatrix == NULL)
	{
		Con_Message("P_LoadReject: No REJECT data found.\n");
	}
}

void P_PlaneChanged(sector_t *sector, boolean theCeiling)
{
    int i, k;
    subsector_t *sub;
    seg_t *seg;
    
    // FIXME: Find a better way to find the subsectors of a sector.
    
    for(i = 0; i < numsubsectors; ++i)
    {
        sub = SUBSECTOR_PTR(i);

        // Only the subsectors of the changed sector.
        if(sub->sector != sector)
            continue;

        for(k = 0; k < sub->linecount; ++k)
        {
            seg = SEG_PTR(k + sub->firstline);

            // Inform the shadow bias of changed geometry.
            SB_SegHasMoved(seg);
        }

        // Inform the shadow bias of changed geometry.
        SB_PlaneHasMoved(sub, theCeiling);
    }
}

/*
 * When a change is made, this must be called to inform the engine of
 * it.  Repercussions include notifications to the renderer, network...
 */
void P_FloorChanged(sector_t *sector)
{
    P_PlaneChanged(sector, false);
}

void P_CeilingChanged(sector_t *sector)
{
    P_PlaneChanged(sector, true);
}

void P_PolyobjChanged(polyobj_t *po)
{
    seg_t **seg = po->segs;
    int i;

    for(i = 0; i < po->numsegs; ++i, ++seg)
    {
        // Shadow bias must be told.
        SB_SegHasMoved(*seg);
    }
}
