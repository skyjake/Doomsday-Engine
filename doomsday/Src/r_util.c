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
 * r_util.c: Refresh Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

#define	SLOPERANGE	2048
#define	SLOPEBITS	11
#define	DBITS		(FRACBITS-SLOPEBITS)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern	int	tantoangle[SLOPERANGE+1];		// get from tables.c

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// R_PointOnSide
//	Returns side 0 (front) or 1 (back).
//===========================================================================
int	R_PointOnSide (fixed_t x, fixed_t y, node_t *node)
{
	fixed_t	dx,dy;
	fixed_t	left, right;

	if (!node->dx)
	{
		if (x <= node->x)
			return node->dy > 0;
		return node->dy < 0;
	}
	if (!node->dy)
	{
		if (y <= node->y)
			return node->dx < 0;
		return node->dx > 0;
	}

	dx = (x - node->x);
	dy = (y - node->y);

	// Try to quickly decide by looking at sign bits.
	if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
	{
		if  ( (node->dy ^ dx) & 0x80000000 )
			return 1;	// (left is negative)
		return 0;
	}

	left = FixedMul ( node->dy>>FRACBITS , dx );
	right = FixedMul ( dy , node->dx>>FRACBITS );

	if (right < left) return 0;		// front side
	return 1;			// back side
}

/*
//===========================================================================
// R_PointOnSegSide
//===========================================================================
int	R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line)
{
	fixed_t	lx, ly;
	fixed_t	ldx, ldy;
	fixed_t	dx,dy;
	fixed_t	left, right;

	lx = line->v1->x;
	ly = line->v1->y;

	ldx = line->v2->x - lx;
	ldy = line->v2->y - ly;

	if (!ldx)
	{
		if (x <= lx)
			return ldy > 0;
		return ldy < 0;
	}
	if (!ldy)
	{
		if (y <= ly)
			return ldx < 0;
		return ldx > 0;
	}

	dx = (x - lx);
	dy = (y - ly);

	// try to quickly decide by looking at sign bits
	if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
	{
		if  ( (ldy ^ dx) & 0x80000000 )
			return 1;	// (left is negative)
		return 0;
	}

	left = FixedMul ( ldy>>FRACBITS , dx );
	right = FixedMul ( dy , ldx>>FRACBITS );

	if (right < left)
		return 0;		// front side
	return 1;			// back side
}
*/

//===========================================================================
// R_SlopeDiv
//===========================================================================
int R_SlopeDiv (unsigned num, unsigned den)
{
	unsigned ans;
	if (den < 512)
		return SLOPERANGE;
	ans = (num<<3)/(den>>8);
	return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

//===========================================================================
// R_PointToAngle
// to get a global angle from cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system, then
// the y (<=x) is scaled and divided by x to get a tangent (slope) value
// which is looked up in the tantoangle[] table.  The +1 size is to handle
// the case when x==y without additional checking.
//===========================================================================
angle_t R_PointToAngle (fixed_t x, fixed_t y)
{
	x -= viewx;
	y -= viewy;
	if ( (!x) && (!y) )
		return 0;
	if (x>= 0)
	{	// x >=0
		if (y>= 0)
		{	// y>= 0
			if (x>y)
				return tantoangle[ R_SlopeDiv(y,x)];     // octant 0
			else
				return ANG90-1-tantoangle[ R_SlopeDiv(x,y)];  // octant 1
		}
		else
		{	// y<0
			y = -y;
			if (x>y)
				return -tantoangle[R_SlopeDiv(y,x)];  // octant 8
			else
				return ANG270+tantoangle[ R_SlopeDiv(x,y)];  // octant 7
		}
	}
	else
	{	// x<0
		x = -x;
		if (y>= 0)
		{	// y>= 0
			if (x>y)
				return ANG180-1-tantoangle[ R_SlopeDiv(y,x)]; // octant 3
			else
				return ANG90+ tantoangle[ R_SlopeDiv(x,y)];  // octant 2
		}
		else
		{	// y<0
			y = -y;
			if (x>y)
				return ANG180+tantoangle[ R_SlopeDiv(y,x)];  // octant 4
			else
				return ANG270-1-tantoangle[ R_SlopeDiv(x,y)];  // octant 5
		}
	}

	return 0;
}

//===========================================================================
// R_PointToAngle2
//===========================================================================
angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
	viewx = x1;
	viewy = y1;
	return R_PointToAngle (x2, y2);
}

