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

// Some more or less basic data types
// we depend on.
//#include "m_fixed.h"

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

//#define MAXDRAWSEGS		256





//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//  like some DOOM-alikes ("wt", "WebView") did.
//
/*typedef struct
{
    fixed_t	x;
    fixed_t	y;
    
} vertex_t;*/



// Forward of LineDefs, for Sectors.
struct line_s;

// Each sector has a degenmobj_t in its center
//  for sound origin purposes.
// I suppose this does not handle sound from
//  moving objects (doppler), because
//  position is prolly just buffered, not
//  updated.
/*typedef struct
{
    thinker_t		thinker;	// not used for anything
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

} degenmobj_t;*/

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

    int			linecount;
    struct line_s**	Lines;	// [linecount] size

	//int		flatoffx, flatoffy;		// floor texture offset
	float 	flooroffx, flooroffy; 	// floor texture offset
	float	ceiloffx, ceiloffy;		// ceiling texture offset

	int		skyfix;					// Offset to ceiling height rendering w/sky.
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



//
// Move clipping aid for LineDefs.
//
/*typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE

} slopetype_t;
*/

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
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//  indicating the visible walls that define
//  (all or some) sides of a convex BSP leaf.
//
/*typedef struct subsector_s
{
    sector_t*		sector;
    unsigned short	numLines;
    unsigned short	firstline;

   	void			*poly;			// Polyobj pointer (not used in Doom)
	
	// Sorted edge vertices for rendering floors and ceilings.
	char			numedgeverts;
	fvertex_t		*edgeverts;		// A list of edge vertices.
//	fvertex_t		*origedgeverts;	// Unmodified, accurate edge vertices.
//	fvertex_t		*diffverts;		// Unit modifiers.
	fvertex_t		bbox[2];		// Min and max points.
	fvertex_t		midpoint;		// Center of bounding box.
	byte			flags;

// --- Don't change anything above ---

} subsector_t;*/


//
// The LineSeg.
//
/*typedef struct
{
    vertex_t*	v1;
    vertex_t*	v2;

	float length;			// Accurate length of the segment.

    fixed_t	offset;

    side_t*	sidedef;
    line_t*	linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is NULL for one sided lines
    sector_t*	frontsector;
    sector_t*	backsector;

	byte flags;

	angle_t	angle;

// --- Don't change anything above ---

} seg_t;*/


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




/*// posts are runs of non masked source pixels
typedef struct
{
    byte		topdelta;	// -1 is the last post in a column
    byte		length; 	// length data bytes follows
} post_t;
*/
// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;



// PC direct to screen pointers
//B UNUSED - keep till detailshift in r_draw.c resolved
//extern byte*	destview;
//extern byte*	destscreen;





//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
//  precalculating 24bpp lightmap/colormap LUT.
//  from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef byte	lighttable_t;	




//
// ?
//
/*typedef struct drawseg_s
{
    seg_t*		curline;
    int			x1;
    int			x2;

    fixed_t		scale1;
    fixed_t		scale2;
    fixed_t		scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int			silhouette;

    // do not clip sprites above this
    fixed_t		bsilheight;

    // do not clip sprites below this
    fixed_t		tsilheight;
    
    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    short*		sprtopclip;		
    short*		sprbottomclip;	
    short*		maskedtexturecol;
    
} drawseg_t;

*/

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
/*typedef struct 
{ 
    short		width;		// bounding box size 
    short		height; 
    short		leftoffset;	// pixels to the left of origin 
    short		topoffset;	// pixels below the origin 
    int			columnofs[8];	// only [width] used
    // the [0] is &columnofs[width] 
} patch_t;
*/



// A combination of patch data and its lump number.
typedef struct
{
	int	width, height;
	int leftoffset, topoffset;
	int	lump;
} dpatch_t;


#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.3  2003/03/12 20:30:48  skyjake
// Some cleanup
//
// Revision 1.2  2003/02/27 23:14:31  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:18:41  skyjake
// Initial checkin
//
//-----------------------------------------------------------------------------
