
//**************************************************************************
//**
//** REND_DYN.C
//**
//** Dynamic lights.
//**
//**************************************************************************

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

enum { CLIP_TOP, CLIP_BOTTOM, CLIP_LEFT, CLIP_RIGHT };

#define ZMOD_FLAT		0 //.05f	// How much difference?
#define ZMOD_WALL		0//.15f		
#define DISTFACTOR		1		// Really...

#define X_TO_DLBX(cx)			( ((cx) - dlBlockOrig.x) >> (FRACBITS+7) )
#define Y_TO_DLBY(cy)			( ((cy) - dlBlockOrig.y) >> (FRACBITS+7) )
#define DLB_ROOT_DLBXY(bx, by)	(dlBlockLinks + bx + by*dlBlockWidth)

// Wall Surface Present flags (for DL_ProcessWallSeg).
#define WSP_MIDDLE		0x1
#define WSP_TOP			0x2
#define WSP_BOTTOM		0x4

#define LUM_FACTOR(dist)	(1.5f - 1.5f*(dist)/lum->radius)

// TYPES -------------------------------------------------------------------

typedef struct
{
	boolean		lightFloor, lightCeiling;
	rendpoly_t	*poly;
	subsector_t	*subsector;
	float		fceil, ffloor;
} flatitervars_t;

typedef struct
{
	rendpoly_t	*poly;
	float		*v1, *v2;
} wallitervars_t;

typedef struct
{
	float color[3];
	float size;
	float xoffset, yoffset;
} lightconfig_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

fvertex_t *edgeClipper(int *numpoints, fvertex_t *points, int numclippers, fdivline_t *clippers);
int Rend_MidTexturePos(rendpoly_t *quad, float tcyoff, boolean lower_unpeg);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int			useDynLights;
extern subsector_t	*currentssec;
extern int			whitefog;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		dlInited = false;
int			useDynLights = true, dlBlend = 0;
float		dlFactor = 0.7f;	// was 0.6f
int			useWallGlow = true;
int			glowHeight = 100;
lumobj_t	*luminousList = 0;
int			numLuminous = 0, maxLuminous = 0;
int			dlMaxRad = 256; // Dynamic lights maximum radius.
float		dlRadFactor = 3;
int			maxDynLights = 0;
int			clipLights = 1;
float		dlContract = 0.02f; // Almost unnoticeable... a hack, though.
int			rend_info_lums = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

lumobj_t	**dlBlockLinks = 0;
vertex_t	dlBlockOrig;
int			dlBlockWidth, dlBlockHeight;	// In 128 blocks.
lumobj_t	**dlSubLinks = 0;		

// CODE --------------------------------------------------------------------

void DL_ThingRadius(lumobj_t *lum, lightconfig_t *cf)
{
	lum->radius = cf->size * 40 * dlRadFactor;
	
	// Don't make a too small or too large light.
	if(lum->radius < 32) lum->radius = 32;
	if(lum->radius > dlMaxRad) lum->radius = dlMaxRad;

	lum->flaresize = cf->size * 60 * (50 + haloSize)/100.0f;
	if(lum->flaresize < 8) lum->flaresize = 8;
}

void DL_ThingColor(lumobj_t *lum, DGLubyte *outRGB, float light)
{
	int		i;
	extern	int whitefog;

	if(light < 0) light = 0;
	if(light > 1) light = 1;
	light *= dlFactor;
	if(whitefog) light *= .5f; // Would be too much.
	// Multiply with the light color.
	for(i=0; i<3; i++)
	{
		outRGB[i] = (DGLubyte) (light * lum->rgb[i]);
		//outRGB[i] += (255 - outRGB[i])/4; // more brightness, less color...
	}
}