//===========================================================================
// R_PointToDist
//===========================================================================
fixed_t	R_PointToDist (fixed_t x, fixed_t y)
{
	int		angle;
	fixed_t	dx, dy, temp;
	fixed_t	dist;

	dx = abs(x - viewx);
	dy = abs(y - viewy);

	if (dy>dx)
	{
		temp = dx;
		dx = dy;
		dy = temp;
	}

	angle = (tantoangle[ FixedDiv(dy,dx)>>DBITS ]+ANG90) >> ANGLETOFINESHIFT;

	dist = FixedDiv (dx, finesine[angle] );	// use as cosine

	return dist;
}

//===========================================================================
// R_PointInSubsector
//===========================================================================
subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t	*node;
	int		side, nodenum;

	if(!numnodes)				// single subsector is a special case
		return (subsector_t*) subsectors;

	nodenum = numnodes-1;

	while(!(nodenum & NF_SUBSECTOR))
	{
		node = NODE_PTR(nodenum);
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}
	return SUBSECTOR_PTR(nodenum & ~NF_SUBSECTOR);
}

//===========================================================================
// R_GetLineForSide
//===========================================================================
line_t *R_GetLineForSide(int sideNumber)
{
	side_t *side = SIDE_PTR(sideNumber);
	sector_t *sector = side->sector;
	int i;

	// All sides may not have a sector.
	if(!sector) return NULL;

	for(i = 0; i < sector->linecount; i++)
		if(sector->lines[i]->sidenum[0] == sideNumber 
			|| sector->lines[i]->sidenum[1] == sideNumber)
		{
			return sector->lines[i];
		}
	
	return NULL;
}

//===========================================================================
// R_IsPointInSector
//	Returns true if the point is inside the sector, according to the lines
//	that completely surround the sector. Uses the well-known algorithm 
//	described here: http://www.alienryderflex.com/polygon/
//===========================================================================
boolean R_IsPointInSector(fixed_t x, fixed_t y, sector_t *sector)
{
	int	i; 
	boolean isOdd = false;
	vertex_t *vi, *vj;

	for(i = 0; i < sector->linecount; i++) 
	{
		// Skip lines that aren't sector boundaries.
		if(sector->lines[i]->frontsector == sector
			&& sector->lines[i]->backsector == sector) continue;

		// It shouldn't matter whether the line faces inward or outward.
		vi = sector->lines[i]->v1;
		vj = sector->lines[i]->v2;
				
		if((vi->y < y && vj->y >= y) || (vj->y < y && vi->y >= y)) 
		{
			if(vi->x + FixedMul(FixedDiv(y - vi->y, vj->y - vi->y),
				vj->x - vi->x) < x) 
			{
				// Toggle oddness.
				isOdd = !isOdd;
			}
		}
	}
	
	// The point is inside if the number of crossed nodes is odd.
	return isOdd;
}

//===========================================================================
// R_GetSectorNumForDegen
//	Returns the index of the sector who owns the given degenmobj.
//===========================================================================
int R_GetSectorNumForDegen(void *degenmobj)
{
	int i;
	
	// Check all sectors; find where the sound is coming from.
	for(i = 0; i < numsectors; i++)
	{
		if(degenmobj == &SECTOR_PTR(i)->soundorg)
		{
			return i;
		}
	}
	return -1;
}

