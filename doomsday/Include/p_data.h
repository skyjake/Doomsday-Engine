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
 * Based on Hexen by Raven Software.
 */

/*
 * p_data.h: Playsim Data Structures, Macros and Constants
 */

#ifndef __DOOMSDAY_PLAY_DATA_H__
#define __DOOMSDAY_PLAY_DATA_H__

#include "dd_share.h"

// Ticcmd
#define TICCMD_SIZE		gx.ticcmd_size
#define TICCMD_IDX(i)	((i)*TICCMD_SIZE)

// Map data
#define VTXSIZE		gx.vertex_size
#define SEGSIZE		gx.seg_size
#define SECTSIZE	gx.sector_size
#define SUBSIZE		gx.subsector_size
#define NODESIZE	gx.node_size
#define LINESIZE	gx.line_size
#define SIDESIZE	gx.side_size
#define POSIZE		gx.polyobj_size

#define VTXIDX(i)	((i)*VTXSIZE)
#define SEGIDX(i)	((i)*SEGSIZE)
#define SECTIDX(i)	((i)*SECTSIZE)
#define SUBIDX(i)	((i)*SUBSIZE)
#define NODEIDX(i)	((i)*NODESIZE)
#define LINEIDX(i)	((i)*LINESIZE)
#define SIDEIDX(i)	((i)*SIDESIZE)
#define POIDX(i)	((i)*POSIZE)

#define GET_VERTEX_IDX(vtx)		( ((byte*)(vtx) - vertexes) / VTXSIZE )
#define GET_LINE_IDX(li)		( ((byte*)(li) - lines) / LINESIZE )
#define GET_SECTOR_IDX(sec)		( ((byte*)(sec) - sectors) / SECTSIZE )
#define GET_SUBSECTOR_IDX(sub)	( ((byte*)(sub) - subsectors) / SUBSIZE )
#define GET_POLYOBJ_IDX(po)		( ((byte*)(po) - polyobjs) / POSIZE )
#define GET_SEG_IDX(seg)		( ((byte*)(seg) - segs) / SEGSIZE )

#define VERTEX_PTR(i)		( (vertex_t*) (vertexes+VTXIDX(i)) )
#define SEG_PTR(i)			( (seg_t*) (segs+SEGIDX(i)) )
#define SECTOR_PTR(i)		( (sector_t*) (sectors+SECTIDX(i)) )
#define SUBSECTOR_PTR(i)	( (subsector_t*) (subsectors+SUBIDX(i)) )
#define NODE_PTR(i)			( (node_t*) (nodes+NODEIDX(i)) )
#define LINE_PTR(i)			( (line_t*) (lines+LINEIDX(i)) )
#define SIDE_PTR(i)			( (side_t*) (sides+SIDEIDX(i)) )
#define PO_PTR(i)			( (polyobj_t*) ((byte*)polyobjs+POIDX(i)) )

// Map line flags.
#define ML_BLOCKING			0x0001
#define ML_TWOSIDED			0x0004
#define	ML_DONTPEGTOP		0x0008
#define	ML_DONTPEGBOTTOM	0x0010
#define	ML_MAPPED			0x0100	// set if already drawn in automap

#define MAX_POLY_SIDES		64		// A subsector has at most this many edges.

// Node flags.
#define	NF_SUBSECTOR	0x8000

struct line_s;

typedef struct sector_s
{
	fixed_t		floorheight, ceilingheight;
	short		floorpic, ceilingpic;
	short		lightlevel;
	byte		rgb[3];
	int 		validcount; 			// if == validcount, already checked
	struct mobj_s *thinglist; 			// list of mobjs in sector
	int 		linecount;
	struct line_s **lines;				// [linecount] size
	float 		flooroffx, flooroffy; 	// floor texture offset
	float		ceiloffx, ceiloffy;		// ceiling texture offset
	int 		skyfix; 				// Offset to ceiling height
										// rendering w/sky.
	float		reverb[NUM_REVERB_DATA];
	int 		blockbox[4];			// mapblock bounding box for
										// height changes
	plane_t		planes[2];				// PLN_*
    degenmobj_t	soundorg;				// origin for any sounds
										// played by the sector
} sector_t;

typedef struct side_s
{
	fixed_t		textureoffset;		// add this to the calculated texture col
	fixed_t     rowoffset;			// add this to the calculated texture top
	short       toptexture, bottomtexture, midtexture;
	sector_t    *sector;
} side_t;

typedef struct line_s
{
	vertex_t	*v1;
	vertex_t	*v2;
	short		flags;
	sector_t	*frontsector;
	sector_t	*backsector;
	fixed_t		dx;
	fixed_t		dy;
	slopetype_t slopetype;
	int			validcount;		
	short		sidenum[2];
	fixed_t		bbox[4];
} line_t;

typedef struct polyobj_s
{
	int			numsegs;
	seg_t		**segs;
	int			validcount;
	degenmobj_t startSpot;
	angle_t		angle;
	vertex_t	*originalPts; 	// used as the base for the rotations
	vertex_t	*prevPts; 		// use to restore the old point values
	int			tag;			// reference tag assigned in HereticEd
	int			bbox[4];
	vertex_t	dest;
	int			speed;			// Destination XY and speed.
	angle_t		destAngle, angleSpeed;	// Destination angle and rotation speed.
} 
polyobj_t;

/*typedef struct subsector_s {
	struct subsector_base_s;
} subsector_t;*/

typedef struct
{
	fixed_t		x,y,dx,dy;		// partition line
	fixed_t     bbox[2][4];     // bounding box for each child
	unsigned short children[2]; // if NF_SUBSECTOR its a subsector
} node_t;

extern int		numvertexes;
extern byte		*vertexes;

extern int		numsegs;
extern byte		*segs;

extern int		numsectors;
extern byte		*sectors;

extern int		numsubsectors;
extern byte		*subsectors;

extern int		numnodes;
extern byte		*nodes;

extern int		numlines;
extern byte		*lines;

extern int		numsides;
extern byte		*sides;

extern fixed_t	mapgravity;				// Gravity for the current map.

void P_ValidateLevel(void);
void P_LoadBlockMap(int lump);
void P_LoadReject(int lump);

#endif 