//===========================================================================
// DL_InitBlockMap
//===========================================================================
void DL_InitBlockMap(void)
{
	byte		*ptr;
	vertex_t	min, max;
	int			i, x, y;

	// First initialize the subsector links (root pointers).
	dlSubLinks = Z_Calloc(sizeof(lumobj_t*) * numsubsectors, PU_LEVEL, 0);

	// Then the blocklinks.
	memcpy(&min, vertexes, sizeof(min));
	memcpy(&max, vertexes, sizeof(max));
	for(i=1, ptr=vertexes+VTXSIZE; i<numvertexes; i++, ptr += VTXSIZE)
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
}

// Set dlq.top and .bottom before calling this.
static void DL_FinalizeSurface(rendpoly_t *dlq, lumobj_t *lum)
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
}

//===========================================================================
// DL_ProcessWallSeg
//	The front sector must be given because of polyobjs.
//===========================================================================
void DL_ProcessWallSeg(lumobj_t *lum, seg_t *seg, sector_t *frontsec)
{
	int			c, present = 0;
	sector_t	*backsec = seg->backsector;
	side_t		*sdef = seg->sidedef;
	float		dist, pntLight[2], uvecWall[2], fceil, ffloor, bceil, bfloor;
	rendpoly_t	dlq;

	fceil = SECT_CEIL(frontsec);
	ffloor = SECT_FLOOR(frontsec);
	if(backsec)
	{
		bceil = SECT_CEIL(backsec);
		bfloor = SECT_FLOOR(backsec);
	}

	// Let's begin with an analysis of the present surfaces.	
	if(sdef->midtexture)
	{
		present |= WSP_MIDDLE;
		if(backsec)
		{
			// Check the middle texture's mask status.
			GL_GetTextureInfo(sdef->midtexture); 
			if(texmask)
			{
				// We can't light masked textures.
				// FIXME: Use vertex lighting.
				present &= ~WSP_MIDDLE;
			}
		}
	}
	// Is there a top wall segment?
	if(backsec)
	{
		if(fceil > bceil && sdef->toptexture)
		{
			present |= WSP_TOP;
		}
		// Is there a lower wall segment? 
		if(ffloor < bfloor && sdef->bottomtexture)
		{
			present |= WSP_BOTTOM;
		}
		// The top texture can't be present when front and back sectors
		// both have the sky ceiling.
		if(frontsec->ceilingpic == skyflatnum 
			&& backsec->ceilingpic == skyflatnum)
		{
			present &= ~WSP_TOP;
		}
	}

	// There are no surfaces to light!
	if(!present) return;

	dlq.type = rp_quad;
	dlq.numvertices = 2;
	dlq.vertices[0].pos[VX] = FIX2FLT(seg->v1->x);
	dlq.vertices[0].pos[VY] = FIX2FLT(seg->v1->y);
	dlq.vertices[1].pos[VX] = FIX2FLT(seg->v2->x);
	dlq.vertices[1].pos[VY] = FIX2FLT(seg->v2->y);

	if(!Rend_SegFacingDir(dlq.vertices[0].pos, dlq.vertices[1].pos)) 
		return; // Facing the other way.

	pntLight[VX] = FIX2FLT(lum->thing->x);
	pntLight[VY] = FIX2FLT(lum->thing->y);
	
	dist = ((dlq.vertices[0].pos[VY] - pntLight[VY]) * 
		(dlq.vertices[1].pos[VX] - dlq.vertices[0].pos[VX]) - 
		(dlq.vertices[0].pos[VX] - pntLight[VX]) * 
		(dlq.vertices[1].pos[VY] - dlq.vertices[0].pos[VY])) / seg->length;

	// Is it close enough and on the right side?
	if(dist < 0 || dist > lum->radius) return; // Nope.

	// Initialize the poly used for all of the lights.
	dlq.flags = RPF_LIGHT; // This is a light polygon.
	dlq.texw = lum->radius*2;
	dlq.texh = dlq.texw/DYN_ASPECT;
	dlq.length = seg->length;
	
	// Do a scalar projection for the offset.
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

	// A hack...
	if(dlContract)
	{
		dlq.vertices[0].pos[VX] += uvecWall[VX] * dlContract;
		dlq.vertices[0].pos[VY] += uvecWall[VY] * dlContract;
		dlq.vertices[1].pos[VX] -= uvecWall[VX] * dlContract;
		dlq.vertices[1].pos[VY] -= uvecWall[VY] * dlContract;
	}

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

	/*fceil = FIX2FLT(frontsec->ceilingheight);// + frontsec->skyfix;
	ffloor = FIX2FLT(frontsec->floorheight);
	if(backsec)
	{
		bceil = FIX2FLT(backsec->ceilingheight);// + backsec->skyfix;
		bfloor = FIX2FLT(backsec->floorheight);
	}*/

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
}

