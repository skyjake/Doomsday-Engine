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
 * p_polyob.c: Polygon Objects
 *
 * Polyobj translation and rotation.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

#define MAXRADIUS	32*FRACUNIT

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void UpdateSegBBox(seg_t *seg);
static void RotatePt(int an, fixed_t *x, fixed_t *y, 
					 fixed_t startSpotX, fixed_t startSpotY);
static boolean CheckMobjBlocking(seg_t *seg, polyobj_t *po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Called when the polyobj hits a mobj.
void (*po_callback)(mobj_t *mobj, void *seg, void *po);

byte			*polyobjs;		// list of all poly-objects on the level
int				po_NumPolyobjs;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// PO_SetCallback
//	The po_callback is called when a polyobj hits a mobj.
//==========================================================================
void PO_SetCallback(void (*func)(mobj_t*, void*, void*))
{
	po_callback = func;
}

//==========================================================================
// GetPolyobj
//==========================================================================
polyobj_t *GetPolyobj(int polyNum)
{
	int i;

	for(i = 0; i < po_NumPolyobjs; i++)
	{
		if(PO_PTR(i)->tag == polyNum)
		{
			return PO_PTR(i); //&polyobjs[i];
		}
	}
	return NULL;
}

//==========================================================================
// UpdateSegBBox
//==========================================================================
void UpdateSegBBox(seg_t *seg)
{
	line_t *line;

	line = seg->linedef;

	if(seg->v1->x < seg->v2->x)
	{
		line->bbox[BOXLEFT] = seg->v1->x;
		line->bbox[BOXRIGHT] = seg->v2->x;
	}
	else
	{
		line->bbox[BOXLEFT] = seg->v2->x;
		line->bbox[BOXRIGHT] = seg->v1->x;
	}
	if(seg->v1->y < seg->v2->y)
	{
		line->bbox[BOXBOTTOM] = seg->v1->y;
		line->bbox[BOXTOP] = seg->v2->y;
	}
	else
	{
		line->bbox[BOXBOTTOM] = seg->v2->y;
		line->bbox[BOXTOP] = seg->v1->y;
	}

	// Update the line's slopetype
	line->dx = line->v2->x - line->v1->x;
	line->dy = line->v2->y - line->v1->y;
	if(!line->dx)
	{
		line->slopetype = ST_VERTICAL;
	}
	else if(!line->dy)
	{
		line->slopetype = ST_HORIZONTAL;
	}
	else
	{
		if(FixedDiv(line->dy, line->dx) > 0)
		{
			line->slopetype = ST_POSITIVE;
		}
		else
		{
			line->slopetype = ST_NEGATIVE;
		}
	}
}

//==========================================================================
// PO_MovePolyobj
//==========================================================================
boolean PO_MovePolyobj(int num, int x, int y)
{
	int count;
	seg_t **segList;
	seg_t **veryTempSeg;
	polyobj_t *po;
	vertex_t *prevPts;
	boolean blocked;

	if(num & 0x80000000)
	{
		po = PO_PTR(num & 0x7fffffff);
	}
	else if(!(po = GetPolyobj(num)))
	{
		Con_Error("PO_MovePolyobj:  Invalid polyobj number: %d\n", num);
	}

	PO_UnLinkPolyobj(po);

	segList = po->segs;
	prevPts = po->prevPts;
	blocked = false;

	validcount++;
	for(count = po->numsegs; count; count--, segList++, prevPts++)
	{
		if((*segList)->linedef->validcount != validcount)
		{
			(*segList)->linedef->bbox[BOXTOP] += y;
			(*segList)->linedef->bbox[BOXBOTTOM] += y;
			(*segList)->linedef->bbox[BOXLEFT] += x;
			(*segList)->linedef->bbox[BOXRIGHT] += x;
			(*segList)->linedef->validcount = validcount;
		}
		for(veryTempSeg = po->segs; veryTempSeg != segList;
			veryTempSeg++)
		{
			if((*veryTempSeg)->v1 == (*segList)->v1)
			{
				break;
			}
		}
		if(veryTempSeg == segList)
		{
			(*segList)->v1->x += x;
			(*segList)->v1->y += y;
		}
		(*prevPts).x += x; // previous points are unique for each seg
		(*prevPts).y += y;
	}
	segList = po->segs;
	for(count = po->numsegs; count; count--, segList++)
	{
		if(CheckMobjBlocking(*segList, po))
		{
			blocked = true;
		}
	}
	if(blocked)
	{
		count = po->numsegs;
		segList = po->segs;
		prevPts = po->prevPts;
		validcount++;
		while(count--)
		{
			if((*segList)->linedef->validcount != validcount)
			{
				(*segList)->linedef->bbox[BOXTOP] -= y;
				(*segList)->linedef->bbox[BOXBOTTOM] -= y;
				(*segList)->linedef->bbox[BOXLEFT] -= x;
				(*segList)->linedef->bbox[BOXRIGHT] -= x;
				(*segList)->linedef->validcount = validcount;
			}
			for(veryTempSeg = po->segs; veryTempSeg != segList;
				veryTempSeg++)
			{
				if((*veryTempSeg)->v1 == (*segList)->v1)
				{
					break;
				}
			}
			if(veryTempSeg == segList)
			{
				(*segList)->v1->x -= x;
				(*segList)->v1->y -= y;
			}
			(*prevPts).x -= x;
			(*prevPts).y -= y;
			segList++;
			prevPts++;
		}
		PO_LinkPolyobj(po);
		return false;
	}
	po->startSpot.x += x;
	po->startSpot.y += y;
	PO_LinkPolyobj(po);
	return true;
}

//==========================================================================
// RotatePt
//==========================================================================
static void RotatePt(int an, fixed_t *x, fixed_t *y, 
					 fixed_t startSpotX, fixed_t startSpotY)
{
	fixed_t trx, try;
	fixed_t gxt, gyt;

	trx = *x;
	try = *y;

	gxt = FixedMul(trx, finecosine[an]);
	gyt = FixedMul(try, finesine[an]);
	*x = (gxt-gyt)+startSpotX;

	gxt = FixedMul(trx, finesine[an]);
	gyt = FixedMul(try, finecosine[an]);
	*y = (gyt+gxt)+startSpotY;
}

//==========================================================================
// PO_RotatePolyobj
//==========================================================================
boolean PO_RotatePolyobj(int num, angle_t angle)
{
	int count;
	seg_t **segList;
	vertex_t *originalPts;
	vertex_t *prevPts;
	int an;
	polyobj_t *po;
	boolean blocked;

	if(num & 0x80000000)
	{
		po = PO_PTR(num & 0x7fffffff);
	}
	else if(!(po = GetPolyobj(num)))
	{
		Con_Error("PO_RotatePolyobj: Invalid polyobj number: %d\n", num);
	}
	an = (po->angle + angle) >> ANGLETOFINESHIFT;

	PO_UnLinkPolyobj(po);

	segList = po->segs;
	originalPts = po->originalPts;
	prevPts = po->prevPts;

	for(count = po->numsegs; count; count--, segList++, originalPts++,
		prevPts++)
	{
		prevPts->x = (*segList)->v1->x;
		prevPts->y = (*segList)->v1->y;
		(*segList)->v1->x = originalPts->x;
		(*segList)->v1->y = originalPts->y;
		RotatePt(an, &(*segList)->v1->x, &(*segList)->v1->y, po->startSpot.x,
			po->startSpot.y);
	}
	segList = po->segs;
	blocked = false;
	validcount++;
	for(count = po->numsegs; count; count--, segList++)
	{
		if(CheckMobjBlocking(*segList, po))
		{
			blocked = true;
		}
		if((*segList)->linedef->validcount != validcount)
		{
			UpdateSegBBox(*segList);
			(*segList)->linedef->validcount = validcount;
		}
		(*segList)->angle += angle;
	}
	if(blocked)
	{
		segList = po->segs;
		prevPts = po->prevPts;
		for(count = po->numsegs; count; count--, segList++, prevPts++)
		{
			(*segList)->v1->x = prevPts->x;
			(*segList)->v1->y = prevPts->y;
		}
		segList = po->segs;
		validcount++;
		for(count = po->numsegs; count; count--, segList++, prevPts++)
		{
			if((*segList)->linedef->validcount != validcount)
			{
				UpdateSegBBox(*segList);
				(*segList)->linedef->validcount = validcount;
			}
			(*segList)->angle -= angle;
		}
		PO_LinkPolyobj(po);
		return false;
	}
	po->angle += angle;
	PO_LinkPolyobj(po);
	return true;
}

//==========================================================================
// PO_UnLinkPolyobj
//==========================================================================
void PO_UnLinkPolyobj(polyobj_t *po)
{
	polyblock_t *link;
	int i, j;
	int index;

	// remove the polyobj from each blockmap section
	for(j = po->bbox[BOXBOTTOM]; j <= po->bbox[BOXTOP]; j++)
	{
		index = j*bmapwidth;
		for(i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight)
			{
				link = polyblockmap[index+i];
				while(link != NULL && link->polyobj != po)
				{
					link = link->next;
				}
				if(link == NULL)
				{ // polyobj not located in the link cell
					continue;
				}
				link->polyobj = NULL;
			}
		}
	}
}

