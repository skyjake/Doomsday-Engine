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
 */

/*
 * rend_dyn.c: Dynamic Lights
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
	PROF_DYN_INIT_DEL,
	PROF_DYN_INIT_ADD,
	PROF_DYN_INIT_LINK
END_PROF_TIMERS()

//enum { CLIP_TOP, CLIP_BOTTOM, CLIP_LEFT, CLIP_RIGHT };

//#define ZMOD_FLAT		0 //.05f	// How much difference?
//#define ZMOD_WALL		0//.15f		
//#define DISTFACTOR		1		// Really...

#define X_TO_DLBX(cx)			( ((cx) - dlBlockOrig.x) >> (FRACBITS+7) )
#define Y_TO_DLBY(cy)			( ((cy) - dlBlockOrig.y) >> (FRACBITS+7) )
#define DLB_ROOT_DLBXY(bx, by)	(dlBlockLinks + bx + by*dlBlockWidth)

#define LUM_FACTOR(dist)	(1.5f - 1.5f*(dist)/lum->radius)

// TYPES -------------------------------------------------------------------

typedef struct {
	boolean		lightFloor, lightCeiling;
	int			subIndex;
	subsector_t	*subsector;
	float		fceil, ffloor;
	DGLuint		floorDecorMap, ceilDecorMap;
} flatitervars_t;

typedef struct {
	rendpoly_t	*poly;
	float		*v1, *v2;
} wallitervars_t;

typedef struct {
	float		color[3];
	float		size;
	float		xoffset, yoffset;
} lightconfig_t;

typedef struct seglight_s {
	dynlight_t	*mid;
	dynlight_t	*top;
	dynlight_t	*bottom;
} seglight_t;

typedef struct lumcontact_s {
	struct lumcontact_s *next;		// Next in the subsector.
	struct lumcontact_s *nextUsed;	// Next used contact.
	lumobj_t *lum;
} lumcontact_t;

typedef struct contactfinder_data_s {
	fixed_t box[4];
	boolean didSpread;
	lumobj_t *lum;
	int firstValid;
} contactfinder_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

//fvertex_t *edgeClipper(int *numpoints, fvertex_t *points, int numclippers, fdivline_t *clippers);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int			useDynLights;
extern subsector_t	*currentssec;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		dlInited = false;
int			useDynLights = true, dlBlend = 0;
float		dlFactor = 0.7f;	// was 0.6f
int			useWallGlow = true;
int			glowHeight = 100;
float		glowFogBright = .15f;
lumobj_t	*luminousList = 0;
int			numLuminous = 0, maxLuminous = 0;
int			dlMaxRad = 256; // Dynamic lights maximum radius.
float		dlRadFactor = 3;
int			maxDynLights = 0;
//int		clipLights = 1;
int			rend_info_lums = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dynlight nodes.
dynlight_t	*dynFirst, *dynCursor;

lumobj_t	**dlBlockLinks = 0;
vertex_t	dlBlockOrig;
int			dlBlockWidth, dlBlockHeight;	// In 128 blocks.
lumobj_t	**dlSubLinks = 0;		

// A list of dynlight nodes for each surface (seg, floor, ceiling).
// The segs are indexed by seg index, floors/ceilings are indexed by
// subsector index.
seglight_t	*segLightLinks;
dynlight_t	**floorLightLinks;
dynlight_t	**ceilingLightLinks;

// List of unused and used lumobj-subsector contacts.
lumcontact_t *contFirst, *contCursor;

// List of lumobj contacts for each subsector.
lumcontact_t **subContacts;

// A framecount for each block. Used to prevent multiple processing of
// a block during one frame.
int			*spreadBlocks;

// CODE --------------------------------------------------------------------

/*
 * Moves all used dynlight nodes to the list of unused nodes, so they
 * can be reused.
 */
void DL_DeleteUsed(void)
{
	// Start reusing nodes from the first one in the list.
	dynCursor = dynFirst;
	contCursor = contFirst;

	// Clear the surface light links.
	memset(segLightLinks, 0, numsegs * sizeof(seglight_t));
	memset(floorLightLinks, 0, numsubsectors * sizeof(dynlight_t*));
	memset(ceilingLightLinks, 0, numsubsectors * sizeof(dynlight_t*));

	// Clear lumobj contacts.
	memset(subContacts, 0, numsubsectors * sizeof(lumcontact_t*));
}

/*
 * Returns a new dynlight node. If the list of unused nodes is empty, 
 * a new node is created.
 */
dynlight_t *DL_New(float *s, float *t)
{
	dynlight_t *dyn;

	// Have we run out of nodes?
	if(dynCursor == NULL)
	{
		dyn = Z_Malloc(sizeof(dynlight_t), PU_STATIC, NULL);

		// Link the new node to the list.
		dyn->nextUsed = dynFirst;
		dynFirst = dyn;
	}
	else
	{
		dyn = dynCursor;
		dynCursor = dynCursor->nextUsed;
	}

	dyn->next = NULL;
	dyn->flags = 0;

	if(s) 
	{
		dyn->s[0] = s[0];
		dyn->s[1] = s[1];
	}
	if(t)
	{
		dyn->t[0] = t[0];
		dyn->t[1] = t[1];
	}

	return dyn;
}

/*
 * Links the dynlight node to the list.
 */
void DL_Link(dynlight_t *dyn, dynlight_t **list, int index)
{
	dyn->next = list[index];
	list[index] = dyn;
}

void DL_SegLink(dynlight_t *dyn, int index, int segPart)
{
	switch(segPart)
	{
	case SEG_MIDDLE:
		DL_Link(dyn, &segLightLinks[index].mid, 0);
		break;

	case SEG_TOP:
		DL_Link(dyn, &segLightLinks[index].top, 0);
		break;

	case SEG_BOTTOM:
		DL_Link(dyn, &segLightLinks[index].bottom, 0);
		break;
	}
}

/*
 * Returns a pointer to the list of dynlights for the segment part.
 */
dynlight_t *DL_GetSegLightLinks(int seg, int whichpart)
{
	switch(whichpart)
	{
	case SEG_MIDDLE:
		return segLightLinks[seg].mid;

	case SEG_TOP:
		return segLightLinks[seg].top;

	case SEG_BOTTOM:
		return segLightLinks[seg].bottom;
	}
	return NULL;
}

/*
 * Returns a new lumobj contact. If there are nodes in the list of unused 
 * nodes, the new contact is taken from there.
 */
lumcontact_t *DL_NewContact(lumobj_t *lum)
{
	lumcontact_t *con;

	if(contCursor == NULL)
	{
		con = Z_Malloc(sizeof(lumcontact_t), PU_STATIC, NULL);

		// Link to the list of lumcontact nodes.
		con->nextUsed = contFirst;
		contFirst = con;
	}
	else
	{
		con = contCursor;
		contCursor = contCursor->nextUsed;
	}

	con->lum = lum;
	return con;
}