void DL_SetupGlowQuads(seg_t *seg, fixed_t segtop, fixed_t segbottom, 
					   boolean glow_floor, boolean glow_ceil)
{
	rendpoly_t glow;
	int i, g;
	fixed_t top, bottom, glowhgt = glowHeight << FRACBITS, limit;
	sector_t *sect = seg->sidedef->sector;
	
	// Check the heights.
	if(segtop <= segbottom) return;	// No height.

	for(g = 0; g < 2; g++)
	{
		// Only do what's told.
		if(g == 0 && !glow_ceil || g == 1 && !glow_floor) continue;

		top = segtop;
		bottom = segbottom;

		// Clip.						
		if(g == 0) // Ceiling glow.
		{
			limit = sect->ceilingheight - glowhgt;
			// Too low?
			if(top < limit) continue;
			// Clip the bottom.
			if(bottom < limit) bottom = limit;
		}
		else // Floor glow.
		{
			limit = sect->floorheight + glowhgt;
			// Too high?
			if(bottom > limit) continue;
			// Clip the top.
			if(top > limit) top = limit;
		}

		glow.type = rp_quad;
		glow.length = seg->length;
		glow.flags = RPF_WALL_GLOW | RPF_LIGHT;
		glow.vertices[0].pos[VX] = FIX2FLT(seg->v1->x);
		glow.vertices[0].pos[VY] = FIX2FLT(seg->v1->y);
		glow.vertices[1].pos[VX] = FIX2FLT(seg->v2->x);
		glow.vertices[1].pos[VY] = FIX2FLT(seg->v2->y);
		if(dlContract && glow.length)
		{
			float vec[2];
			for(i = 0; i < 2; i++)
			{
				vec[i] = (glow.vertices[1].pos[i] 
					- glow.vertices[0].pos[i]) / glow.length;
			}
			glow.vertices[0].pos[VX] += vec[VX] * dlContract;
			glow.vertices[0].pos[VY] += vec[VY] * dlContract;
			glow.vertices[1].pos[VX] -= vec[VX] * dlContract;
			glow.vertices[1].pos[VY] -= vec[VY] * dlContract;
		}
		GL_GetFlatColor(g? sect->floorpic : sect->ceilingpic,
			glow.vertices[0].color.rgb);
		for(i = 0; i < 3; i++)
		{
			glow.vertices[0].color.rgb[i] *= dlFactor;
			if(whitefog) glow.vertices[0].color.rgb[i] *= .4f;
		}
		glow.top = FIX2FLT(top) - dlContract;
		glow.bottom = FIX2FLT(bottom) + dlContract;
		// texoffx will store the height.
		glow.texoffx = g? -glowHeight : glowHeight;	// Flip for floor glow.
		//glow.texoffx = glow.texoffy = 0;
		if(g == 0) // Ceiling glow.
		{
			glow.texoffy = FIX2FLT(sect->ceilingheight - top);
		}
		else // Floor glow.
		{
			// We need to set texoffy so that the bottom of the texture
			// meets bottom height.
			glow.texoffy = FIX2FLT(bottom - sect->floorheight);
		}
		RL_AddPoly(&glow);
		// Mark the segment for dlight rendering.
		seg->flags |= DDSEGF_DLIGHT;
	}
}