//==========================================================================
// PO_LinkPolyobj
//==========================================================================
void PO_LinkPolyobj(polyobj_t *po)
{
	int leftX, rightX;
	int topY, bottomY;
	seg_t **tempSeg;
	polyblock_t **link;
	polyblock_t *tempLink;
	int i, j;

	// calculate the polyobj bbox
	tempSeg = po->segs;
	rightX = leftX = (*tempSeg)->v1->x;
	topY = bottomY = (*tempSeg)->v1->y;

	for(i = 0; i < po->numsegs; i++, tempSeg++)
	{
		if((*tempSeg)->v1->x > rightX)
		{
			rightX = (*tempSeg)->v1->x;
		}
		if((*tempSeg)->v1->x < leftX)
		{
			leftX = (*tempSeg)->v1->x;
		}
		if((*tempSeg)->v1->y > topY)
		{
			topY = (*tempSeg)->v1->y;
		}
		if((*tempSeg)->v1->y < bottomY)
		{
			bottomY = (*tempSeg)->v1->y;
		}
	}
	po->bbox[BOXRIGHT] = (rightX-bmaporgx)>>MAPBLOCKSHIFT;
	po->bbox[BOXLEFT] = (leftX-bmaporgx)>>MAPBLOCKSHIFT;
	po->bbox[BOXTOP] = (topY-bmaporgy)>>MAPBLOCKSHIFT;
	po->bbox[BOXBOTTOM] = (bottomY-bmaporgy)>>MAPBLOCKSHIFT;
	// add the polyobj to each blockmap section
	for(j = po->bbox[BOXBOTTOM]*bmapwidth; j <= po->bbox[BOXTOP]*bmapwidth;
		j += bmapwidth)
	{
		for(i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight*bmapwidth)
			{
				link = &polyblockmap[j+i];
				if(!(*link))
				{ // Create a new link at the current block cell
					*link = Z_Malloc(sizeof(polyblock_t), PU_LEVEL, 0);
					(*link)->next = NULL;
					(*link)->prev = NULL;
					(*link)->polyobj = po;
					continue;
				}
				else
				{
					tempLink = *link;
					while(tempLink->next != NULL && tempLink->polyobj != NULL)
					{
						tempLink = tempLink->next;
					}
				}
				if(tempLink->polyobj == NULL)
				{
					tempLink->polyobj = po;
					continue;
				}
				else
				{
					tempLink->next = Z_Malloc(sizeof(polyblock_t), 
						PU_LEVEL, 0);
					tempLink->next->next = NULL;
					tempLink->next->prev = tempLink;
					tempLink->next->polyobj = po;
				}
			}
			// else, don't link the polyobj, since it's off the map
		}
	}
}