/*
 * Link the contact to the subsector's list of contacts. 
 * The lumobj is contacting the subsector. 
 * This called if a light passes the sector spread test.
 * Returns true because this function is also used as an iterator.
 */
boolean DL_AddContact(subsector_t *subsector, void *lum)
{
	lumcontact_t *con = DL_NewContact(lum);
	lumcontact_t **list = subContacts + GET_SUBSECTOR_IDX(subsector); 

	con->next = *list;
	*list = con;
	return true;
}

void DL_ThingRadius(lumobj_t *lum, lightconfig_t *cf)
{
	lum->radius = cf->size * 40 * dlRadFactor;
	
	// Don't make a too small or too large light.
	if(lum->radius < 32) lum->radius = 32;
	if(lum->radius > dlMaxRad) lum->radius = dlMaxRad;

	lum->flareSize = cf->size * 60 * (50 + haloSize)/100.0f;
	if(lum->flareSize < 8) lum->flareSize = 8;
}

//===========================================================================
// DL_ThingColor
//===========================================================================
void DL_ThingColor(lumobj_t *lum, DGLubyte *outRGB, float light)
{
	int i;

	if(light < 0) light = 0;
	if(light > 1) light = 1;
	light *= dlFactor;

	// If fog is enabled, make the light dimmer.
	// FIXME: This should be a cvar.
	if(useFog) light *= .5f; // Would be too much.

	// Multiply with the light color.
	for(i = 0; i < 3; i++)
	{
		if(!lum->decorMap)
		{
			outRGB[i] = (DGLubyte) (light * lum->rgb[i]);
		}
		else 
		{
			// Decoration maps are pre-colored.
			outRGB[i] = (DGLubyte) (light * 255);
		}
	}
}

//===========================================================================
// DL_InitLinks
//===========================================================================
void DL_InitLinks(void)
{
	byte		*ptr;
	vertex_t	min, max;
	int			i, x, y;

	// First initialize the subsector links (root pointers).
	dlSubLinks = Z_Calloc(sizeof(lumobj_t*) * numsubsectors, PU_LEVEL, 0);

	// Then the blocklinks.
	memcpy(&min, vertexes, sizeof(min));
	memcpy(&max, vertexes, sizeof(max));
	for(i = 1, ptr = vertexes + VTXSIZE; i < numvertexes; i++, ptr += VTXSIZE)
	{
		x = ( (vertex_t*) ptr)->x;
		y = ( (vertex_t*) ptr)->y;
		if(x < min.x) min.x = x;
		if(x > max.x) max.x = x;
		if(y < min.y) min.y = y;
		if(y > max.y) max.y = y;
	}
	
	// Origin has fixed-point coordinates.
	memcpy(&dlBlockOrig, &min, sizeof(min));
	max.x -= min.x;
	max.y -= min.y;
	dlBlockWidth = (max.x >> (FRACBITS+7)) + 1;
	dlBlockHeight = (max.y >> (FRACBITS+7)) + 1;
	
	// Blocklinks is a table of lumobj_t pointers.
	dlBlockLinks = realloc(dlBlockLinks, sizeof(lumobj_t*) 
		* dlBlockWidth * dlBlockHeight);

	// Initialize the dynlight -> surface links.
	segLightLinks = Z_Calloc(numsegs * sizeof(seglight_t), PU_LEVEL, 0);
	floorLightLinks = Z_Calloc(numsubsectors * sizeof(dynlight_t*), 
		PU_LEVEL, 0);
	ceilingLightLinks = Z_Calloc(numsubsectors * sizeof(dynlight_t*),
		PU_LEVEL, 0);

	// Initialize lumobj -> subsector contacts.
	subContacts = Z_Calloc(numsubsectors * sizeof(lumcontact_t*), 
		PU_LEVEL, 0);

	// A framecount was each block.
	spreadBlocks = Z_Malloc(sizeof(int) * dlBlockWidth * dlBlockHeight, 
		PU_LEVEL, 0);
}

// Set dlq.top and .bottom before calling this.
/*static void DL_FinalizeSurface(rendpoly_t *dlq, lumobj_t *lum)
{
	dlq->light = lum;
	dlq->texoffy = FIX2FLT(lum->thing->z) + lum->center 
		+ lum->radius/DYN_ASPECT - dlq->top;
	if(dlq->texoffy < -dlq->top+dlq->bottom) return;
	if(dlq->texoffy > dlq->texh) return;
	if(dlq->top+dlq->texoffy-dlq->texh >= dlq->bottom) // Fits completely?
		dlq->bottom = dlq->top+dlq->texoffy-dlq->texh;
	if(dlq->texoffy < 0)
	{
		dlq->top += dlq->texoffy;
		dlq->texoffy = 0;
	}
	// Give the new quad to the rendering engine.
	RL_AddPoly(dlq);
}*/

//===========================================================================
// DL_SegTexCoords
//	Returns true if the coords are in range.
//===========================================================================
boolean DL_SegTexCoords(float *t, float top, float bottom, lumobj_t *lum)
{
	float lightZ = FIX2FLT(lum->thing->z) + lum->center;
	float radius = lum->radius / DYN_ASPECT;

	t[0] = (lightZ + radius - top)/(2 * radius);
	t[1] = t[0] + (top - bottom)/(2 * radius);

	return t[0] < 1 && t[1] > 0;
}