void DL_ProcessWallGlow(seg_t *seg, sector_t *sect)
{
	boolean do_floor = (R_FlatFlags(sect->floorpic) & TXF_GLOW) != 0;
	boolean do_ceil = (R_FlatFlags(sect->ceilingpic) & TXF_GLOW) != 0;
	sector_t *back = seg->backsector;
	side_t *sdef = seg->sidedef;
	fixed_t opentop, openbottom, top, bottom;
	float v1[2], v2[2];

	// Check if this segment is actually facing our way.
	v1[VX] = FIX2FLT(seg->v1->x);
	v1[VY] = FIX2FLT(seg->v1->y);
	v2[VX] = FIX2FLT(seg->v2->x);
	v2[VY] = FIX2FLT(seg->v2->y);
	if(!Rend_SegFacingDir(v1, v2)) return;	// Nope...

	// Determine which portions of the segment get lit.
	if(!back)
	{
		// One sided.
		// Process floor.
		DL_SetupGlowQuads(seg, sect->ceilingheight, sect->floorheight, 
			do_floor, do_ceil);
	}
	else
	{
#define MIN(x,y,z)		((x)<(y)? ((z)=(y)),(x) : ((z)=(x)),(y))
#define MAX(x,y,z)		((x)>(y)? ((z)=(y)),(x) : ((z)=(x)),(y))
		// Two-sided.
		opentop = MIN(sect->ceilingheight, back->ceilingheight, top);
		openbottom = MAX(sect->floorheight, back->floorheight, bottom);

		// The glow can only be visible in the front sector's height range.
		if(top > sect->ceilingheight) top = sect->ceilingheight;
		if(opentop > sect->ceilingheight) opentop = sect->ceilingheight;
		if(openbottom > sect->ceilingheight) openbottom = sect->ceilingheight;
		if(bottom > sect->ceilingheight) bottom = sect->ceilingheight;
		if(top < sect->floorheight) top = sect->floorheight;
		if(opentop < sect->floorheight) opentop = sect->floorheight;
		if(openbottom < sect->floorheight) openbottom = sect->floorheight;
		if(bottom < sect->floorheight) bottom = sect->floorheight;
		
		// Is there a middle texture?
		if(sdef->midtexture)
		{
			//GL_PrepareTexture(sdef->midtexture);
			GL_GetTextureInfo(sdef->midtexture);
			if(!texmask)
				DL_SetupGlowQuads(seg, opentop, openbottom, do_floor, do_ceil);
		}
		if(sdef->toptexture)
			DL_SetupGlowQuads(seg, top, opentop, do_floor, do_ceil);
		if(sdef->bottomtexture)
			DL_SetupGlowQuads(seg, openbottom, bottom, do_floor, do_ceil);
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
	if(!index) return NULL;
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
		lum->xoff = cf.xoffset - spritelumps[lump].width/2.0f;

		// Does the thing have an active light definition?
		if(thing->state && thing->state->light)
		{
			def = (ded_light_t*) thing->state->light;
			if(def->size) cf.size = def->size;
			if(def->xoffset) 
			{
				// Set the x offset here.
				lum->xoff = cf.xoffset = def->xoffset;
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
			for(i=0; i<3; i++) 
				lum->rgb[i] = (byte) (255 * def->color[i]);
		}
		else
		{
			// Use the sprite's (amplified) color.
			GL_GetSpriteColor(lum->patch, lum->rgb);
		}

		// Approximate the distance in 3D.
		lum->distance = P_ApproxDistance3(thing->x - viewx, 
			thing->y - viewy, thing->z - viewz);

		// Is there a model definition?
		R_CheckModelFor(thing, &mf, &nextmf);
		if(mf && useModels)
			lum->xyscale = MAX_OF(mf->scale[VX], mf->scale[VZ]);
		else
			lum->xyscale = 1;
	}
}

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
		lum->ssnext = *root;
		*root = lum;
	}
}

void intersectionVertex(fvertex_t *out, fvertex_t *a, fvertex_t *b,
						boolean horizontal, float boundary)
{
	float dx = b->x-a->x, dy = b->y-a->y;

	if(horizontal)
	{
		out->x = a->x + (boundary - a->y) / dy * dx;
		out->y = boundary;
	}
	else
	{
		out->x = boundary;
		out->y = a->y + (boundary - a->x) / dx * dy;
	}
}