//==========================================================================
// CheckMobjBlocking
//==========================================================================
static boolean CheckMobjBlocking(seg_t *seg, polyobj_t *po)
{
	mobj_t *mobj, *root;
	int i, j;
	int left, right, top, bottom;
	int tmbbox[4];
	line_t *ld;
	boolean blocked;

	ld = seg->linedef;

	top = (ld->bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	bottom = (ld->bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	left = (ld->bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	right = (ld->bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;

	blocked = false;

	bottom = bottom < 0 ? 0 : bottom;
	bottom = bottom >= bmapheight ? bmapheight-1 : bottom;
	top = top < 0 ? 0 : top;
	top = top >= bmapheight  ? bmapheight-1 : top;
	left = left < 0 ? 0 : left;
	left = left >= bmapwidth ? bmapwidth-1 : left;
	right = right < 0 ? 0 : right;
	right = right >= bmapwidth ?  bmapwidth-1 : right;

	for(j = bottom*bmapwidth; j <= top*bmapwidth; j += bmapwidth)
	{
		for(i = left; i <= right; i++)
		{
			root = (mobj_t*) &blockrings[j + i];
			for(mobj = root->bnext; mobj != root; mobj = mobj->bnext)
			{
				if(mobj->ddflags & DDMF_SOLID || mobj->dplayer)
				{
					tmbbox[BOXTOP] = mobj->y+mobj->radius;
					tmbbox[BOXBOTTOM] = mobj->y-mobj->radius;
					tmbbox[BOXLEFT] = mobj->x-mobj->radius;
					tmbbox[BOXRIGHT] = mobj->x+mobj->radius;

					if(tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
						|| tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
						|| tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
						|| tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
					{
						continue;
					}
					if(P_BoxOnLineSide(tmbbox, ld) != -1)
					{
						continue;
					}
					if(po_callback)	po_callback(mobj, seg, po);
					blocked = true;
				}
			}
		}
	}
	return blocked;
}

//===========================================================================
// PO_GetNumForDegen
//	Returns the index of the polyobj that owns the degenmobj.
//	Returns -1 if nothing is found.
//===========================================================================
int PO_GetNumForDegen(void *degenMobj)
{
	int i;

	for(i = 0; i < po_NumPolyobjs; i++)
	{
		if(&PO_PTR(i)->startSpot == degenMobj)
		{
			return i;
		}
	}
	return -1;
}