//===========================================================================
// DL_ProcessWallSeg
//	The front sector must be given because of polyobjs.
//===========================================================================
void DL_ProcessWallSeg(lumobj_t *lum, seg_t *seg, sector_t *frontsec)
{
	int			present = 0;
	sector_t	*backsec = seg->backsector;
	side_t		*sdef = seg->sidedef;
	float		pos[2][2], s[2], t[2];
	float		dist, pntLight[2];
	float		fceil, ffloor, bceil, bfloor;
	float		top, bottom;
	dynlight_t	*dyn;
	int			segindex = GET_SEG_IDX(seg);

	fceil = SECT_CEIL(frontsec);
	ffloor = SECT_FLOOR(frontsec);

	// A zero-volume sector?
	if(fceil <= ffloor) return;

	if(backsec)
	{
		bceil = SECT_CEIL(backsec);
		bfloor = SECT_FLOOR(backsec);
	}

	// Let's begin with an analysis of the visible surfaces.	
	if(sdef->midtexture)
	{
		present |= SEG_MIDDLE;
		if(backsec)
		{
			// Check the middle texture's mask status.
			GL_GetTextureInfo(sdef->midtexture); 
			/*if(texmask)
			{
				// We can't light masked textures.
				// FIXME: Use vertex lighting.
				present &= ~SEG_MIDDLE;
			}*/
		}
	}
	// Is there a top wall segment?
	if(backsec)
	{
		if(fceil > bceil && sdef->toptexture)
		{
			present |= SEG_TOP;
		}
		// Is there a lower wall segment? 
		if(ffloor < bfloor && sdef->bottomtexture)
		{
			present |= SEG_BOTTOM;
		}
		// The top texture can't be present when front and back sectors
		// both have the sky ceiling.
		if(frontsec->ceilingpic == skyflatnum 
			&& backsec->ceilingpic == skyflatnum)
		{
			present &= ~SEG_TOP;
		}
		// The same applies to sky floors.
		if(frontsec->floorpic == skyflatnum
			&& backsec->floorpic == skyflatnum)
		{
			present &= ~SEG_BOTTOM;
		}
	}

	// There are no surfaces to light!
	if(!present) return;

/*	dlq.type = rp_quad;
	dlq.numvertices = 2;
	dlq.vertices[0].pos[VX] = FIX2FLT(seg->v1->x);
	dlq.vertices[0].pos[VY] = FIX2FLT(seg->v1->y);
	dlq.vertices[1].pos[VX] = FIX2FLT(seg->v2->x);
	dlq.vertices[1].pos[VY] = FIX2FLT(seg->v2->y);*/

	pos[0][VX] = FIX2FLT(seg->v1->x);
	pos[0][VY] = FIX2FLT(seg->v1->y);
	pos[1][VX] = FIX2FLT(seg->v2->x);
	pos[1][VY] = FIX2FLT(seg->v2->y);

	// We will only calculate light placement for segs that are facing
	// the viewpoint.
	if(!Rend_SegFacingDir(pos[0], pos[1])) return; 

	pntLight[VX] = FIX2FLT(lum->thing->x);
	pntLight[VY] = FIX2FLT(lum->thing->y);

	// Calculate distance between seg and light source.
	dist = ((pos[0][VY] - pntLight[VY]) * (pos[1][VX] - pos[0][VX]) - 
		(pos[0][VX] - pntLight[VX]) * (pos[1][VY] - pos[0][VY])) 
		/ seg->length;

	// Is it close enough and on the right side?
	if(dist < 0 || dist > lum->radius) return; // Nope.

	// Initialize the poly used for all of the lights.
/*	dlq.flags = RPF_LIGHT; // This is a light polygon.
	dlq.texw = lum->radius*2;
	dlq.texh = dlq.texw/DYN_ASPECT;
	dlq.length = seg->length;*/

//	lightZ = FIX2FLT(lum->thing->z) + lum->center;

	// Do a scalar projection for the offset.
	s[0] = (-( (pos[0][VY] - pntLight[VY]) * (pos[0][VY] - pos[1][VY]) 
		- (pos[0][VX] - pntLight[VX]) * (pos[1][VX] - pos[0][VX]) ) 
		/ seg->length + lum->radius) / (2 * lum->radius);

	s[1] = s[0] + seg->length / (2 * lum->radius);

	// Would the light be visible?
	if(s[0] >= 1 || s[1] <= 0) return; // Outside the seg.

	// Process the visible parts of the segment.
	if(present & SEG_MIDDLE)
	{
		if(backsec)
		{
			top    = (fceil  < bceil?  fceil  : bceil);
			bottom = (ffloor > bfloor? ffloor : bfloor);
			Rend_MidTexturePos(&top, &bottom, NULL, FIX2FLT(sdef->rowoffset),
				seg->linedef? (seg->linedef->flags & ML_DONTPEGBOTTOM) != 0
				: false);
		}
		else
		{
			top    = fceil;
			bottom = ffloor;
		}

		if(DL_SegTexCoords(t, top, bottom, lum))
		{
			dyn = DL_New(s, t);
			DL_ThingColor(lum, dyn->color, LUM_FACTOR(dist));
			dyn->texture = lum->tex;
			DL_SegLink(dyn, segindex, SEG_MIDDLE);	
		}
	}
	if(present & SEG_TOP)
	{
		if(DL_SegTexCoords(t, fceil, MAX_OF(ffloor, bceil), lum))
		{
			dyn = DL_New(s, t);
			DL_ThingColor(lum, dyn->color, LUM_FACTOR(dist));
			dyn->texture = lum->tex;
			DL_SegLink(dyn, segindex, SEG_TOP);	
		}
	}
	if(present & SEG_BOTTOM)
	{
		if(DL_SegTexCoords(t, MIN_OF(bfloor, fceil), ffloor, lum))
		{
			dyn = DL_New(s, t);
			DL_ThingColor(lum, dyn->color, LUM_FACTOR(dist));
			dyn->texture = lum->tex;
			DL_SegLink(dyn, segindex, SEG_BOTTOM);	
		}
	}
	
/*	// Do a scalar projection for the offset.
	dlq.texoffx = ((dlq.vertices[0].pos[VY] - pntLight[VY]) * 
		(dlq.vertices[0].pos[VY] - dlq.vertices[1].pos[VY]) - 
		(dlq.vertices[0].pos[VX] - pntLight[VX]) * 
		(dlq.vertices[1].pos[VX] - dlq.vertices[0].pos[VX])) / seg->length 
		- lum->radius;

	// There is no need to draw the *whole* wall always. Adjust the start
	// and end points so that only a relevant portion is included.
	if(dlq.texoffx > dlq.length) return; // Doesn't fit on the wall.
	if(dlq.texoffx < -dlq.texw) return; // Ditto, but in the other direction.

	// The wall vector.
	for(c = 0; c < 2; c++) 
		uvecWall[c] = (dlq.vertices[1].pos[c] - dlq.vertices[0].pos[c]) / dlq.length;

	if(dlq.texoffx > 0)
	{
		for(c=0; c<2; c++) dlq.vertices[0].pos[c] += dlq.texoffx * uvecWall[c];
		if(dlq.texoffx+dlq.texw <= dlq.length) // Fits completely?
		{
			for(c=0; c<2; c++) 
				dlq.vertices[1].pos[c] = dlq.vertices[0].pos[c] + dlq.texw*uvecWall[c];
			dlq.length = dlq.texw;
		}
		else 
		{
			// Doesn't fit, must truncate.
			dlq.length -= dlq.texoffx; 
		}
		dlq.texoffx = 0;
	}
	else // It goes off to the left.
	{
		if(dlq.texoffx+dlq.texw <= dlq.length) // Fits completely?
		{
			for(c=0; c<2; c++) 
			{
				dlq.vertices[1].pos[c] = dlq.vertices[0].pos[c] + 
					(dlq.texw + dlq.texoffx) * uvecWall[c];
			}
			dlq.length = dlq.texw + dlq.texoffx;
		}
	}

	// Calculate the color of the light.
	DL_ThingColor(lum, dlq.vertices[0].color.rgb, LUM_FACTOR(dist));

	// Render the present surfaces.
	if(present & WSP_MIDDLE) 
	{
		if(backsec)
		{
			dlq.top = fceil < bceil? fceil : bceil;
			dlq.bottom = ffloor > bfloor? ffloor : bfloor;
			Rend_MidTexturePos(&dlq, FIX2FLT(sdef->rowoffset),
				seg->linedef? (seg->linedef->flags & ML_DONTPEGBOTTOM) != 0
				: false);
		}
		else
		{
			dlq.top = fceil;
			dlq.bottom = ffloor;
		}
		DL_FinalizeSurface(&dlq, lum);
		// Mark the seg for dlight rendering.
		seg->flags |= DDSEGF_DLIGHT;
	}
	if(present & WSP_TOP)
	{
		dlq.top = fceil;
		dlq.bottom = bceil;
		DL_FinalizeSurface(&dlq, lum);
		// Mark the seg for dlight rendering.
		seg->flags |= DDSEGF_DLIGHT;
	}
	if(present & WSP_BOTTOM)
	{
		dlq.top = bfloor;
		dlq.bottom = ffloor;
		DL_FinalizeSurface(&dlq, lum);
		// Mark the seg for dlight rendering.
		seg->flags |= DDSEGF_DLIGHT;
	}
	*/
}