//===========================================================================
// Rend_SubsectorClipper
//	Clip the subsector to the rectangle. Returns the number of vertices in 
//	the out buffer.
//===========================================================================
int Rend_SubsectorClipper
	(fvertex_t *out, subsector_t *sub, float x, float y, float radius)
{
	int			i, clip, aidx, bidx, num;
	boolean		clip_needed[4], sides[MAX_POLY_SIDES];
	fvertex_t	inverts[MAX_POLY_SIDES];
	fvertex_t	verts[MAX_POLY_SIDES]; // This many vertices, max!
	fvertex_t	*vptr = verts, *a, *b;
	fvertex_t	light[4];

	light[0].x = light[3].x = x - radius;
	light[0].y = light[1].y = y - radius;
	light[1].x = light[2].x = x + radius;
	light[2].y = light[3].y = y + radius;

	if(sub->bbox[0].x >= light[0].x && sub->bbox[0].y >= light[0].y 
		&& sub->bbox[1].x <= light[2].x && sub->bbox[1].y <= light[2].y)
	{
		// No clipping needed, the whole subsector is inside the light.
		memcpy(out, sub->verts, sizeof(fvertex_t) * sub->numverts);
		return sub->numverts;
	}

	// See which clips are needed.
	clip_needed[CLIP_LEFT] = light[0].x > sub->bbox[0].x;
	clip_needed[CLIP_TOP] = light[0].y > sub->bbox[0].y;
	clip_needed[CLIP_RIGHT] = light[2].x < sub->bbox[1].x;
	clip_needed[CLIP_BOTTOM] = light[2].y < sub->bbox[1].y;

	// Prepare the vertex array.
	memcpy(inverts, sub->verts, sizeof(fvertex_t) * sub->numverts);
	num = sub->numverts;

	// Horizontal clips first.
	for(clip = CLIP_TOP; clip <= CLIP_RIGHT; clip++)
	{
		if(!clip_needed[clip]) continue;

		// First determine on which side each vertex is.
		for(i = 0; i < num; i++)
		{
			switch(clip)
			{
			case CLIP_TOP:
				sides[i] = inverts[i].y >= light[0].y;
				break;
			case CLIP_BOTTOM:
				sides[i] = inverts[i].y <= light[2].y;
				break;
			case CLIP_LEFT:
				sides[i] = inverts[i].x >= light[0].x;
				break;
			case CLIP_RIGHT:
				sides[i] = inverts[i].x <= light[2].x;
			}
		}
		for(i = 0; i < num; i++)
		{
			a = inverts + (aidx = (i==0? num-1 : i-1));
			b = inverts + (bidx = i);
			if(sides[aidx] && sides[bidx])
			{
				// This edge is completely within the light, no need to clip it.
				memcpy(vptr++, a, sizeof(*a));
			}
			else if(sides[aidx])
			{	
				// Only the start point is inside.
				memcpy(vptr++, a, sizeof(*a));
				// Add an intersection vertex.
				intersectionVertex(vptr++, a, b, clip==CLIP_TOP || clip==CLIP_BOTTOM, 
					clip==CLIP_TOP? light[0].y
					: clip==CLIP_BOTTOM? light[2].y
					: clip==CLIP_LEFT? light[0].x
					: light[2].x);
			}
			else if(sides[bidx])
			{
				// First an intersection vertex.
				intersectionVertex(vptr++, a, b, clip==CLIP_TOP || clip==CLIP_BOTTOM, 
					clip==CLIP_TOP? light[0].y
					: clip==CLIP_BOTTOM? light[2].y
					: clip==CLIP_LEFT? light[0].x
					: light[2].x);
				// Then the end vertex, which is inside.
				memcpy(vptr++, b, sizeof(*b));
			}
		}
		// Prepare for another round.
		num = vptr - verts;
		vptr = verts;
		memcpy(inverts, verts, sizeof(fvertex_t) * num);
	}
	memcpy(out, inverts, sizeof(fvertex_t) * num);
	return num;
}

