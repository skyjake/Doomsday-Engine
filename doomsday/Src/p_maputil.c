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
 * p_maputil.c: Map Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)	( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying 
// things). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 things/lines in one block.

#define MAXLINKED			2048
#define DO_LINKS(it, end)	for(it = linkstore; it < end; it++)	\
								if(!func(*it, data)) return false;

// TYPES -------------------------------------------------------------------

typedef struct 
{
	mobj_t		*thing;
	fixed_t		box[4];
} 
linelinker_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// P_AccurateDistance
//==========================================================================
float P_AccurateDistance(fixed_t dx, fixed_t dy)
{
	float fx = FIX2FLT(dx), fy = FIX2FLT(dy);
	return (float)sqrt(fx*fx + fy*fy);
}

//===========================================================================
// P_ApproxDistance
//	Gives an estimation of distance (not exact).
//===========================================================================
fixed_t P_ApproxDistance(fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	return dx + dy - ((dx < dy? dx : dy) >> 1);
}

//===========================================================================
// P_ApproxDistance3
//	Gives an estimation of 3D distance (not exact).
//	The Z axis aspect ratio is corrected.
//===========================================================================
fixed_t P_ApproxDistance3(fixed_t dx, fixed_t dy, fixed_t dz)
{
	return P_ApproxDistance(P_ApproxDistance(dx, dy), 
		FixedMul(dz, 1.2f*FRACUNIT));
}

//===========================================================================
// P_LineUnitVector
//	Returns a two-component float unit vector parallel to the line.
//===========================================================================
void P_LineUnitVector(line_t *line, float *unitvec)
{
	float dx = FIX2FLT(line->dx), dy = FIX2FLT(line->dy);
	float len = M_ApproxDistancef(dx, dy);

	if(len)
	{
		unitvec[VX] = dx/len;
		unitvec[VY] = dy/len;
	}
	else
	{
		unitvec[VX] = unitvec[VY] = 0;
	}
}

//===========================================================================
// P_MobjPointDistancef
//	Either end or fixpoint must be specified. The distance is measured 
//	(approximately) in 3D. Start must always be specified.
//===========================================================================
float P_MobjPointDistancef(mobj_t *start, mobj_t *end, float *fixpoint)
{
	if(!start) return 0;
	if(end)
	{
		// Start -> end.
		return FIX2FLT(P_ApproxDistance(end->z - start->z, P_ApproxDistance
			(end->x - start->x, end->y - start->y)));
	}
	if(fixpoint)
	{
		float sp[3] = { 
			FIX2FLT(start->x), 
			FIX2FLT(start->y), 
			FIX2FLT(start->z) 
		};
		return M_ApproxDistancef(fixpoint[VZ] - sp[VZ], M_ApproxDistancef
			(fixpoint[VX] - sp[VX], fixpoint[VY] - sp[VY]));
	}
	return 0;
}

//==========================================================================
// P_FloatPointOnLineSide
//	Determines on which side of dline the point is. Returns true if the
//	point is on the line or on the right side.
//==========================================================================
#ifdef WIN32
#pragma optimize("g", off)
#endif
int P_FloatPointOnLineSide(fvertex_t *pnt, fdivline_t *dline)
{
/*
            (AY-CY)(BX-AX)-(AX-CX)(BY-AY)
        s = -----------------------------
                        L**2

    If s<0      C is left of AB (you can just check the numerator)
    If s>0      C is right of AB
    If s=0      C is on AB
*/
	// We'll return false if the point c is on the left side.
	return ((dline->y - pnt->y)*dline->dx 
		- (dline->x - pnt->x)*dline->dy >= 0);
}

//==========================================================================
// P_FloatInterceptVertex
//	Lines start, end and fdiv must intersect.
//==========================================================================
float P_FloatInterceptVertex
	(fvertex_t *start, fvertex_t *end, fdivline_t *fdiv, fvertex_t *inter)
{
	float ax = start->x, ay = start->y, bx = end->x, by = end->y;
	float cx = fdiv->x, cy = fdiv->y, dx = cx+fdiv->dx, dy = cy+fdiv->dy;

	/*
	        (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
        r = -----------------------------  (eqn 1)
            (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
	*/

	float r = ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy))
		/ ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
	/*
	    XI=XA+r(XB-XA)
        YI=YA+r(YB-YA)
	*/
	inter->x = ax + r*(bx-ax);
	inter->y = ay + r*(by-ay);
	return r;
}
#ifdef WIN32
#pragma optimize("", on)
#endif