/*
 * Generates one dynlight node per plane glow. The light is attached to 
 * the appropriate seg part.
 */
void DL_CreateGlowLights
	(seg_t *seg, int part, float segtop, float segbottom, 
	 boolean glow_floor, boolean glow_ceil)
{
	dynlight_t *dyn;
	int i, g, segindex = GET_SEG_IDX(seg);
	float ceil, floor, top, bottom, s[2], t[2];
	sector_t *sect = seg->sidedef->sector;
	
	// Check the heights.
	if(segtop <= segbottom) return;	// No height.

	ceil  = SECT_CEIL(sect);
	floor = SECT_FLOOR(sect);

	if(segtop > ceil) segtop = ceil;
	if(segbottom < floor) segbottom = floor;

	for(g = 0; g < 2; g++)
	{
		// Only do what's told.
		if((g == 0 && !glow_ceil) ||
		   (g == 1 && !glow_floor)) continue;

		// Calculate texture coords for the light.
		// The horizontal direction is easy.
		s[0] = 0;
		s[1] = 1;

		if(g == 0)
		{
			// Ceiling glow.
			top = ceil;
			bottom = ceil - glowHeight;

			t[0] = (top - segtop) / glowHeight;
			t[1] = t[0] + (segtop - segbottom) / glowHeight;

			if(t[0] > 1 || t[1] < 0) continue;
		}
		else
		{
			// Floor glow.
			bottom = floor;
			top = floor + glowHeight;

			t[1] = (segbottom - bottom) / glowHeight;
			t[0] = t[1] + (segtop - segbottom) / glowHeight;

			if(t[1] > 1 || t[0] < 0) continue;
		}

		dyn = DL_New(s, t);
		dyn->texture = GL_PrepareLSTexture(LST_GRADIENT);
		
		GL_GetFlatColor(g? sect->floorpic : sect->ceilingpic, dyn->color);

		for(i = 0; i < 3; i++)
		{
			dyn->color[i] *= dlFactor;

			// In fog, additive blending is used. The normal fog color
			// is way too bright.
			if(useFog) dyn->color[i] *= glowFogBright;
		}
		DL_SegLink(dyn, segindex, part);
	}
}

/*
 * If necessary, generate dynamic lights for plane glow.
 */
void DL_ProcessWallGlow(seg_t *seg, sector_t *sect)
{
	boolean do_floor = (R_FlatFlags(sect->floorpic) & TXF_GLOW) != 0;
	boolean do_ceil = (R_FlatFlags(sect->ceilingpic) & TXF_GLOW) != 0;
	sector_t *back = seg->backsector;
	side_t *sdef = seg->sidedef;
	float fceil, ffloor, bceil, bfloor;
	float opentop, openbottom;//, top, bottom;
	float v1[2], v2[2];

	// Check if this segment is actually facing our way.
	v1[VX] = FIX2FLT(seg->v1->x);
	v1[VY] = FIX2FLT(seg->v1->y);
	v2[VX] = FIX2FLT(seg->v2->x);
	v2[VY] = FIX2FLT(seg->v2->y);
	if(!Rend_SegFacingDir(v1, v2)) return;	// Nope...

	// Visible plane heights.
	fceil  = SECT_CEIL(sect);
	ffloor = SECT_FLOOR(sect);
	if(back)
	{
		bceil  = SECT_CEIL(back);
		bfloor = SECT_FLOOR(back); 
	}

	// Determine which portions of the segment get lit.
	if(!back)
	{
		// One sided.
		DL_CreateGlowLights(seg, SEG_MIDDLE, fceil, ffloor, do_floor, do_ceil);
	}
	else
	{
//#define MIN(x,y,z)		((x)<(y)? ((z)=(y)),(x) : ((z)=(x)),(y))
//#define MAX(x,y,z)		((x)>(y)? ((z)=(y)),(x) : ((z)=(x)),(y))

		// Two-sided.
		opentop    = MIN_OF(fceil, bceil);
		openbottom = MAX_OF(ffloor, bfloor);

		// The glow can only be visible in the front sector's height range.
		/*if(top > fceil) top = fceil;
		if(opentop > fceil) opentop = fceil;
		if(openbottom > fceil) openbottom = fceil;
		if(bottom > fceil) bottom = fceil;

		if(top < ffloor) top = ffloor;
		if(opentop < ffloor) opentop = ffloor;
		if(openbottom < ffloor) openbottom = ffloor;
		if(bottom < ffloor) bottom = ffloor;*/
		
		// Is there a middle texture?
		if(sdef->midtexture)
		{
			GL_GetTextureInfo(sdef->midtexture);
			if(!texmask)
			{
				DL_CreateGlowLights(seg, SEG_MIDDLE, opentop, openbottom, 
					do_floor, do_ceil);
			}
		}
		if(sdef->toptexture && fceil > bceil)
		{
			DL_CreateGlowLights(seg, SEG_TOP, fceil, bceil, //top, opentop, 
				do_floor, do_ceil);
		}
		if(sdef->bottomtexture && ffloor < bfloor)
		{
			DL_CreateGlowLights(seg, SEG_BOTTOM, bfloor, ffloor, //openbottom, bottom, 
				do_floor, do_ceil);
		}
	}
}

//===========================================================================
// DL_Clear
//===========================================================================
void DL_Clear()
{
	if(luminousList) Z_Free(luminousList);
	luminousList = 0;
	maxLuminous = numLuminous = 0;

	free(dlBlockLinks);
	dlBlockLinks = 0;
	dlBlockOrig.x = dlBlockOrig.y = 0;
	dlBlockWidth = dlBlockHeight = 0;
}

