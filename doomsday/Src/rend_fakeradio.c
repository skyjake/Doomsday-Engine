/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * rend_fakeradio.c: Faked Radiosity Lighting
 *
 * Perhaps the most distinctive characteristic of radiosity lighting
 * is that the corners of a room are slightly dimmer than the rest of
 * the surfaces.  (It's not the only characteristic, however.)  We
 * will fake these shadowed areas by generating shadow polygons for
 * wall segments and determining, which subsector vertices will be
 * shadowed.
 *
 * In other words, walls use shadow polygons (over entire segs), while
 * planes use vertex lighting.  Since planes are usually tesselated
 * into a great deal of subsectors (and triangles), they are better
 * suited for vertex lighting.  In some cases we will be forced to
 * split a subsector into smaller pieces than strictly necessary in
 * order to achieve better accuracy in the shadow effect.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"

#include "m_vector.h"

// MACROS ------------------------------------------------------------------

#define MIN_OPEN .1f
#define EDGE_OPEN_THRESHOLD 8 // world units (Z axis)

// TYPES -------------------------------------------------------------------

typedef struct shadowcorner_s {
	float corner;
	sector_t *proximity;
	float pOffset;
} shadowcorner_t;

typedef struct edgespan_s {
	float length;
	float shift;
} edgespan_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int rendFakeRadio = true; // cvar

float rendRadioLongWallMin = 400;
float rendRadioLongWallMax = 1500;
float rendRadioLongWallDiv = 30;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static sector_t *frontSector;
static float shadowSize, shadowDark;
static float fFloor, fCeil;

// CODE --------------------------------------------------------------------

float Rend_RadioShadowDarkness(int lightlevel)
{
	//.9f - sector->lightlevel/430.0f;
	// FIXME: Make cvars of the constants.
	return .65f - lightlevel/850.0f;
}

/*
 * Before calling the other rendering routines, this must be called to
 * initialize the state of the FakeRadio renderer.
 */ 
void Rend_RadioInitForSector(sector_t *sector)
{
	// By default, the shadow is disabled.
	shadowSize = 0;
	
	if(!rendFakeRadio) return; // Disabled...

	// Visible plane heights.
	fFloor = SECT_FLOOR(sector);
	fCeil  = SECT_CEIL(sector);

	if(fCeil <= fFloor) return; // A closed sector.

	frontSector = sector;
	
	// Determine the shadow properties.
	// FIXME: Make cvars out of constants.
	shadowSize = 2 * (8 + 16 - sector->lightlevel/16);
	shadowDark = Rend_RadioShadowDarkness(sector->lightlevel);
}

/*
 * Returns true if the specified flat is non-glowing, i.e. not glowing
 * or a sky.
 */
boolean Rend_RadioNonGlowingFlat(int flatPic)
{
	return !(flatPic == skyflatnum ||
			 R_FlatFlags(flatPic) & TXF_GLOW);
}

/*
 * Set the vertex colors in the rendpoly.
 */
void Rend_RadioSetColor(rendpoly_t *q, float darkness)
{
	int i;

	// Clamp it.
	if(darkness < 0) darkness = 0;
	if(darkness > 1) darkness = 1;
	
	for(i = 0; i < 2; i++)
	{
		// Shadows are black.
		memset(q->vertices[i].color.rgba, 0, 3);
		q->vertices[i].color.rgba[CA] = (DGLubyte) (255 * darkness);
	}
}

/*
 * Returns true if there is open space in the sector.
 */
boolean Rend_IsSectorOpen(sector_t *sector)
{
	return sector && sector->ceilingheight > sector->floorheight;
}

/*
 * Returns the corner shadow factor.
 */
float Rend_RadioLineCorner(line_t *self, line_t *other, sector_t *mySector)
{
	lineinfo_t *selfInfo = LINE_INFO(self);
	lineinfo_t *otherInfo = LINE_INFO(other);
	vertex_t *myVtx[2], *otherVtx[2];

	binangle_t diff = selfInfo->angle - otherInfo->angle;

	// Sort the vertices so they can be compared consistently.
	R_OrderVertices(self, mySector, myVtx);
	R_OrderVertices(other, mySector, otherVtx);
	
	if(myVtx[0] == other->v1 || myVtx[1] == other->v2)
	{
		// The line normals are not facing the same direction.
		diff -= BANG_180;
	}
	if(myVtx[0] == otherVtx[1])
	{
		// The other is on our left side.
		// We want the difference: (leftmost wall) - (rightmost wall)
		diff = -diff;
	}
	if(diff > BANG_180)
	{
		// The corner between the walls faces outwards.
		return 0;
	}
	if(other->frontsector && other->backsector)
	{
		if(Rend_IsSectorOpen(other->frontsector) &&
		   Rend_IsSectorOpen(other->backsector)) return 0;
	}

	if(diff < BANG_45/5)
	{
		// The difference is too small, there won't be a shadow.
		return 0;
	}
	if(diff > BANG_90)
	{
		// 90 degrees is the largest effective difference.
		diff = BANG_90;
	}
	return (float) diff / BANG_90;
}

/*
 * Set the rendpoly's X offset and texture size.  A negative length
 * implies that the texture is flipped horizontally.
 */
void Rend_RadioTexCoordX(rendpoly_t *q, float lineLength, float segOffset)
{
	q->tex.width = lineLength;
	if(lineLength > 0)
		q->texoffx = segOffset;
	else
		q->texoffx = lineLength + segOffset;
}

/*
 * Set the rendpoly's Y offset and texture size.  A negative size
 * implies that the texture is flipped vertically.
 */ 
void Rend_RadioTexCoordY(rendpoly_t *q, float size)
{
	if((q->tex.height = size) > 0)
		q->texoffy = fCeil - q->top;
	else
		q->texoffy = fFloor - q->top;
}

/*
 * Returns the side number (0, 1) of the neighbour line.
 */
int R_GetAlignedNeighbor(line_t **neighbor, const line_t *line, int side,
						 boolean leftNeighbor)
{
	lineinfo_t *info = LINE_INFO(line), *nInfo;
	lineinfo_side_t *sideInfo = info->side + side;
	int i;

	*neighbor = sideInfo->alignneighbor[leftNeighbor? 0 : 1];
	if(!*neighbor) return 0;

	nInfo = LINE_INFO(*neighbor);

	// We need to decide which side of the neighbor is chosen.
	for(i = 0; i < 2; i++)
	{
		// Do the selection based on the backlink.
		if(nInfo->side[i].alignneighbor[leftNeighbor? 1 : 0] == line)
			return i;
	}

	// This is odd... There is no link back to the first line.
	// Better take no chances.
	*neighbor = NULL;
	return 0;
}

/*
 * Scan a set of aligned neighbours.  Scans simultaneously both the
 * top and bottom edges.  Looks a bit complicated, but that's because
 * the algorithm must handle both the left and right directions, and
 * scans the top and bottom edges at the same time.
 */
void Rend_RadioScanNeighbors(shadowcorner_t top[2], shadowcorner_t bottom[2],
							 line_t *line, int side, edgespan_t spans[2],
							 boolean toLeft)
{
	struct edge_s {
		boolean done;
		line_t *line;
		lineinfo_side_t *sideInfo;
		sector_t *sector;
		float length;
	} edges[2]; // bottom, top
	
	line_t *iter;
	lineinfo_t *nInfo;
	sector_t *scanSector;
	int scanSide;
	int i, nIdx = (toLeft? 0 : 1); // neighbour index

	edges[0].done = edges[1].done = false;
	edges[0].length = edges[1].length = 0;

	for(iter = line, scanSide = side; !(edges[0].done && edges[1].done); )
	{
		scanSector = (scanSide == 0? iter->frontsector : iter->backsector);

		// Should we stop?
		if(iter != line)
		{
			if(SECT_FLOOR(scanSector) != fFloor) edges[0].done = true;
			if(SECT_CEIL(scanSector) != fCeil) edges[1].done = true;
			if(edges[0].done && edges[1].done) break;
		}

		nInfo = LINE_INFO(iter);
		
		// We'll do the top and bottom simultaneously.
		for(i = 0; i < 2; i++)
			if(!edges[i].done)
			{
				edges[i].line = iter;
				edges[i].sideInfo = nInfo->side + scanSide;
				edges[i].sector = scanSector;

				if(iter != line)
					edges[i].length += nInfo->length;
			}

		scanSide = R_GetAlignedNeighbor(&iter, iter, scanSide, toLeft);

		// Stop the scan?
		if(!iter) break;
	}

	for(i = 0; i < 2; i++) // 0=bottom, 1=top
	{
		shadowcorner_t *corner = (i == 0? &bottom[nIdx] : &top[nIdx]);

		// Increment the apparent line length/offset.
		spans[i].length += edges[i].length;
		if(toLeft) spans[i].shift += edges[i].length;
		
		if(edges[i].sideInfo->neighbor[nIdx])
		{
			corner->corner =
				Rend_RadioLineCorner(edges[i].line,
									 edges[i].sideInfo->neighbor[nIdx],
									 edges[i].sector);
		}
		else
		{
			corner->corner = 0;
		}

		if(edges[i].sideInfo->proxsector[nIdx])
		{
			corner->proximity = edges[i].sideInfo->proxsector[nIdx];
			if(i == 0) // floor
				corner->pOffset = SECT_FLOOR(corner->proximity) - fFloor;
			else // ceiling
				corner->pOffset = SECT_CEIL(corner->proximity) - fCeil;
		}
		else
		{
			corner->proximity = NULL;
			corner->pOffset = 0;
		}
	}
}

/*
 * To determine the dimensions of a shadow, we'll need to scan edges.
 * Edges are composed of aligned lines.  It's important to note that
 * the scanning is done separately for the top/bottom edges (both in
 * the left and right direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array
 * 'spans'.
 *
 * This may look like a complicated operation (performed for all wall
 * polys) but in most cases this won't take long.  Aligned neighbours
 * are relatively rare.
 */
void Rend_RadioScanEdges(shadowcorner_t topCorners[2],
						 shadowcorner_t bottomCorners[2],
						 shadowcorner_t sideCorners[2],
						 line_t *line, int side, edgespan_t spans[2])
{
	lineinfo_t *info = LINE_INFO(line);
	lineinfo_side_t *sInfo = info->side + side;
	int i;

	memset(sideCorners, 0, sizeof(sideCorners));
	
	// Find the sidecorners first: left and right neighbour.
	for(i = 0; i < 2; i++)
	{
		if(sInfo->neighbor[i])
		{
			sideCorners[i].corner =
				Rend_RadioLineCorner(line, sInfo->neighbor[i], frontSector);
		}

		// Scan left/right (both top and bottom).
		Rend_RadioScanNeighbors(topCorners, bottomCorners, line, side,
								spans, !i);
	}
}

/*
 * Long walls get slightly larger shadows. The bonus will simply be
 * added to the shadow size for the wall in question.
 */
float Rend_RadioLongWallBonus(float span)
{
	float limit;
	
	if(rendRadioLongWallDiv > 0 && span > rendRadioLongWallMin)
	{
		limit = span - rendRadioLongWallMin;
		if(limit > rendRadioLongWallMax) limit = rendRadioLongWallMax;
		return limit/rendRadioLongWallDiv;
	}
	return 0;
}

/*
 * Create the appropriate FakeRadio shadow polygons for the wall
 * segment.  The quad must be initialized with all the necessary data
 * (normally comes directly from Rend_RenderWallSeg()).
 */
void Rend_RadioWallSection(seg_t *seg, rendpoly_t *origQuad)
{
	sector_t *backSector;
	float bFloor, bCeil, limit, size, segOffset;
	rendpoly_t quad, *q = &quad;
	int i, texture, sideNum;
	lineinfo_t *info;
	lineinfo_side_t *sInfo;
	shadowcorner_t topCn[2], botCn[2], sideCn[2];
	edgespan_t spans[2]; // bottom, top
	edgespan_t *floorSpan = &spans[0], *ceilSpan = &spans[1];
	
	if(shadowSize <= 0 || // Disabled?
	   origQuad->flags & RPF_GLOW ||
	   seg->linedef == NULL) return; 

	info       = LINE_INFO(seg->linedef);
	backSector = seg->backsector;
	segOffset  = FIX2FLT(seg->offset);

	// Choose the info of the correct side.
	if(seg->linedef->frontsector == frontSector)
	{
		sideNum = 0;
		sInfo = &info->side[0];
	}
	else
	{
		sideNum = 1;
		sInfo = &info->side[1];
	}

	// Determine the shadow properties on the edges of the poly.
	for(i = 0; i < 2; i++)
	{
		spans[i].length = info->length;
		spans[i].shift = segOffset;
	}	
	Rend_RadioScanEdges(topCn, botCn, sideCn, seg->linedef, sideNum, spans);

	// Back sector visible plane heights.
	if(backSector)
	{
		bFloor = SECT_FLOOR(backSector);
		bCeil  = SECT_CEIL(backSector);
	}

	// FIXME: rendpoly_t is quite large and this gets called *many*
	// times. Better to copy less stuff, or do something more
	// clever... Like, only pass the geometry part of rendpoly_t.
	memcpy(q, origQuad, sizeof(rendpoly_t));
	
	// Init the quad.
	q->flags = RPF_SHADOW;
	q->texoffx = segOffset;
	q->texoffy = 0;
	q->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
	q->tex.detail = NULL;
	q->tex.width = info->length;
	q->tex.height = shadowSize;
	q->lights = NULL;
	q->intertex.id = 0;
	q->intertex.detail = NULL;
	Rend_RadioSetColor(q, shadowDark);

	// <--FIXME

#if 1
/*
 * Top Shadow
 */
	// The top shadow will reach this far down.
	size = shadowSize + Rend_RadioLongWallBonus(ceilSpan->length);
	limit = fCeil - size;
	if((q->top > limit && q->bottom < fCeil) &&
	   Rend_RadioNonGlowingFlat(frontSector->ceilingpic))
	{
		Rend_RadioTexCoordY(q, size);

		if(topCn[0].corner <= MIN_OPEN && topCn[1].corner <= MIN_OPEN)
		{
			Rend_RadioTexCoordX(q, ceilSpan->length, ceilSpan->shift);
			if(topCn[0].proximity && topCn[1].proximity)
			{
				if(topCn[0].pOffset < 0 && topCn[1].pOffset > 0)
				{
					texture = LST_RADIO_CO;
					// The shadow can't go over the higher edge.
					if(size > -topCn[0].pOffset)
						Rend_RadioTexCoordY(q, -topCn[0].pOffset);
				}
				else if(topCn[0].pOffset > 0 && topCn[1].pOffset < 0)
				{
					// Must flip horizontally!
					texture = LST_RADIO_CO;
					Rend_RadioTexCoordX(q, -ceilSpan->length, ceilSpan->shift);
					// The shadow can't go over the higher edge.
					if(size > -topCn[1].pOffset)
						Rend_RadioTexCoordY(q, -topCn[1].pOffset);
				}
				else
					texture = LST_GRADIENT; // Possibly C/C?
			}
			else
			{
				texture = LST_GRADIENT; // Radio O/O!
			}
		}
		else if(topCn[1].corner <= MIN_OPEN)
		{
			texture = LST_RADIO_CO;
			Rend_RadioTexCoordX(q, ceilSpan->length, ceilSpan->shift);
		}
		else if(topCn[0].corner <= MIN_OPEN)
		{
			texture = LST_RADIO_CO; // Flipped!
			Rend_RadioTexCoordX(q, -ceilSpan->length, ceilSpan->shift);
		}
		else // C/C
		{
			texture = LST_RADIO_CC;
			Rend_RadioTexCoordX(q, ceilSpan->length, ceilSpan->shift);
		}
		q->tex.id = GL_PrepareLSTexture(texture);
		RL_AddPoly(q);
	}
#endif
		
#if 1
/*
 * Bottom Shadow
 */
	size = shadowSize + Rend_RadioLongWallBonus(floorSpan->length)/2;
	limit = fFloor + size;
	if((q->bottom < limit && q->top > fFloor) &&
	   Rend_RadioNonGlowingFlat(frontSector->floorpic))
	{
		Rend_RadioTexCoordY(q, -size);
		if(botCn[0].corner <= MIN_OPEN &&
		   botCn[1].corner <= MIN_OPEN)
		{
			Rend_RadioTexCoordX(q, floorSpan->length, floorSpan->shift);
			if(botCn[0].proximity && botCn[1].proximity)
			{
				if(botCn[0].pOffset > 0 && botCn[1].pOffset < 0)
				{
					texture = LST_RADIO_CO;
					// The shadow can't go over the higher edge.
					if(size > botCn[0].pOffset)
						Rend_RadioTexCoordY(q, -botCn[0].pOffset);
				}
				else if(botCn[0].pOffset < 0 && botCn[1].pOffset > 0)
				{
					// Must flip horizontally!
					texture = LST_RADIO_CO;
					Rend_RadioTexCoordX(q, -floorSpan->length,
										floorSpan->shift);
					// The shadow can't go over the higher edge.
					if(size > botCn[1].pOffset)
						Rend_RadioTexCoordY(q, -botCn[1].pOffset);
				}
				else
					texture = LST_GRADIENT; // Possibly C/C?
			}
			else
			{
				texture = LST_GRADIENT; // Radio O/O!
			}
		}
		else if(botCn[1].corner <= MIN_OPEN)
		{
			texture = LST_RADIO_CO;
			Rend_RadioTexCoordX(q, floorSpan->length, floorSpan->shift);
		}
		else if(botCn[0].corner <= MIN_OPEN)
		{
			texture = LST_RADIO_CO; // Flipped!
			Rend_RadioTexCoordX(q, -floorSpan->length, floorSpan->shift);
		}
		else // C/C
		{
			texture = LST_RADIO_CC;
			Rend_RadioTexCoordX(q, floorSpan->length, floorSpan->shift);
		}
		q->tex.id = GL_PrepareLSTexture(texture);
		RL_AddPoly(q);
	}
#endif

#if 1
/*
 * Left Shadow
 */
	size = shadowSize + Rend_RadioLongWallBonus(info->length);
	if(sideCn[0].corner > 0 &&
	   segOffset < size)
	{
		q->flags |= RPF_HORIZONTAL;
		q->texoffx = segOffset;
		q->texoffy = q->bottom - fFloor;
		q->tex.width = size;
		q->tex.height = fCeil - fFloor;
		q->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
		Rend_RadioSetColor(q, sideCn[0].corner * shadowDark);
		RL_AddPoly(q);
	}

/*
 * Right Shadow
 */
	if(sideCn[1].corner > 0 &&
	   segOffset + q->length > info->length - size)
	{
		q->flags |= RPF_HORIZONTAL;
		q->texoffx = -info->length + segOffset;
		q->texoffy = q->bottom - fFloor;
		q->tex.width = -size;
		q->tex.height = fCeil - fFloor;
		q->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
		Rend_RadioSetColor(q, sideCn[1].corner * shadowDark);
		RL_AddPoly(q);
	}
#endif
	
}

/*
 * Returns a value in the range of 0...2, which depicts how open the
 * specified edge is.  Zero means that the edge is completely closed:
 * it is facing a wall or is relatively distant from the edge on the
 * other side.  Values between zero and one describe how near the
 * other edge is.  An openness value of one means that the other edge
 * is at the same height as this one.  2 means that the other edge is
 * past our height ("clearly open").
 */
float Rend_RadioEdgeOpenness(line_t *line, boolean frontside, boolean isFloor)
{
	sector_t *front = (frontside? line->frontsector : line->backsector);
	sector_t *back = (frontside? line->backsector : line->frontsector);
	sectorinfo_t *fInfo, *bInfo;
	float fz, bhz, bz; // Front and back Z height

	if(!back) return 0; // No backsector, this is a one-sided wall.

	fInfo = SECT_INFO(front);
	bInfo = SECT_INFO(back);

	// Is the back sector closed?
	if(bInfo->visfloor >= bInfo->visceil) return 0;
	
	if(isFloor)
	{
		fz = fInfo->visfloor;
		bz = bInfo->visfloor;
		bhz = bInfo->visceil;
	}
	else
	{
		fz = -fInfo->visceil;
		bz = -bInfo->visceil;
		bhz = -bInfo->visfloor;
	}

	if(fz <= bz - EDGE_OPEN_THRESHOLD || fz >= bhz)
		return 0; // Fully closed.

	if(fz >= bhz - EDGE_OPEN_THRESHOLD)
		return (bhz - fz)/EDGE_OPEN_THRESHOLD;
	
	if(fz <= bz)
		return 1 - (bz - fz)/EDGE_OPEN_THRESHOLD;

	if(fz <= bz + EDGE_OPEN_THRESHOLD)
		return 1 + (fz - bz)/EDGE_OPEN_THRESHOLD;
	
	// Fully open!
	return 2;
}

/*
 * Calculate the corner coordinates and add a new shadow polygon to
 * the rendering lists.
 */
void Rend_RadioAddShadowEdge(shadowpoly_t *shadow, boolean isFloor,
							 float darkness, float sideOpen[2])
{
	rendpoly_t q;
	rendpoly_vertex_t *vtx;
	sector_t *sector;
	float z, pos;
	int i, /*dir = (isFloor? 1 : -1),*/ *idx;
	int floorIndices[] = { 0, 1, 2, 3 };
	int ceilIndices[] = { 0, 3, 2, 1 };
	vec2_t inner[2];

	// This is the sector the shadow is actually in.
	sector = (shadow->flags & SHPF_FRONTSIDE? shadow->line->frontsector
			  : shadow->line->backsector);

	z = (isFloor? SECT_FLOOR(sector) : SECT_CEIL(sector));

	// Sector lightlevel affects the darkness of the shadows.
	if(darkness > 1) darkness = 1;
	darkness *= Rend_RadioShadowDarkness(sector->lightlevel) * .8f;

	// Determine the inner shadow corners.
	for(i = 0; i < 2; i++)
	{
		pos = sideOpen[i];
		if(pos < 1) // Nearly closed.
		{
			/*V2_Lerp(inner[i], shadow->inoffset[i],
			  shadow->bextoffset[i], pos);*/
			V2_Copy(inner[i], shadow->inoffset[i]);
		}
		else if(pos == 1) // Same height on both sides.
		{
			V2_Copy(inner[i], shadow->bextoffset[i]);
		}
		else // Fully, unquestionably open.
		{
			/*if(pos > 2) pos = 2;
			  V2_Lerp(inner[i], shadow->bextoffset[i],
			  shadow->extoffset[i], pos - 1);*/
			V2_Copy(inner[i], shadow->extoffset[i]);
		}
	}

	// Initialize the rendpoly.
	q.type = RP_FLAT;
	q.flags = RPF_SHADOW;
	memset(&q.tex, 0, sizeof(q.tex));
	memset(&q.intertex, 0, sizeof(q.intertex));
	q.interpos = 0;
	q.lights = NULL;
	q.sector = NULL;

	q.top = z;
	q.numvertices = 4;
	memset(q.vertices, 0, q.numvertices * sizeof(rendpoly_vertex_t));
	
	vtx = q.vertices;
	idx = (isFloor? floorIndices : ceilIndices);

	// Left outer corner.
	vtx[idx[0]].pos[VX] = FIX2FLT(shadow->outer[0]->x);
	vtx[idx[0]].pos[VY] = FIX2FLT(shadow->outer[0]->y);
	vtx[idx[0]].color.rgba[CA] = (DGLubyte) (255 * darkness); // Black.

	if(sideOpen[0] < 1)
		vtx[idx[0]].color.rgba[CA] *= 1 - sideOpen[0];

	// Right outer corner.
	vtx[idx[1]].pos[VX] = FIX2FLT(shadow->outer[1]->x);
	vtx[idx[1]].pos[VY] = FIX2FLT(shadow->outer[1]->y);
	vtx[idx[1]].color.rgba[CA] = (DGLubyte) (255 * darkness);

	if(sideOpen[1] < 1)
		vtx[idx[1]].color.rgba[CA] *= 1 - sideOpen[1];

	// Right inner corner.
	vtx[idx[2]].pos[VX] = vtx[idx[1]].pos[VX] + inner[1][VX];
	vtx[idx[2]].pos[VY] = vtx[idx[1]].pos[VY] + inner[1][VY];

	// Left inner corner.
	vtx[idx[3]].pos[VX] = vtx[idx[0]].pos[VX] + inner[0][VX];
	vtx[idx[3]].pos[VY] = vtx[idx[0]].pos[VY] + inner[0][VY];
	
	RL_AddPoly(&q);
}

/*
 * Render the shadowpolygons linked to the subsector, if they haven't
 * already been rendered.
 *
 * Don't use the global radio state in here, the subsector can be part
 * of any sector, not the one chosen for wall rendering.
 */ 
void Rend_RadioSubsectorEdges(subsector_t *subsector)
{
	subsectorinfo_t *info;
	shadowlink_t *link;
	shadowpoly_t *shadow;
	sector_t *sector;
	line_t *neighbor;
	float open, sideOpen[2];
	int i, surface;

	if(!rendFakeRadio) return;

	info = SUBSECT_INFO(subsector);
	
	// We need to check all the shadowpolys linked to this subsector.
	for(link = info->shadows; link != NULL; link = link->next)
	{
		// Already rendered during the current frame? We only want to
		// render each shadow once per frame.
		if(link->poly->visframe == (ushort) framecount) continue;

		// Now it will be rendered.
		shadow = link->poly;
		shadow->visframe = (ushort) framecount;

		// Determine the openness of the line and its neighbors.  If
		// this edge is open, there won't be a shadow at all.  Open
		// neighbours cause some changes in the polygon corner
		// vertices (placement, colour).

		sector = R_GetShadowSector(shadow);
		
		for(surface = 0; surface < 2; surface++)
		{
			// Glowing surfaces shouldn't have shadows on them.
			if(!Rend_RadioNonGlowingFlat(surface? sector->floorpic
										 : sector->ceilingpic)) continue;
				
			open = Rend_RadioEdgeOpenness(
				shadow->line,
				(shadow->flags & SHPF_FRONTSIDE) != 0,
				surface);
			if(open >= 1) continue;
			
			// What about the neighbours?
			for(i = 0; i < 2; i++)
			{
				neighbor = R_GetShadowNeighbor(shadow, i == 0, false);
				sideOpen[i] = Rend_RadioEdgeOpenness(
					neighbor,
					neighbor->frontsector == sector,
					surface);
			}

			Rend_RadioAddShadowEdge(shadow, surface, 1 - open, sideOpen);
		}
	}
}