//===========================================================================
// P_SectorBoundingBox
//	(0,1) = top left; (2,3) = bottom right
//	Assumes sectors are always closed.
//===========================================================================
void P_SectorBoundingBox(sector_t *sec, float *bbox)
{
	float	x, y;
	int		i;
	line_t	*li;
	
	if(!sec->linecount) return;
	bbox[BLEFT] = bbox[BRIGHT] = sec->lines[0]->v1->x >> FRACBITS;
	bbox[BTOP] = bbox[BBOTTOM] = sec->lines[0]->v1->y >> FRACBITS;
	for(i = 1; i < sec->linecount; i++)
	{
		li = sec->lines[i];
		x = li->v1->x >> FRACBITS;
		y = li->v1->y >> FRACBITS;
		if(x < bbox[BLEFT]) bbox[BLEFT] = x;
		if(x > bbox[BRIGHT]) bbox[BRIGHT] = x;
		if(y < bbox[BTOP]) bbox[BTOP] = y;
		if(y > bbox[BBOTTOM]) bbox[BBOTTOM] = y;
	}
}


/*
==================
=
= P_PointOnLineSide
=
= Reformatted
= Returns 0 or 1
==================
*/

int P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line)
{
	return !line->dx ? x <= line->v1->x ? line->dy > 0 : line->dy < 0 : 
		!line->dy ? y <= line->v1->y ? line->dx < 0 : line->dx > 0 :
		FixedMul(y-line->v1->y, line->dx>>FRACBITS) >=
		FixedMul(line->dy>>FRACBITS, x-line->v1->x);
}


/*
=================
=
= P_BoxOnLineSide
=
= Considers the line to be infinite
= Reformatted
= Returns side 0 or 1, -1 if box crosses the line
=================
*/

int P_BoxOnLineSide (fixed_t *tmbox, line_t *ld)
{
	switch (ld->slopetype)
	{
		int p;
	default: // shut up compiler warnings -- killough
	case ST_HORIZONTAL:
		return
			(tmbox[BOXBOTTOM] > ld->v1->y) == (p = tmbox[BOXTOP] > ld->v1->y) ?
			p ^ (ld->dx < 0) : -1;
	case ST_VERTICAL:
		return
			(tmbox[BOXLEFT] < ld->v1->x) == (p = tmbox[BOXRIGHT] < ld->v1->x) ?
			p ^ (ld->dy < 0) : -1;
	case ST_POSITIVE:
		return
			P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld) ==
			(p = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXTOP], ld)) ? p : -1;
	case ST_NEGATIVE:
		return
			(P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld)) ==
			(p = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], ld)) ? p : -1;
	}
}

/*
==================
=
= P_PointOnDivlineSide
=
= Reformatted
= Returns 0 or 1
==================
*/

int P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line)
{
	return !line->dx ? x <= line->x ? line->dy > 0 : line->dy < 0 :
		!line->dy ? y <= line->y ? line->dx < 0 : line->dx > 0 :
		(line->dy^line->dx^(x -= line->x)^(y -= line->y)) < 0 ? (line->dy^x) < 0 :
		FixedMul(y>>8, line->dx>>8) >= FixedMul(line->dy>>8, x>>8);
}



/*
==============
=
= P_MakeDivline
=
==============
*/

void P_MakeDivline (line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->dx;
	dl->dy = li->dy;
}


/*
===============
=
= P_InterceptVector
=
= Returns the fractional intercept point along the first divline
=
= Reformatted
= This is only called by the addthings and addlines traversers
===============
*/

