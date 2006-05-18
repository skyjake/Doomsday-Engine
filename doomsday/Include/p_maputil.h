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
 * p_maputil.h: Map Utility Routines
 */

#ifndef __DOOMSDAY_MAP_UTILITIES_H__
#define __DOOMSDAY_MAP_UTILITIES_H__

#include "m_vector.h"
#include "p_mobj.h"

#define	MAXINTERCEPTS	128

#define IS_SECTOR_LINKED(thing)		((thing)->sprev != NULL)
#define IS_BLOCK_LINKED(thing)		((thing)->bnext != NULL)

extern fixed_t  opentop, openbottom, openrange, lowfloor;
extern divline_t trace;

float           P_AccurateDistance(fixed_t dx, fixed_t dy);
fixed_t         P_ApproxDistance(fixed_t dx, fixed_t dy);
fixed_t         P_ApproxDistance3(fixed_t dx, fixed_t dy, fixed_t dz);
void            P_LineUnitVector(line_t *line, float *unitvec);
float           P_MobjPointDistancef(mobj_t *start, mobj_t *end,
									 float *fixpoint);
void            P_SectorBoundingBox(sector_t *sec, float *bbox);
int             P_PointOnLineSide(fixed_t x, fixed_t y, line_t *line);
int             P_BoxOnLineSide(fixed_t *tmbox, line_t *ld);
void            P_MakeDivline(line_t *li, divline_t * dl);
int             P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t * line);
fixed_t         P_InterceptVector(divline_t * v2, divline_t * v1);
int             P_FloatPointOnLineSide(fvertex_t * pnt, fdivline_t * dline);
float           P_FloatInterceptVertex(fvertex_t * start, fvertex_t * end,
									   fdivline_t * fdiv, fvertex_t * inter);
void            P_LineOpening(line_t *linedef);
mobj_t         *P_GetBlockRootIdx(int index);
void            P_LinkThing(mobj_t *thing, byte flags);
void            P_UnlinkThing(mobj_t *thing);
void            P_PointToBlock(fixed_t x, fixed_t y, int *bx, int *by);
boolean         P_BlockThingsIterator(int x, int y,
									  boolean (*func) (mobj_t *, void *),
									  void *);
boolean         P_ThingLinesIterator(mobj_t *thing,
									 boolean (*func) (line_t *, void *),
									 void *);
boolean         P_ThingSectorsIterator(mobj_t *thing,
									   boolean (*func) (sector_t *, void *),
									   void *data);
boolean         P_LineThingsIterator(line_t *line,
									 boolean (*func) (mobj_t *, void *),
									 void *data);
boolean         P_SectorTouchingThingsIterator(sector_t *sector,
											   boolean (*func) (mobj_t *,
																void *),
											   void *data);

boolean         P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
							   int flags, boolean (*trav) (intercept_t *));

#endif