//===========================================================================
// DL_LightIteratorFunc
//===========================================================================
boolean DL_LightIteratorFunc(lumobj_t *lum, void *ptr) 
{
	int				i;
	byte			*seg;
	flatitervars_t	*fi = ptr;
	float			x = FIX2FLT(lum->thing->x), y = FIX2FLT(lum->thing->y);
	float			z = FIX2FLT(lum->thing->z), cdiff, fdiff;	
	float			applyCeiling, applyFloor;	// Is the light on the right side?
	int				num_vertices;
	fvertex_t		vertices[MAX_POLY_SIDES];
	float			srcRadius;

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

	if(clipLights && (applyCeiling || applyFloor))
	{
		num_vertices = Rend_SubsectorClipper(vertices, fi->subsector, 
			FIX2FLT(lum->thing->x), FIX2FLT(lum->thing->y), lum->radius);

		// Hmm? The light doesn't reach this subsector, we can move on.
		if(!num_vertices) return true;
	}

	fi->poly->light = lum;

	if(applyCeiling > 0)
	{
		// Check that the height difference is tolerable.
		cdiff = fi->fceil - z;
		if(cdiff < 0) cdiff = 0;
		if(cdiff < lum->radius) 
		{
			fi->poly->top = fi->fceil - ZMOD_FLAT;
			DL_ThingColor(lum, fi->poly->vertices[0].color.rgb, 
				LUM_FACTOR(cdiff) * applyCeiling);
			// We can add the light poly.
			if(clipLights)
				RL_PrepareFlat(fi->poly, num_vertices, vertices, RLPF_REVERSE, 0);
			else
				RL_PrepareFlat(fi->poly, 0, 0, RLPF_REVERSE, fi->subsector);
			RL_AddPoly(fi->poly);
			// Mark the ceiling for dlighting.
			fi->subsector->flags |= DDSUBF_DLIGHT_CEILING;
		}
	}
	if(applyFloor > 0)
	{
		//fdiff = fabs(fi->ffloor - z);
		fdiff = z - fi->ffloor;
		if(fdiff < 0) fdiff = 0;
		if(fdiff < lum->radius) 
		{
			fi->poly->top = fi->ffloor + ZMOD_FLAT;
			DL_ThingColor(lum, fi->poly->vertices[0].color.rgb, 
				LUM_FACTOR(fdiff) * applyFloor);
			// Add the light quads.
			if(clipLights)
				RL_PrepareFlat(fi->poly, num_vertices, vertices, RLPF_NORMAL, 0);
			else
				RL_PrepareFlat(fi->poly, 0, 0, RLPF_NORMAL, fi->subsector);
			RL_AddPoly(fi->poly);
			// Mark the ceiling for dlighting.
			fi->subsector->flags |= DDSUBF_DLIGHT_FLOOR;
		}
	}

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

void DL_ProcessSubsector(rendpoly_t *poly, subsector_t *ssec)
{
	sector_t		*sect = ssec->sector;
	flatitervars_t	fi;
	fixed_t			box[4];
	int				i;
	byte			*seg;

	fi.poly = poly;
	fi.subsector = ssec;
	fi.fceil = SECT_CEIL(sect);
	fi.ffloor = SECT_FLOOR(sect);

	// Check if lighting can be skipped.
	fi.lightFloor = (sect->floorpic != skyflatnum);
	fi.lightCeiling = (sect->ceilingpic != skyflatnum);

	// View height might prevent us from seeing the lights.
	if(viewz < sect->floorheight) fi.lightFloor = false;
	if(viewz > sect->ceilingheight) fi.lightCeiling = false;

	poly->flags = RPF_LIGHT;
	
	// Prepare the bounding box.
	box[BLEFT] = (int) (ssec->bbox[0].x - dlMaxRad) << FRACBITS;
	box[BTOP] = (int) (ssec->bbox[0].y - dlMaxRad) << FRACBITS;
	box[BRIGHT] = (int) (ssec->bbox[1].x + dlMaxRad) << FRACBITS;
	box[BBOTTOM] = (int) (ssec->bbox[1].y + dlMaxRad) << FRACBITS;

	DL_BoxIterator(box, &fi, DL_LightIteratorFunc);

	// Check glowing planes.
	if(useWallGlow && (R_FlatFlags(sect->floorpic) & TXF_GLOW 
		|| R_FlatFlags(sect->ceilingpic) & TXF_GLOW))
	{
		// The wall segments.
		for(i=0, seg=segs+SEGIDX(ssec->firstline); 
			i<ssec->linecount; i++, seg+=SEGSIZE)
		{
			if(((seg_t*)seg)->linedef)	// "minisegs" have no linedefs.
				DL_ProcessWallGlow((seg_t*)seg, sect);
		}
			
		// Is there a polyobj on board? Light it, too.
		if(ssec->poly)
		{
			for(i=0; i<ssec->poly->numsegs; i++)
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
	int			i, done = false;

	// The luminousList already contains lumobjs is there are any light
	// decorations in use.

	dlInited = true;

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

/*	if(maxDynLights)
	{
		// Sort the lumobjs based on their distance.
		qsort(luminousList, numLuminous, sizeof(lumobj_t), lumobjSorter);
		// Force the maximum.
		if(numLuminous > maxDynLights) numLuminous = maxDynLights;
	}*/
	// Link the luminous objects into the blockmap.
	DL_LinkLuminous();
}

// Calls func for all luminous objects within the specified range from (x,y).
boolean DL_RadiusIterator(fixed_t x, fixed_t y, fixed_t radius, 
						  boolean (*func)(lumobj_t*,fixed_t))
{
	int		s, t, bx1, by1, bx2, by2;
	fixed_t	dist;
	lumobj_t *iter;
	
	bx1 = X_TO_DLBX(x-radius);
	bx2 = X_TO_DLBX(x+radius);
	by1 = Y_TO_DLBY(y-radius);
	by2 = Y_TO_DLBY(y+radius);
	// Walk through the blocks.
	for(t=by1; t<=by2; t++)
		for(s=bx1; s<=bx2; s++)
		{
			// We can't go outside the blockmap.
			if(s < 0 || t < 0 || s >= dlBlockWidth || t >= dlBlockHeight) continue;
			for(iter = *DLB_ROOT_DLBXY(s, t); iter; iter = iter->next)
			{
				dist = P_ApproxDistance(iter->thing->x - x, iter->thing->y - y);
				if(dist <= radius)
				{
					if(!func(iter, dist)) return false;						
				}
			}
		}
	return true;
}

// Box contains the coordinates of the top left and bottom right corners.
boolean DL_BoxIterator(fixed_t box[4], void *ptr, boolean (*func)(lumobj_t*,void*))
{
	register fixed_t x, y;
	int			s, t, bbc[4];	// box block coordinates
	lumobj_t	*iter;
	boolean		accurate;
	
	bbc[BLEFT] = X_TO_DLBX(box[BLEFT]);
	bbc[BRIGHT] = X_TO_DLBX(box[BRIGHT]);
	bbc[BTOP] = Y_TO_DLBY(box[BTOP]);
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
			// If the edge of the box is in this block, we must do an accurate check.
			accurate = (t==bbc[BTOP] || t==bbc[BBOTTOM] || s==bbc[BLEFT] || s==bbc[BRIGHT]);
			// Iterate the lights in this block.
			for(iter = *DLB_ROOT_DLBXY(s, t); iter; iter = iter->next)
			{
				if(accurate)
				{
					x = iter->thing->x;
					y = iter->thing->y;
					if(x >= box[BLEFT] && y >= box[BTOP] && x <= box[BRIGHT] && y <= box[BBOTTOM])
					{
						if(!func(iter, ptr)) return false;
					}
				}
				else if(!func(iter, ptr)) return false;
			}
		}
	return true;
}