fixed_t P_InterceptVector (divline_t *v2, divline_t *v1)
{
	fixed_t den = FixedMul(v1->dy>>8, v2->dx) - FixedMul(v1->dx>>8, v2->dy);
	return den ? FixedDiv((FixedMul((v1->x-v2->x)>>8, v1->dy) +
		FixedMul((v2->y-v1->y)>>8, v1->dx)), den) : 0;
}

/*
==================
=
= P_LineOpening
=
= Sets opentop and openbottom to the window through a two sided line
= OPTIMIZE: keep this precalculated
==================
*/

fixed_t opentop, openbottom, openrange;
fixed_t	lowfloor;

void P_LineOpening (line_t *linedef)
{
	sector_t	*front, *back;
	
	if(!linedef->backsector)
	{	// single sided line
		openrange = 0;
		return;
	}
	 
	front = linedef->frontsector;
	back = linedef->backsector;
	
	if (front->ceilingheight < back->ceilingheight)
		opentop = front->ceilingheight;
	else
		opentop = back->ceilingheight;
	if (front->floorheight > back->floorheight)
	{
		openbottom = front->floorheight;
		lowfloor = back->floorheight;
	}
	else
	{
		openbottom = back->floorheight;
		lowfloor = front->floorheight;
	}
	
	openrange = opentop - openbottom;
}

/*
===============================================================================

						THING POSITION SETTING

===============================================================================
*/

//===========================================================================
// P_GetBlockRootIdx
//	The index is not checked.
//===========================================================================
mobj_t *P_GetBlockRootIdx(int index)
{
	return (mobj_t*) (blockrings + index);
}

//==========================================================================
// P_GetBlockRoot
//	Returns a pointer to the root linkmobj_t of the given mobj. If such
//	a block does not exist, NULL is returned. This routine is exported
//	for use in Games.
//==========================================================================
mobj_t *P_GetBlockRoot(int blockx, int blocky)
{
	// We must be in the block map range.
	if(blockx < 0 || blocky < 0 
		|| blockx >= bmapwidth || blocky >= bmapheight) 
	{
		return NULL;
	}
	return (mobj_t*) (blockrings + (blocky*bmapwidth + blockx));
}

//===========================================================================
// P_GetBlockRootXY
//	Same as P_GetBlockRoot, but takes world coordinates as parameters.
//===========================================================================
mobj_t *P_GetBlockRootXY(int x, int y)
{
	return P_GetBlockRoot((x - bmaporgx) >> MAPBLOCKSHIFT,
		(y - bmaporgy) >> MAPBLOCKSHIFT);
}

//===========================================================================
// P_UnlinkFromSector
//	Only call if it is certain the thing is linked to a sector!
//===========================================================================
void P_UnlinkFromSector(mobj_t *thing)
{
	// Two links to update: 
	// 1) The link to us from the previous node (sprev, always set) will
	//    be modified to point to the node following us.
	// 2) If there is a node following us, set its sprev pointer to point
	//    to the pointer that points back to it (our sprev, just modified).

	if((*thing->sprev = thing->snext)) 
		thing->snext->sprev = thing->sprev;

	// Not linked any more.
	thing->snext = NULL;
	thing->sprev = NULL;
}

//===========================================================================
// P_UnlinkFromBlock
//	Only call if it is certain that the thing is linked to a block!
//===========================================================================
void P_UnlinkFromBlock(mobj_t *thing)
{
	(thing->bnext->bprev = thing->bprev)->bnext = thing->bnext;
	// Not linked any more.
	thing->bnext = thing->bprev = NULL;
}

//===========================================================================
// P_UnlinkFromLines
//	Unlinks the thing from all the lines it's been linked to. Can be called
//	without checking that the list does indeed contain lines.
//===========================================================================
void P_UnlinkFromLines(mobj_t *thing)
{
	linknode_t *tn = thingnodes.nodes;
	nodeindex_t nix;

	// Try unlinking from lines.
	if(!thing->lineroot) return; // A zero index means it's not linked.
	
	// Unlink from each line.
	for(nix = tn[thing->lineroot].next; nix != thing->lineroot; 
		nix = tn[nix].next)
	{
		// Data is the linenode index that corresponds this thing.
		NP_Unlink(&linenodes, tn[nix].data);
		// We don't need these nodes any more, mark them as unused.
		// Dismissing is a macro.
		NP_Dismiss(linenodes, tn[nix].data);
		NP_Dismiss(thingnodes, nix);
	}

	// The thing no longer has a line ring.
	NP_Dismiss(thingnodes, thing->lineroot);
	thing->lineroot = 0;
}

