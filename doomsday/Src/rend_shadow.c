
//**************************************************************************
//**
//** REND_SHADOW.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int		useShadows = true;
int		shadowMaxRad = 80;
int		shadowMaxDist = 1000;
float	shadowFactor = 0.5f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Rend_ProcessThingShadow
//===========================================================================
void Rend_ProcessThingShadow(mobj_t *mo)
{
	float		height, moh, halfmoh, color, pos[2], z;
	sector_t	*sec = mo->subsector->sector;
	int			radius, i;
	rendpoly_t	poly;

	// Is this too far?
	pos[VX] = FIX2FLT( mo->x );
	pos[VY] = FIX2FLT( mo->y );
	if(Rend_PointDist2D(pos) > shadowMaxDist) return;

	// Apply a Short Range Visual Offset?
	if(r_use_srvo && mo->state && mo->tics >= 0)
	{
		float mul = mo->tics / (float) mo->state->tics;				
		pos[VX] += FIX2FLT(mo->srvo[VX] << 8) * mul;
		pos[VY] += FIX2FLT(mo->srvo[VY] << 8) * mul;
	}

	// Check the height.
	z = FIX2FLT( mo->floorz );
	height = FIX2FLT( mo->z - mo->floorz );
	moh = FIX2FLT( mo->height );
	if(!moh) moh = 1;
	if(height > moh) return;	// Too far.
	if(mo->z + mo->height < mo->floorz) return;
	
	// Calculate the strength of the shadow.
	color = shadowFactor 
		* sec->lightlevel/255.0f
		* (1 - mo->translucency/255.0f);
	halfmoh = moh/2;
	if(height > halfmoh) color *= 1 - (height - halfmoh)/(moh - halfmoh);
	if(whitefog) color /= 2;
	if(color <= 0) return;		// Can't be seen.
	if(color > 1) color = 1;

	// Calculate the radius of the shadow.
	radius = R_VisualRadius(mo);
	if(!radius) return;
	if(radius > shadowMaxRad) radius = shadowMaxRad;

	// Prepare the poly.
	memset(&poly, 0, sizeof(poly));
	poly.type = rp_flat;
	poly.flags = RPF_SHADOW;
	poly.light = (lumobj_t*) (radius*2);
/*	gl.TexCoord2f((prim->texoffx - vtx->pos[VX])/prim->shadowradius,
				(prim->texoffy - vtx->pos[VY])/prim->shadowradius);*/
	poly.texoffx = pos[VX] + radius;
	poly.texoffy = pos[VY] + radius;
	poly.top = FIX2FLT( mo->floorz ) + 0.1f;

	poly.numvertices = 4;
	poly.vertices[0].pos[VX] = pos[VX] - radius;
	poly.vertices[0].pos[VY] = pos[VY] + radius;
	poly.vertices[1].pos[VX] = pos[VX] + radius;
	poly.vertices[1].pos[VY] = pos[VY] + radius;
	poly.vertices[2].pos[VX] = pos[VX] + radius;
	poly.vertices[2].pos[VY] = pos[VY] - radius;
	poly.vertices[3].pos[VX] = pos[VX] - radius;
	poly.vertices[3].pos[VY] = pos[VY] - radius;
	for(i = 0; i < 4; i++)
		memset(poly.vertices[i].color.rgb, color*255, 3);

	RL_AddPoly(&poly);
}

//===========================================================================
// Rend_RenderShadows
//===========================================================================
void Rend_RenderShadows(void)
{
	sector_t *sec;
	mobj_t *mo;
	int i;

	if(!useShadows) return;

	// Check all mobjs in all visible sectors.
	for(i = 0; i < numsectors; i++)
	{
		if(!(secinfo[i].flags & SIF_VISIBLE)) continue;
		sec = SECTOR_PTR(i);
		for(mo = sec->thinglist; mo; mo = mo->snext)
		{
			// Should this mobj have a shadow?
			if(mo->frame & FF_FULLBRIGHT 
				|| mo->ddflags & DDMF_DONTDRAW
				|| mo->ddflags & DDMF_ALWAYSLIT) continue;
			Rend_ProcessThingShadow(mo);	
		}
	}
}