//===========================================================================
// DL_ClearForFrame
//===========================================================================
void DL_ClearForFrame(void)
{
#ifdef DD_PROFILE
	static int i;

	if(++i > 40)
	{
		i = 0;
		PRINT_PROF( PROF_DYN_INIT_DEL );
		PRINT_PROF( PROF_DYN_INIT_ADD );
		PRINT_PROF( PROF_DYN_INIT_LINK );
	}
#endif

	// Clear all the roots.
	memset(dlSubLinks, 0, sizeof(lumobj_t*) * numsubsectors);
	memset(dlBlockLinks, 0, sizeof(lumobj_t*) * dlBlockWidth * dlBlockHeight);

	numLuminous = 0;
}

//===========================================================================
// DL_NewLuminous
//	Allocates a new lumobj and returns a pointer to it.
//===========================================================================
int DL_NewLuminous(void)
{
	lumobj_t *newList;

	numLuminous++;

	// Only allocate memory when it's needed.
	// FIXME: No upper limit?
	if(numLuminous > maxLuminous)
	{
		maxLuminous *= 2;

		// The first time, allocate eight lumobjs.
		if(!maxLuminous) maxLuminous = 8;

		newList = Z_Malloc(sizeof(lumobj_t) * maxLuminous, PU_STATIC, 0);

		// Copy the old data over to the new list.
		if(luminousList)
		{
			memcpy(newList, luminousList, sizeof(lumobj_t) 
				* (numLuminous - 1));
			Z_Free(luminousList);
		}
		luminousList = newList;
	}

	// Clear the new lumobj.
	memset(luminousList + numLuminous - 1, 0, sizeof(lumobj_t));

	return numLuminous; // == index + 1
}

//===========================================================================
// DL_GetLuminous
//	Returns a pointer to the lumobj with the given 1-based index.
//===========================================================================
lumobj_t *DL_GetLuminous(int index)
{
	if(index <= 0 || index > numLuminous) return NULL;
	return luminousList + index - 1;
}

//===========================================================================
// DL_AddLuminous
//	Registers the given thing as a luminous, light-emitting object.
//	Note that this is called each frame for each luminous object!
//===========================================================================
void DL_AddLuminous(mobj_t *thing)
{
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	int i, lump;
	float mul;
	lumobj_t *lum;
	lightconfig_t cf;
	ded_light_t *def = 0;
	modeldef_t *mf, *nextmf;
	
	if((thing->frame & FF_FULLBRIGHT 
		&& !(thing->ddflags & DDMF_DONTDRAW))
			|| thing->ddflags & DDMF_ALWAYSLIT)
	{
		// Determine the sprite frame lump of the source.
		sprdef = &sprites[thing->sprite];
		sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK ];
		if(sprframe->rotate)
		{
			lump = sprframe->lump[ (R_PointToAngle(thing->x, thing->y) 
				- thing->angle + (unsigned)(ANG45/2)*9) >> 29 ];
		}
		else
		{
			lump = sprframe->lump[0];
		}

		// This'll allow a halo to be rendered. If the light is hidden from 
		// view by world geometry, the light pointer will be set to NULL.
		thing->light = DL_NewLuminous();

		lum = DL_GetLuminous(thing->light);
		lum->thing = thing;
		lum->flags = LUMF_CLIPPED;

		// This'll ensure we have up-to-date information about the texture.
		GL_PrepareSprite(lump, 0);

		// Let's see what our light should look like.
		cf.size = spritelumps[lump].lumsize;
		cf.xoffset = spritelumps[lump].flarex;
		cf.yoffset = spritelumps[lump].flarey;

		// X offset to the flare position.
		lum->xOff = cf.xoffset - spritelumps[lump].width/2.0f;

		// Does the thing have an active light definition?
		if(thing->state && thing->state->light)
		{
			def = (ded_light_t*) thing->state->light;
			if(def->size) cf.size = def->size;
			if(def->xoffset) 
			{
				// Set the x offset here.
				lum->xOff = cf.xoffset = def->xoffset;
			}
			if(def->yoffset) cf.yoffset = def->yoffset;		
			lum->flags |= def->flags;
		}

		lum->patch = lump;
		lum->center = spritelumps[lump].topoffset
			- FIX2FLT(lum->thing->floorclip + R_GetBobOffset(lum->thing))
			- cf.yoffset;
		
		// Will the sprite be allowed to go inside the floor?
		mul = FIX2FLT(thing->z) + spritelumps[lump].topoffset 
			- spritelumps[lump].height
			- FIX2FLT(thing->subsector->sector->floorheight);
		if(!(thing->ddflags & DDMF_NOFITBOTTOM) 
			&& mul < 0)
		{
			// Must adjust.
			lum->center -= mul;
		}

		// Sets the dynlight and flare radii.
		DL_ThingRadius(lum, &cf);

		// Does the mobj use a light scale?
		if(thing->ddflags & DDMF_LIGHTSCALE)
		{
			// Also reduce the size of the light according to 
			// the scale flags. *Won't affect the flare.*
			mul = 1.0f - ((thing->ddflags & DDMF_LIGHTSCALE) 
				>> DDMF_LIGHTSCALESHIFT)/4.0f;
			lum->radius *= mul;
		}

		if(def && (def->color[0] || def->color[1] || def->color[2]))
		{
			// If any of the color components are != 0, use the 
			// definition's color.
			for(i = 0; i < 3; i++) 
				lum->rgb[i] = (byte) (255 * def->color[i]);
		}
		else
		{
			// Use the sprite's (amplified) color.
			GL_GetSpriteColor(lum->patch, lum->rgb);
		}
		lum->flareMul = 1;

		// Approximate the distance in 3D.
		lum->distance = P_ApproxDistance3(thing->x - viewx, 
			thing->y - viewy, thing->z - viewz);

		// Is there a model definition?
		R_CheckModelFor(thing, &mf, &nextmf);
		if(mf && useModels)
			lum->xyScale = MAX_OF(mf->scale[VX], mf->scale[VZ]);
		else
			lum->xyScale = 1;

		// This light source is not associated with a decormap.
		lum->decorMap = 0;

		// Use the same default light texture for all directions.
		if(def)
		{
			lum->tex = def->sides.tex;
			lum->ceilTex = def->up.tex;
			lum->floorTex = def->down.tex;
		}
		else
		{
			lum->tex = lum->ceilTex = lum->floorTex =
				GL_PrepareLSTexture(LST_DYNAMIC);
		}
	}
}

//===========================================================================
// DL_ContactSector
//===========================================================================
void DL_ContactSector(lumobj_t *lum, fixed_t *box, sector_t *sector)
{
	P_SubsectorBoxIterator(box, sector, DL_AddContact, lum);
}