//==========================================================================
// P_UnlinkThing
//	Unlinks a thing from everything it has been linked to.
//==========================================================================
void P_UnlinkThing(mobj_t *thing)
{
	if(IS_SECTOR_LINKED(thing)) P_UnlinkFromSector(thing);
	if(IS_BLOCK_LINKED(thing)) P_UnlinkFromBlock(thing);
	P_UnlinkFromLines(thing);
}

//===========================================================================
// PIT_LinkToLines
//	The given line might cross the thing. If necessary, link the mobj 
//	into the line's ring.
//===========================================================================
boolean PIT_LinkToLines(line_t *ld, void *parm)
{
	linelinker_data_t *data = parm;
	fixed_t bbox[4];
	nodeindex_t nix;

	// Setup the bounding box for the line.
	ORDER(ld->v1->x, ld->v2->x, bbox[BOXLEFT], bbox[BOXRIGHT]);
	ORDER(ld->v1->y, ld->v2->y, bbox[BOXBOTTOM], bbox[BOXTOP]);

    if(data->box[BOXRIGHT] <= bbox[BOXLEFT]
		|| data->box[BOXLEFT] >= bbox[BOXRIGHT]
		|| data->box[BOXTOP] <= bbox[BOXBOTTOM]
		|| data->box[BOXBOTTOM] >= bbox[BOXTOP])
		// Bounding boxes do not overlap.
		return true;

    if(P_BoxOnLineSide(data->box, ld) != -1)
		// Line does not cross the thing's bounding box.
		return true; 
  
	// One sided lines will not be linked to because a mobj 
	// can't legally cross one.
    if(!ld->backsector) return true;	

	// No redundant nodes will be creates since this routine is 
	// called only once for each line.

	// Add a node to the thing's ring.
	NP_Link(&thingnodes, nix = NP_New(&thingnodes, ld), 
		data->thing->lineroot);

	// Add a node to the line's ring. Also store the linenode's index
	// into the thingring's node, so unlinking is easy.
	NP_Link(&linenodes, 
		thingnodes.nodes[nix].data = NP_New(&linenodes, data->thing), 
		linelinks[ GET_LINE_IDX(ld) ]);

    return true;
}