#if 0
//===========================================================================
// Rend_ProcessThingShadow
//===========================================================================
boolean Rend_ProcessThingShadow(mobj_t *mo, subsector_t *sub)
{
	sector_t	*sec = sub->sector;
	float		height, moh, halfmoh, color, pos[2], z;
	int			radius;
	int			num_vertices, i;
	fvertex_t	vertices[MAX_POLY_SIDES];
	rendpoly_t	poly;

	// Is this too far?
	pos[VX] = FIX2FLT( mo->x );
	pos[VY] = FIX2FLT( mo->y );
	if(Rend_PointDist2D(pos) > shadowMaxDist) return true;

	// Check the height.
	z = FIX2FLT( mo->z );
	height = FIX2FLT( mo->z - sec->floorheight );
	moh = FIX2FLT( mo->height );
	if(!moh) moh = 1;
	if(height > moh) return true;	// Too far.
	if(mo->z + mo->height < sec->floorheight) return true;
	
	// Calculate the strength of the shadow.
	color = shadowFactor * sec->lightlevel/255.0f;
	halfmoh = moh/2;
	if(height > halfmoh) color *= 1 - (height - halfmoh)/(moh - halfmoh);
	if(color <= 0) return true;		// Can't be seen.
	if(color > 1) color = 1;

	// Calculate the radius of the shadow.
	radius = mo->radius >> FRACBITS;
	if(radius > shadowMaxRad) radius = shadowMaxRad;

	// Prepare the poly.
	memset(&poly, 0, sizeof(poly));
	poly.type = rp_flat;
	poly.flags = RPF_SHADOW;
	poly.light = (lumobj_t*) radius;
	poly.texoffx = pos[VX] - radius;
	poly.texoffy = pos[VY] - radius;
	poly.top = FIX2FLT( sec->floorheight ) + 0.1f;

	poly.numvertices = 4;
	poly.vertices[0].x = pos[VX] + radius;
	poly.vertices[0].y = pos[VY] + radius;
	poly.vertices[1].x = pos[VX] - radius;
	poly.vertices[1].y = pos[VY] + radius;
	poly.vertices[2].x = pos[VX] - radius;
	poly.vertices[2].y = pos[VY] - radius;
	poly.vertices[3].x = pos[VX] + radius;
	poly.vertices[3].y = pos[VY] - radius;
	for(i = 0; i < 4; i++)
		memset(poly.vertices[i].color.rgb, color*255, 3);

	RL_AddPoly(&poly);

	// More shadows, please!
	return true;
}

//===========================================================================
// Rend_SubsectorShadows
//	Create shadow polygons for objects inside the subsector.
//	The objects are searched using the blockmap, which means objects that
//	aren't blocklinked can't have shadows. 
//===========================================================================
void Rend_SubsectorShadows(subsector_t *sub)
{
	int	i, k, lowx, lowy, hix, hiy;

	// Calculate the area of the blockmap to iterate for things.
	lowx = (((int)sub->bbox[0].x - shadowMaxRad) << FRACBITS) - bmaporgx;
	lowy = (((int)sub->bbox[0].y - shadowMaxRad) << FRACBITS) - bmaporgx;
	hix = (((int)sub->bbox[1].x + shadowMaxRad) << FRACBITS) - bmaporgy;
	hiy = (((int)sub->bbox[1].y + shadowMaxRad) << FRACBITS) - bmaporgy;
	lowx >>= MAPBLOCKSHIFT;
	lowy >>= MAPBLOCKSHIFT;
	hix >>= MAPBLOCKSHIFT;
	hiy >>= MAPBLOCKSHIFT;

	for(k = lowy; k <= hiy; k++)
		for(i = lowx; i <= hix; i++)
			P_BlockThingsIterator(i, k, Rend_ProcessThingShadow, sub);
}
#endif 