//===========================================================================
// DLIT_ContactFinder
//===========================================================================
boolean DLIT_ContactFinder(line_t *line, void *data)
{
	contactfinder_data_t *light = data;
	sector_t *source, *dest;
	lineinfo_t *info;
	float distance;
	
	if(!line->backsector 
		|| !line->frontsector
		|| line->frontsector == line->backsector)
	{
		// Line must be between two different sectors.
		return true;
	}

	// Which way does the spread go?
	if(line->frontsector->validcount == validcount)
	{
		source = line->frontsector;
		dest = line->backsector;
	}
	else if(line->backsector->validcount == validcount)
	{
		source = line->backsector;
		dest = line->frontsector;
	}
	else 
	{
		// Not eligible for spreading.
		return true;
	}

	if(dest->validcount >= light->firstValid
		&& dest->validcount <= validcount + 1) 
	{
		// This was already spreaded to.
		return true;
	}

	// Is this line inside the light's bounds?
	if(line->bbox[BOXRIGHT] <= light->box[BOXLEFT]
		|| line->bbox[BOXLEFT] >= light->box[BOXRIGHT]
		|| line->bbox[BOXTOP] <= light->box[BOXBOTTOM]
		|| line->bbox[BOXBOTTOM] >= light->box[BOXTOP])
	{
		// The line is not inside the light's bounds.
		return true;
	}

	// Can the spread happen?
	if(dest->ceilingheight <= dest->floorheight
		|| dest->ceilingheight <= source->floorheight
		|| dest->floorheight >= source->ceilingheight)
	{
		// No; destination sector is closed with no height.
		return true;
	}

	info = lineinfo + GET_LINE_IDX(line);
	if(info->length <= 0) 
	{
		// This can't be a good line.
		return true;
	}

	// Calculate distance to line.
	distance = ( FIX2FLT(line->v1->y - light->lum->thing->y) 
		* FIX2FLT(line->dx)	- FIX2FLT(line->v1->x - light->lum->thing->x) 
		* FIX2FLT(line->dy) ) / info->length;

	if((source == line->frontsector && distance < 0) ||
	   (source == line->backsector && distance > 0))
	{
		// Can't spread in this direction.
		return true;
	}

	// Check distance against the light radius.
	if(distance < 0) distance = -distance;
	if(distance >= light->lum->radius) 
	{
		// The light doesn't reach that far.
		return true;
	}

	// Light spreads to the destination sector.
	light->didSpread = true;

	// During next step, light will continue spreading from there.
	dest->validcount = validcount + 1;

	// Add this lumobj to the destination's subsectors.
	DL_ContactSector(light->lum, light->box, dest);

	return true;
}