//===========================================================================
// P_LinkToLines
//	The thing must be currently unlinked.
//===========================================================================
void P_LinkToLines(mobj_t *thing)
{
	int xl, yl, xh, yh, bx, by;
	linelinker_data_t data;

	// Get a new root node.
	thing->lineroot = NP_New(&thingnodes, NP_ROOT_NODE);

	// Set up a line iterator for doing the linking.
	data.thing = thing;
    data.box[BOXTOP] = thing->y + thing->radius;
    data.box[BOXBOTTOM] = thing->y - thing->radius;
    data.box[BOXRIGHT] = thing->x + thing->radius;
    data.box[BOXLEFT] = thing->x - thing->radius;
	
	xl = (data.box[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
	xh = (data.box[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
	yl = (data.box[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
	yh = (data.box[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;
			
    validcount++;
	for(bx = xl; bx <= xh; bx++)
		for(by = yl; by <= yh; by++)
			P_BlockLinesIterator(bx, by, PIT_LinkToLines, &data);
}


//==========================================================================
// P_LinkThing
//	Links a thing into both a block and a subsector based on it's (x,y).
//	Sets thing->subsector properly. Calling with flags==0 only updates
//	the subsector pointer. Can be called without unlinking first.
//==========================================================================
void P_LinkThing(mobj_t *thing, byte flags)
{
	sector_t	*sec;
	mobj_t		*root;
	
	// Link into the sector.
	sec = (thing->subsector = R_PointInSubsector(thing->x, thing->y))->sector;
	if(flags & DDLINK_SECTOR)
	{
		// Unlink from the current sector, if any.
		if(thing->sprev) P_UnlinkFromSector(thing);

		// Link the new thing to the head of the list.
		// Prev pointers point to the pointer that points back to us.
		// (Which practically disallows traversing the list backwards.)

		if((thing->snext = sec->thinglist))
			thing->snext->sprev = &thing->snext;

		*(thing->sprev = &sec->thinglist) = thing;
	}
	
	// Link into blockmap.
	if(flags & DDLINK_BLOCKMAP)
	{
		// Unlink from the old block, if any.
		if(thing->bnext) P_UnlinkFromBlock(thing);

		// Link into the block we're currently in.
		root = P_GetBlockRootXY(thing->x, thing->y);
		if(root) 		
		{
			// Only link if we're inside the blockmap.
			thing->bprev = root;
			thing->bnext = root->bnext;
			thing->bnext->bprev = root->bnext = thing;
		}
	}

	// Link into lines.
	if(!(flags & DDLINK_NOLINE))
	{
		// Unlink from any existing lines.
		P_UnlinkFromLines(thing);

		// Link to all contacted lines.
		P_LinkToLines(thing);
	}
}

//===========================================================================
// P_BlockThingsIterator
//	'func' can do whatever it pleases to the mobjs.
//===========================================================================
boolean P_BlockThingsIterator
	(int x, int y, boolean(*func)(mobj_t*,void*), void *data)
{
	mobj_t *mobj, *root = P_GetBlockRoot(x, y);
	void *linkstore[MAXLINKED], **end = linkstore, **it;
	
	if(!root) return true; // Not inside the blockmap.
	// Gather all the things in the block into the list.
	for(mobj = root->bnext; mobj != root; mobj = mobj->bnext) 
		*end++ = mobj;
	DO_LINKS(it, end);
	return true;
}

//===========================================================================
// P_ThingLinesIterator
//	The callback function will be called once for each line that crosses
//	trough the object. This means all the lines will be two-sided.
//===========================================================================
boolean P_ThingLinesIterator
	(mobj_t *thing, boolean (*func)(line_t*,void*), void *data)
{
	nodeindex_t nix;
	linknode_t *tn = thingnodes.nodes;
	void *linkstore[MAXLINKED], **end = linkstore, **it;

	if(!thing->lineroot) return true; // No lines to process.
	for(nix = tn[thing->lineroot].next; nix != thing->lineroot; 
		nix = tn[nix].next) *end++ = tn[nix].ptr;
	DO_LINKS(it, end);
	return true;
}

//===========================================================================
// P_ThingSectorsIterator
//	Increment validcount before calling this routine. The callback function
//	will be called once for each sector the thing is touching 
//	(totally or partly inside). This is not a 3D check; the thing may 
//	actually reside above or under the sector.
//===========================================================================
boolean P_ThingSectorsIterator
	(mobj_t *thing, boolean (*func)(sector_t*,void*), void *data)
{
	void *linkstore[MAXLINKED], **end = linkstore, **it;
	nodeindex_t nix;
	linknode_t *tn = thingnodes.nodes;
	line_t *ld;
	sector_t *sec;

	// Always process the thing's own sector first.
	*end++ = sec = thing->subsector->sector;
	sec->validcount = validcount;

	// Any good lines around here?
	if(thing->lineroot) 
	{
		for(nix = tn[thing->lineroot].next; nix != thing->lineroot; 
			nix = tn[nix].next)
		{
			ld = (line_t*) tn[nix].ptr;
			// All these lines are two-sided. Try front side.
			sec = ld->frontsector;
			if(sec->validcount != validcount)
			{
				*end++ = sec;
				sec->validcount = validcount;
			}
			// And then the back side.
			sec = ld->backsector;
			if(sec->validcount != validcount)
			{
				*end++ = sec;
				sec->validcount = validcount;
			}
		}
	}
	DO_LINKS(it, end);
	return true;
}

//===========================================================================
// P_LineThingsIterator
//===========================================================================
boolean P_LineThingsIterator
	(line_t *line, boolean (*func)(mobj_t*,void*), void *data)
{
	void *linkstore[MAXLINKED], **end = linkstore, **it;
	nodeindex_t root = linelinks[ GET_LINE_IDX(line) ], nix;
	linknode_t *ln = linenodes.nodes;
	
	for(nix = ln[root].next; nix != root; nix = ln[nix].next)
		*end++ = ln[nix].ptr;
	DO_LINKS(it, end);
	return true;
}

//===========================================================================
// P_SectorTouchingThingsIterator
//	Increment validcount before using this. 'func' is called for each mobj
//	that is (even partly) inside the sector. This is not a 3D test, the 
//	mobjs may actually be above or under the sector.
//
//	(Lovely name; actually this is a combination of SectorThings and 
//	a bunch of LineThings iterations.)
//===========================================================================
boolean P_SectorTouchingThingsIterator
	(sector_t *sector, boolean (*func)(mobj_t*,void*), void *data)
{
	void *linkstore[MAXLINKED], **end = linkstore, **it;
	mobj_t *mo;
	line_t *li;
	nodeindex_t root, nix;
	linknode_t *ln = linenodes.nodes;
	int i;
		
	// First process the things that obviously are in the sector.
	for(mo = sector->thinglist; mo; mo = mo->snext)
	{
		if(mo->validcount == validcount) continue;
		mo->validcount = validcount;
		*end++ = mo;
	}
	// Then check the sector's lines.
	for(i = 0; i < sector->linecount; i++)
	{
		li = sector->lines[i];
		// Iterate all mobjs on the line.
		root = linelinks[ GET_LINE_IDX(li) ];
		for(nix = ln[root].next; nix != root; nix = ln[nix].next)
		{
			mo = (mobj_t*) ln[nix].ptr;
			if(mo->validcount == validcount) continue;
			mo->validcount = validcount;
			*end++ = mo;
		}
	}
	DO_LINKS(it, end);	
	return true;
}

/*
===============================================================================

					INTERCEPT ROUTINES

===============================================================================
*/

//intercept_t		intercepts[MAXINTERCEPTS], *intercept_p;

divline_t 	trace;
boolean 	earlyout;
int			ptflags;

/*
==================
=
= PIT_AddLineIntercepts
=
= Looks for lines in the given block that intercept the given trace
= to add to the intercepts list
= A line is crossed if its endpoints are on opposite sides of the trace
= Returns true if earlyout and a solid line hit
==================
*/

boolean PIT_AddLineIntercepts (line_t *ld, void *data)
{
	int			s1, s2;
	fixed_t		frac;
	divline_t	dl;
	
// avoid precision problems with two routines
	if ( trace.dx > FRACUNIT*16 || trace.dy > FRACUNIT*16
	|| trace.dx < -FRACUNIT*16 || trace.dy < -FRACUNIT*16)
	{
		s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
		s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
	}
	else
	{
		s1 = P_PointOnLineSide (trace.x, trace.y, ld);
		s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
	}
	if (s1 == s2)
		return true;		// line isn't crossed

//
// hit the line
//
	P_MakeDivline (ld, &dl);
	frac = P_InterceptVector (&trace, &dl);
	if (frac < 0)
		return true;		// behind source
	
// try to early out the check
	if (earlyout && frac < FRACUNIT && !ld->backsector)
		return false;	// stop checking
	
/*	intercept_p->frac = frac;
	intercept_p->isaline = true;
	intercept_p->d.line = ld;
	intercept_p++;*/
	P_AddIntercept(frac, true, ld);

	return true;		// continue
}



/*
==================
=
= PIT_AddThingIntercepts
=
==================
*/

boolean PIT_AddThingIntercepts(mobj_t *thing, void *data)
{
	fixed_t		x1,y1, x2,y2;
	int			s1, s2;
	boolean		tracepositive;
	divline_t	dl;
	fixed_t		frac;
	
	if(thing->dplayer && thing->dplayer->flags & DDPF_CAMERA)
		return true; // $democam: ssshh, keep going, we're not here...

	tracepositive = (trace.dx ^ trace.dy)>0;
		
	// check a corner to corner crossection for hit
	
	if (tracepositive)
	{
		x1 = thing->x - thing->radius;
		y1 = thing->y + thing->radius;
		
		x2 = thing->x + thing->radius;
		y2 = thing->y - thing->radius;			
	}
	else
	{
		x1 = thing->x - thing->radius;
		y1 = thing->y - thing->radius;
		
		x2 = thing->x + thing->radius;
		y2 = thing->y + thing->radius;			
	}
	s1 = P_PointOnDivlineSide (x1, y1, &trace);
	s2 = P_PointOnDivlineSide (x2, y2, &trace);
	if (s1 == s2)
		return true;	// line isn't crossed
	
	dl.x = x1;
	dl.y = y1;
	dl.dx = x2-x1;
	dl.dy = y2-y1;
	frac = P_InterceptVector (&trace, &dl);
	if (frac < 0)
		return true;		// behind source
/*	intercept_p->frac = frac;
	intercept_p->isaline = false;
	intercept_p->d.thing = thing;
	intercept_p++;*/
	P_AddIntercept(frac, false, thing);

	return true;			// keep going
}

/*
==================
=
= P_PathTraverse
=
= Traces a line from x1,y1 to x2,y2, calling the traverser function for each
= Returns true if the traverser function returns true for all lines
==================
*/

boolean P_PathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
	int flags, boolean (*trav) (intercept_t *))
{
	fixed_t	xt1,yt1,xt2,yt2;
	fixed_t	xstep,ystep;
	fixed_t	partial;
	fixed_t	xintercept, yintercept;
	int		mapx, mapy, mapxstep, mapystep;
	int		count;
		
	earlyout = flags & PT_EARLYOUT;
		
	validcount++;
	//intercept_p = intercepts;
	P_ClearIntercepts();
	
	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT;				// don't side exactly on a line
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT;				// don't side exactly on a line
	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

	x1 -= bmaporgx;
	y1 -= bmaporgy;
	xt1 = x1>>MAPBLOCKSHIFT;
	yt1 = y1>>MAPBLOCKSHIFT;

	x2 -= bmaporgx;
	y2 -= bmaporgy;
	xt2 = x2>>MAPBLOCKSHIFT;
	yt2 = y2>>MAPBLOCKSHIFT;

	if (xt2 > xt1)
	{
		mapxstep = 1;
		partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else if (xt2 < xt1)
	{
		mapxstep = -1;
		partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else
	{
		mapxstep = 0;
		partial = FRACUNIT;
		ystep = 256*FRACUNIT;
	}	
	yintercept = (y1>>MAPBTOFRAC) + FixedMul (partial, ystep);

	
	if (yt2 > yt1)
	{
		mapystep = 1;
		partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else if (yt2 < yt1)
	{
		mapystep = -1;
		partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else
	{
		mapystep = 0;
		partial = FRACUNIT;
		xstep = 256*FRACUNIT;
	}	
	xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);

	
//
// step through map blocks
// Count is present to prevent a round off error from skipping the break
	mapx = xt1;
	mapy = yt1;
	
	for (count = 0 ; count < 64 ; count++)
	{
		if (flags & PT_ADDLINES)
		{
			if (!P_BlockLinesIterator (mapx, mapy, PIT_AddLineIntercepts, 0))
				return false;	// early out
		}
		if (flags & PT_ADDTHINGS)
		{
			if (!P_BlockThingsIterator (mapx, mapy, PIT_AddThingIntercepts, 0))
				return false;	// early out
		}
		
		if (mapx == xt2 && mapy == yt2)
			break;
			
		if ( (yintercept >> FRACBITS) == mapy)
		{
			yintercept += ystep;
			mapx += mapxstep;
		}
		else if ( (xintercept >> FRACBITS) == mapx)
		{
			xintercept += xstep;
			mapy += mapystep;
		}
		
	}


//
// go through the sorted list
//
	return P_TraverseIntercepts ( trav, FRACUNIT );
}
