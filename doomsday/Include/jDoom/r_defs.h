// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//      Refresh/rendering module, shared data struct definitions.
//
//-----------------------------------------------------------------------------


#ifndef __R_DEFS__
#define __R_DEFS__


// Screenwidth.
#include "doomdef.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#include "p_xg.h"



#ifdef __GNUG__
#pragma interface
#endif



// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE		0
#define SIL_BOTTOM		1
#define SIL_TOP			2
#define SIL_BOTH		3



//
// INTERNAL MAP TYPES
//  used by play and refresh
//


// Forward of LineDefs, for Sectors.
struct line_s;

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef	struct sector_s
{
    fixed_t	floorheight;
    fixed_t	ceilingheight;
    short	floorpic;
    short	ceilingpic;
    short	lightlevel;
	byte	rgb[3];

    // if == validcount, already checked
    int		Validcount;

    // list of mobjs in sector
    mobj_t*	thinglist;

    int		linecount;
    struct line_s**	Lines;	// [linecount] size

	float 	flooroffx, flooroffy; 	// floor texture offset
	float	ceiloffx, ceiloffy;		// ceiling texture offset

	int		skyfix;					// Offset to ceiling height
									// rendering w/sky.
	float	reverb[NUM_REVERB_DATA];

    // mapblock bounding box for height changes
    int		blockbox[4];

	plane_t	planes[2];				// PLN_*

	degenmobj_t	soundorg;			// for any sounds played by the sector

// --- Don't change anything above ---

    short	special;
    short	tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int		soundtraversed;

    // thing that made a sound (or null)
    mobj_t*	soundtarget;

    // thinker_t for reversable actions
    void*	specialdata;

	int		origfloor, origceiling, origlight;
	byte	origrgb[3];
	xgsector_t *xg;

} sector_t;




//
// The SideDef.
//

typedef struct side_s
{
    // add this to the calculated texture column
    fixed_t	textureoffset;
    
    // add this to the calculated texture top
    fixed_t	rowoffset;

    // Texture indices.
    // We do not maintain names here. 
    short	toptexture;
    short	bottomtexture;
    short	midtexture;

    // Sector the SideDef is facing.
    sector_t*	sector;

// --- Don't change anything above ---

} side_t;


typedef struct line_s
{
    // Vertices, from v1 to v2.
    vertex_t*	v1;
    vertex_t*	v2;

    short	flags;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t*	frontsector;
    sector_t*	backsector;

    // Precalculated v2 - v1 for side checking.
    fixed_t	dx;
    fixed_t	dy;

    // To aid move clipping.
    slopetype_t	slopetype;

    // if == validcount, already checked
    int		Validcount;

    // Visual appearance: SideDefs.
    //  sidenum[1] will be -1 if one sided
    short	sidenum[2];	
	
	fixed_t bbox[4];

// --- Don't change anything above ---

    // Animation related.
    short	special;
    short	tag;

    // thinker_t for reversable actions
    void*	specialdata;	
	
	// Extended generalized lines.
	xgline_t *xg;
} line_t;


//
// BSP node.
//
typedef struct
{
    // Partition line.
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;

    // Bounding box for each child.
    fixed_t	bbox[2][4];

    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];
    
// --- Don't change anything above ---

} node_t;

// A combination of patch data and its lump number.
typedef struct
{
	int	width, height;
	int leftoffset, topoffset;
	int	lump;
} dpatch_t;

#endif