//===========================================================================
// DL_FindContacts
//	Create a contact for this lumobj in all the subsectors this light
//	source is contacting (tests done on bounding boxes and the sector
//	spread test).
//===========================================================================
void DL_FindContacts(lumobj_t *lum)
{
	int firstValid = ++validcount;
	int xl, yl, xh, yh, bx, by;
	contactfinder_data_t light;
	// Use a slightly smaller radius than what the light really is.
	fixed_t radius = lum->radius * FRACUNIT - 2*FRACUNIT;
	static uint numSpreads = 0, numFinds = 0;

	/*DL_AddContact(lum, lum->thing->subsector);*/

	// Do the sector spread. Begin from the light's own sector.
	lum->thing->subsector->sector->validcount = validcount;

	light.lum = lum;
	light.firstValid = firstValid;
    light.box[BOXTOP] = lum->thing->y + radius;
    light.box[BOXBOTTOM] = lum->thing->y - radius;
    light.box[BOXRIGHT] = lum->thing->x + radius;
    light.box[BOXLEFT] = lum->thing->x - radius;

	DL_ContactSector(lum, light.box, lum->thing->subsector->sector);

	xl = (light.box[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
	xh = (light.box[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
	yl = (light.box[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
	yh = (light.box[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

	numFinds++;

	// We'll keep doing this until the light has spreaded everywhere 
	// inside the bounding box.
	do
	{
		light.didSpread = false;

		numSpreads++;

		for(bx = xl; bx <= xh; bx++)
			for(by = yl; by <= yh; by++)
				P_BlockLinesIterator(bx, by, DLIT_ContactFinder, &light);

		// Increment validcount for the next round of spreading.
		validcount++;
	}
	while(light.didSpread);

/*#ifdef _DEBUG
	if(!((numFinds + 1) % 1000))
	{
		Con_Printf("finds=%i avg=%.3f\n", numFinds, 
			numSpreads / (float)numFinds);
	}
#endif*/
}

//===========================================================================
// DL_SpreadBlocks
//===========================================================================
void DL_SpreadBlocks(subsector_t *subsector)
{
	int xl, xh, yl, yh, x, y, *count;
	lumobj_t *iter;

	xl = X_TO_DLBX(	FLT2FIX(subsector->bbox[0].x - dlMaxRad) );
	xh = X_TO_DLBX( FLT2FIX(subsector->bbox[1].x + dlMaxRad) );
	yl = Y_TO_DLBY( FLT2FIX(subsector->bbox[0].y - dlMaxRad) );
	yh = Y_TO_DLBY( FLT2FIX(subsector->bbox[1].y + dlMaxRad) );

	// Are we completely outside the blockmap?
	if(xh < 0 || xl >= dlBlockWidth || yh < 0 || yl >= dlBlockHeight) 
		return;

	// Clip to blockmap bounds.
	if(xl < 0) xl = 0;
	if(xh >= dlBlockWidth) xh = dlBlockWidth - 1;
	if(yl < 0) yl = 0;
	if(yh >= dlBlockHeight) yh = dlBlockHeight - 1;

	for(x = xl; x <= xh; x++)
		for(y = yl; y <= yh; y++)
		{
			count = &spreadBlocks[x + y * dlBlockWidth];
			if(*count != framecount)
			{
				*count = framecount;

				// Spread the lumobjs in this block.
				for(iter = *DLB_ROOT_DLBXY(x, y); iter; iter = iter->next)
					DL_FindContacts(iter);				
			}
		}
}

//===========================================================================
// lumobjSorter
//	Used to sort lumobjs by distance from viewpoint.
//===========================================================================
int C_DECL lumobjSorter(const void *e1, const void *e2)
{
	lumobj_t *lum1 = DL_GetLuminous( *(const ushort*) e1 );
	lumobj_t *lum2 = DL_GetLuminous( *(const ushort*) e2 );

	if(lum1->distance > lum2->distance) return 1;
	if(lum1->distance < lum2->distance) return -1;
	return 0;
}

//===========================================================================
// DL_LinkLuminous
//	Clears the dlBlockLinks and then links all the listed luminous objects.
//===========================================================================
void DL_LinkLuminous()
{
#define MAX_LUMS 8192 // Normally 100-200, heavy: 1000
	ushort		order[MAX_LUMS];	

	lumobj_t	**root, *lum;
	int			i, bx, by, num = numLuminous;

	// Should the proper order be determined?
	if(maxDynLights)
	{
		if(num > maxDynLights) num = maxDynLights;
		
		// Init the indices.
		for(i = 0; i < numLuminous; i++)
			order[i] = i + 1;

		qsort(order, numLuminous, sizeof(ushort), lumobjSorter);
	}
	
	for(i = 0; i < num; i++)
	{
		lum = (maxDynLights? DL_GetLuminous(order[i]) : luminousList + i);

		// Link this lumobj to the dlBlockLinks, if it can be linked.
		lum->next = NULL;
		bx = X_TO_DLBX(lum->thing->x);
		by = Y_TO_DLBY(lum->thing->y);
		if(bx >= 0 && by >= 0 && bx < dlBlockWidth && by < dlBlockHeight)
		{
			root = DLB_ROOT_DLBXY(bx, by);
			lum->next = *root;
			*root = lum;
		}

		// Link this lumobj into its subsector (always possible).
		root = dlSubLinks + GET_SUBSECTOR_IDX(lum->thing->subsector);
		lum->ssNext = *root;
		*root = lum;
	}
}

//===========================================================================
// DL_IsTexUsed
//	Returns true if the texture is already used in the list of dynlights.
//===========================================================================
boolean DL_IsTexUsed(dynlight_t *node, DGLuint texture)
{
	for(; node; node = node->next)
		if(node->texture == texture) return true;
	return false;
}

//===========================================================================
// DL_LightIteratorFunc
//===========================================================================
boolean DL_LightIteratorFunc(lumobj_t *lum, flatitervars_t *fi) 
{
	int			i;
	byte		*seg;
	float		x = FIX2FLT(lum->thing->x);
	float		y = FIX2FLT(lum->thing->y);
	float		z = FIX2FLT(lum->thing->z);
	float		cdiff, fdiff, applyCeiling, applyFloor, srcRadius;
	float		s[2], t[2];
	dynlight_t	*dyn;

	if(haloMode)
	{
		if(lum->thing->subsector == fi->subsector)
			lum->flags |= LUMF_RENDERED;
	}

	// Center the Z.
	z += lum->center;
	srcRadius = lum->radius/4;
	if(srcRadius == 0) srcRadius = 1;
	applyFloor = applyCeiling = 0;

	// Determine on which side the light is for both the floor and ceiling.
	if(fi->lightCeiling)
	{
		if(z < fi->fceil) 
			applyCeiling = 1;
		else if(z < fi->fceil + srcRadius)
			applyCeiling = 1 - (z - fi->fceil)/srcRadius;
	}
	if(fi->lightFloor)
	{
		if(z > fi->ffloor)
			applyFloor = 1;
		else if(z > fi->ffloor - srcRadius)
			applyFloor = 1 - (fi->ffloor - z)/srcRadius;
	}

/*	if(clipLights && (applyCeiling || applyFloor))
	{
		num_vertices = Rend_SubsectorClipper(vertices, fi->subsector, 
			FIX2FLT(lum->thing->x), FIX2FLT(lum->thing->y), lum->radius);

		// Hmm? The light doesn't reach this subsector, we can move on.
		if(!num_vertices) return true;
	}*/

	//fi->poly->light = lum;

	if(applyCeiling > 0 && lum->ceilTex)
	{
		// Check that the height difference is tolerable.
		cdiff = fi->fceil - z;
		if(cdiff < 0) cdiff = 0;
		if(cdiff < lum->radius) 
		{
#if 0
			if(!lum->decorMap
				|| lum->decorMap != fi->ceilDecorMap
				/*|| !DL_IsTexUsed(ceilingLightLinks[fi->subIndex], 
					lum->decorMap)*/)
			{
				//if(!lum->decorMap || lum->decorMap != fi->ceilDecorMap)
				{
#endif
					// Calculate dynlight position. It may still be outside 
					// the bounding box the subsector.
					s[0] = -x + lum->radius;
					t[0] = y + lum->radius;
					s[1] = t[1] = 1.0f/(2 * lum->radius);

					// A dynamic light will be generated.
					dyn = DL_New(s, t);
					dyn->flags = 0;
					dyn->texture = lum->ceilTex;

#if 0
				}
				/*else
				{
					// Don't bother with texcoords, decormaps use the same 
					// ones as the main texture.
					dyn = DL_New(0, 0);
					dyn->flags = DYNF_PREGEN_DECOR;
					dyn->texture = lum->decorMap;
				}*/
#endif

				DL_ThingColor(lum, dyn->color, LUM_FACTOR(cdiff) 
					* applyCeiling);

				// Link to this ceiling's list.
				DL_Link(dyn, ceilingLightLinks, fi->subIndex);
			}
		//}
	}

	if(applyFloor > 0 && lum->floorTex)
	{
		fdiff = z - fi->ffloor;
		if(fdiff < 0) fdiff = 0;
		if(fdiff < lum->radius) 
		{
			// Calculate dynlight position. It may still be outside the
			// bounding box of the subsector.
			s[0] = -x + lum->radius;
			t[0] = y + lum->radius;
			s[1] = t[1] = 1.0f/(2 * lum->radius);

			// A dynamic light will be generated.
			dyn = DL_New(s, t);

			DL_ThingColor(lum, dyn->color, LUM_FACTOR(fdiff) * applyFloor);
			dyn->texture = lum->floorTex;
			dyn->flags = 0;

			// Link to this floor's list.
			DL_Link(dyn, floorLightLinks, GET_SUBSECTOR_IDX(fi->subsector));
		}
	}

	// If the light has no texture for the 'sides', there's no point in
	// going through the wall segments.
	if(!lum->tex) return true;

	// The wall segments.
	for(i = 0, seg = segs + SEGIDX(fi->subsector->firstline); 
		i < fi->subsector->linecount; i++, seg += SEGSIZE)
	{
		if(((seg_t*)seg)->linedef)	// "minisegs" have no linedefs.
			DL_ProcessWallSeg(lum, (seg_t*) seg, fi->subsector->sector);
	}

	// Is there a polyobj on board? Light it, too.
	if(fi->subsector->poly)
		for(i = 0; i < fi->subsector->poly->numsegs; i++)
		{
			DL_ProcessWallSeg(lum, fi->subsector->poly->segs[i], 
				fi->subsector->sector);
		}

	return true;
}

/*
 * Return the texture name of the decoration light map for the flat.
 * (Zero if no such texture exists.)
 */
DGLuint DL_GetFlatDecorLightMap(int pic)
{
	ded_decor_t *decor;

	if(pic == skyflatnum) return 0;	
	decor = R_GetFlat(pic)->decoration;
	return decor? decor->pregen_lightmap : 0;
}

/*
 * Process dynamic lights for the specified subsector.
 */
void DL_ProcessSubsector(subsector_t *ssec)
{
	int i;
	byte *seg;
	flatitervars_t fi;
	lumcontact_t *con;
	sector_t *sect = ssec->sector;

	// First make sure we know which lumobjs are contacting us.
	DL_SpreadBlocks(ssec);

	fi.subsector = ssec;
	fi.subIndex = GET_SUBSECTOR_IDX(ssec);
	fi.fceil = SECT_CEIL(sect);
	fi.ffloor = SECT_FLOOR(sect);

#if 0
	// Are there pregenerated decoration lightmaps?
	fi.floorDecorMap = DL_GetFlatDecorLightMap(sect->floorpic);
	fi.ceilDecorMap = DL_GetFlatDecorLightMap(sect->ceilingpic);
#endif

	// Check if lighting can be skipped.
	fi.lightFloor = (sect->floorpic != skyflatnum);
	fi.lightCeiling = (sect->ceilingpic != skyflatnum);

	// View height might prevent us from seeing the lights.
	if(vy < fi.ffloor) fi.lightFloor = false;
	if(vy > fi.fceil) fi.lightCeiling = false;

/*	
	// Prepare the bounding box.
	box[BLEFT] = (int) (ssec->bbox[0].x - dlMaxRad) << FRACBITS;
	box[BTOP] = (int) (ssec->bbox[0].y - dlMaxRad) << FRACBITS;
	box[BRIGHT] = (int) (ssec->bbox[1].x + dlMaxRad) << FRACBITS;
	box[BBOTTOM] = (int) (ssec->bbox[1].y + dlMaxRad) << FRACBITS;

	DL_BoxIterator(box, &fi, DL_LightIteratorFunc);
*/

	// Process each lumobj contacting the subsector.
	for(con = subContacts[ GET_SUBSECTOR_IDX(ssec) ]; con; con = con->next)
		DL_LightIteratorFunc(con->lum, &fi);

	// Check glowing planes.
	if(useWallGlow && (R_FlatFlags(sect->floorpic) & TXF_GLOW 
		|| R_FlatFlags(sect->ceilingpic) & TXF_GLOW))
	{
		// The wall segments.
		for(i = 0, seg = segs + SEGIDX(ssec->firstline); 
			i < ssec->linecount; i++, seg += SEGSIZE)
		{
			if(((seg_t*)seg)->linedef)	// "minisegs" have no linedefs.
				DL_ProcessWallGlow((seg_t*)seg, sect);
		}
			
		// Is there a polyobj on board? Light it, too.
		if(ssec->poly)
		{
			for(i = 0; i < ssec->poly->numsegs; i++)
				DL_ProcessWallGlow(ssec->poly->segs[i], sect);
		}
	}
}

//===========================================================================
// DL_InitForNewFrame
//	Creates the dynlight links by removing everything and then linking
//	this frame's luminous objects.
//===========================================================================
void DL_InitForNewFrame()
{
	sector_t	*seciter;
	int			i;

	BEGIN_PROF( PROF_DYN_INIT_DEL );

	// Clear the dynlight lists, which are used to track the lights on
	// each surface of the map.
	DL_DeleteUsed();

	END_PROF( PROF_DYN_INIT_DEL );

	// The luminousList already contains lumobjs if there are any light
	// decorations in use.
	dlInited = true;

	BEGIN_PROF( PROF_DYN_INIT_ADD );

	for(i = 0; i < numsectors; i++)
	{
		mobj_t *iter;
		seciter = SECTOR_PTR(i);
		for(iter = seciter->thinglist; iter; iter = iter->snext)
		{
			iter->light = 0;
			DL_AddLuminous(iter);
		}
	}

	END_PROF( PROF_DYN_INIT_ADD );

	BEGIN_PROF( PROF_DYN_INIT_LINK );

	// Link the luminous objects into the blockmap.
	DL_LinkLuminous();

	END_PROF( PROF_DYN_INIT_LINK );
}

/*
 * Calls func for all luminous objects within the specified range from (x,y).
 * 'subsector' is the subsector in which (x,y) resides.
 */
boolean DL_RadiusIterator
	(subsector_t *subsector, fixed_t x, fixed_t y, fixed_t radius, 
	 boolean (*func)(lumobj_t*,fixed_t))
{
	lumcontact_t *con;
	fixed_t	dist;

	if(!subsector) return true;

	for(con = subContacts[ GET_SUBSECTOR_IDX(subsector) ]; con; 
		con = con->next)
	{
		dist = P_ApproxDistance(con->lum->thing->x - x, 
			con->lum->thing->y - y);
		
		if(dist <= radius && !func(con->lum, dist)) 
			return false;						
	}
	return true;
}

#if 0
/*
 * 'box' contains the coordinates of the top left and bottom right corners.
 */
boolean DL_BoxIterator
	(fixed_t box[4], void *ptr, boolean (*func)(lumobj_t*,void*))
{
	fixed_t		x, y;
	int			s, t, bbc[4];	// box block coordinates
	lumobj_t	*iter;
	boolean		accurate;
	
	bbc[BLEFT]   = X_TO_DLBX(box[BLEFT]);
	bbc[BRIGHT]  = X_TO_DLBX(box[BRIGHT]);
	bbc[BTOP]    = Y_TO_DLBY(box[BTOP]);
	bbc[BBOTTOM] = Y_TO_DLBY(box[BBOTTOM]);

	// Can we skip the whole test?
	if(bbc[BRIGHT] < 0 || bbc[BBOTTOM] < 0 || bbc[BLEFT] >= dlBlockWidth 
		|| bbc[BTOP] >= dlBlockHeight)
	{
		// No lights outside the map.
		return true;		
	}

	// Clip to box to fit inside the blockmap.
	if(bbc[BLEFT] < 0) bbc[BLEFT] = 0;
	if(bbc[BTOP] < 0) bbc[BTOP] = 0;
	if(bbc[BRIGHT] >= dlBlockWidth) bbc[BRIGHT] = dlBlockWidth-1;
	if(bbc[BBOTTOM] >= dlBlockHeight) bbc[BBOTTOM] = dlBlockHeight-1;
	
	// Now the box lies within valid blocks.

	// The most typical box spans 3x3 blocks (dlMaxRad is 128 => diameter 256).
	for(t = bbc[BTOP]; t <= bbc[BBOTTOM]; t++)
		for(s = bbc[BLEFT]; s <= bbc[BRIGHT]; s++)
		{
			// If the edge of the box is in this block, we must do an 
			// accurate check.
			accurate = (t==bbc[BTOP] || t==bbc[BBOTTOM] 
				|| s==bbc[BLEFT] || s==bbc[BRIGHT]);
			// Iterate the lights in this block.
			for(iter = *DLB_ROOT_DLBXY(s, t); iter; iter = iter->next)
			{
				if(accurate)
				{
					x = iter->thing->x;
					y = iter->thing->y;
					if(x >= box[BLEFT] && y >= box[BTOP] 
						&& x <= box[BRIGHT] && y <= box[BBOTTOM])
					{
						if(!func(iter, ptr)) return false;
					}
				}
				else if(!func(iter, ptr)) return false;
			}
		}
	return true;
}
#endif
