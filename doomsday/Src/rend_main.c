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
 * rend_main.c: Rendering Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
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
	PROF_REND_MAP,
	PROF_REND_INIT,
	PROF_REND_INIT_LIGHTS,
	PROF_REND_NODES,
	PROF_REND_SHADOWS,
	
	PROF_REND_SUB_LIGHTS,
	PROF_REND_SUB_OCCLUDE,
	PROF_REND_SUB_ADD_SPRITES,
	PROF_REND_SUB_SEGS,
	PROF_REND_WALLSEG_1,
	PROF_REND_WALLSEG_2,
	PROF_REND_WALLSEG_CADD,
	PROF_REND_WALLSEG_3,
	PROF_REND_WALLSEG_4,
	PROF_REND_WALLSEG_5,
	PROF_REND_SUB_PLANE_1,
	PROF_REND_PREP_FLAT,
	PROF_REND_SUB_PLANE_2,
	PROF_REND_SUB_PLANES
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void Rend_ProjectionMatrix();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int	useDynLights, translucentIceCorpse;
extern int	skyhemispheres, haloMode;
extern int	dlMaxRad;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		useFog = false;		// Is the fog in use?
byte		fogColor[4];
float		fieldOfView = 90.0f;
float		maxLightDist = 1024;
boolean		smoothTexAnim = true;

float		vx, vy, vz, vang, vpitch;
float		viewsidex, viewsidey;	

boolean		willRenderSprites = true, freezeRLs = false;
int			missileBlend = 1;
int			litSprites = 1;
int			r_ambient = 0;

int viewpw, viewph;	// Viewport size, in pixels.
int viewpx, viewpy; // Viewpoint top left corner, in pixels.

float		yfov;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector;		// No range checking for the first one.

// CODE --------------------------------------------------------------------

//===========================================================================
// Rend_SignedPointDist2D
//===========================================================================
float Rend_SignedPointDist2D(float c[2])
{
/*          (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
        s = -----------------------------
                        L**2
	Luckily, L**2 is one. dist = s*L. Even more luckily, L is also one.
*/
	return (vz-c[VY])*viewsidex - (vx-c[VX])*viewsidey;
}

//===========================================================================
// Rend_PointDist3D
//	Approximated! The Z axis aspect ratio is corrected.
//===========================================================================
float Rend_PointDist3D(float c[3])
{
	return M_ApproxDistance3f(vx - c[VX], vz - c[VY], 1.2f * (vy - c[VZ]));
}	

//===========================================================================
// Rend_Init
//===========================================================================
void Rend_Init(void)
{
	C_Init();			// Clipper.
	RL_Init();			// Rendering lists.
	Rend_InitSky();		// The sky.	
}

//===========================================================================
// Rend_Reset
//	Used to be called before starting a new level.
//===========================================================================
void Rend_Reset(void)	
{
	// Textures are deleted (at least skies need this???).
	GL_ClearRuntimeTextures();
	DL_Clear();

	PRINT_PROF( PROF_REND_MAP );
	PRINT_PROF( PROF_REND_INIT );
	PRINT_PROF( PROF_REND_INIT_LIGHTS );
	PRINT_PROF( PROF_REND_NODES );
	PRINT_PROF( PROF_REND_SHADOWS );

	PRINT_PROF( PROF_REND_SUB_LIGHTS );
	PRINT_PROF( PROF_REND_SUB_OCCLUDE );
	PRINT_PROF( PROF_REND_SUB_ADD_SPRITES );
	PRINT_PROF( PROF_REND_SUB_SEGS );
	PRINT_PROF( PROF_REND_WALLSEG_1 );
	PRINT_PROF( PROF_REND_WALLSEG_2 );
	PRINT_PROF( PROF_REND_WALLSEG_CADD );
	PRINT_PROF( PROF_REND_WALLSEG_3 );
	PRINT_PROF( PROF_REND_WALLSEG_4 );
	PRINT_PROF( PROF_REND_WALLSEG_5 );
	PRINT_PROF( PROF_REND_SUB_PLANES );
	PRINT_PROF( PROF_REND_SUB_PLANE_1 );
	PRINT_PROF( PROF_REND_PREP_FLAT );
	PRINT_PROF( PROF_REND_SUB_PLANE_2 );
}

//===========================================================================
// Rend_ModelViewMatrix
//===========================================================================
void Rend_ModelViewMatrix(boolean use_angles)
{
	vx = FIX2FLT(viewx);
	vy = FIX2FLT(viewz);
	vz = FIX2FLT(viewy);
	vang = viewangle / (float)ANGLE_MAX * 360 - 90;

	gl.MatrixMode(DGL_MODELVIEW);
	gl.LoadIdentity();
	if(use_angles)
	{
		gl.Rotatef(vpitch = viewpitch*85.0/110.0, 1, 0, 0);
		gl.Rotatef(vang, 0, 1, 0);
	}
	gl.Scalef(1, 1.2f, 1);	// This is the aspect correction.
	gl.Translatef(-vx,-vy,-vz);
}

#if 0
//===========================================================================
// R_AttenuateLevel
//	Models the effect of distance to the light level. Extralight (torch)
//	is also noted. This is meant to be used for white light only 
//	(a light level).
//===========================================================================
int R_AttenuateLevel(int lightlevel, float distance)
{
	float light = lightlevel / 255.0f, real, minimum;
	float d, newmin;
	int i;
	//boolean usewhite = false;

	real = light - (distance - 32)/maxLightDist * (1-light);
	minimum = light*light + (light - .63f) / 2;
	if(real < minimum) real = minimum; // Clamp it.

	// Add extra light.
	real += extralight/16.0f;

	// Check for torch.
	if(viewplayer->fixedcolormap)
	{
		// Colormap 1 is the brightest. I'm guessing 16 would be the darkest.
		newmin = ((1024 - distance) / 512) 
			* (16 - viewplayer->fixedcolormap) / 15.0f;
		if(real < newmin) 
		{
			real = newmin;
			//usewhite = true; // FIXME : Do some linear blending.
		}
	}

	real *= 256;

	// Clamp the final light.
	if(real < 0) real = 0;
	if(real > 255) real = 255;

	/*for(i = 0; i < 3; i++)
		vtx->color.rgb[i] = (DGLubyte) ((usewhite? 0xff : rgb[c]) * real);*/
	return real;
}
#endif

//===========================================================================
// Rend_SegFacingDir
//===========================================================================
int Rend_SegFacingDir(float v1[2], float v2[2])
{
	// The dot product. (1 if facing front.)
	return (v1[VY] - v2[VY]) * (v1[VX] - vx) 
		+ (v2[VX] - v1[VX]) * (v1[VY] - vz) > 0;
}

//===========================================================================
// Rend_FixedSegFacingDir
//===========================================================================
int Rend_FixedSegFacingDir(seg_t *seg)
{
	// The dot product. (1 if facing front.)
	return FIX2FLT(seg->v1->y - seg->v2->y) * FIX2FLT(seg->v1->x - viewx) 
		+ FIX2FLT(seg->v2->x - seg->v1->x) * FIX2FLT(seg->v1->y - viewy) > 0;
}

//===========================================================================
// Rend_SegFacingPoint
//===========================================================================
int Rend_SegFacingPoint(float v1[2], float v2[2], float pnt[2])
{
	float nx = v1[VY] - v2[VY], ny = v2[VX] - v1[VX];
	float vvx = v1[VX] - pnt[VX], vvy = v1[VY] - pnt[VY];

	// The dot product.
	if(nx * vvx + ny * vvy > 0) return 1;	// Facing front.
	return 0;	// Facing away.
}

static int C_DECL DivSortAscend(const void *e1, const void *e2)
{
	float f1 = *(float*) e1, f2 = *(float*) e2;

	if(f1 > f2) return 1;
	if(f2 > f1) return -1;
	return 0;
}

static int C_DECL DivSortDescend(const void *e1, const void *e2)
{
	float f1 = *(float*) e1, f2 = *(float*) e2;

	if(f1 > f2) return -1;
	if(f2 > f1) return 1;
	return 0;
}

//===========================================================================
// Rend_CheckDiv
//	Returns true if the quad has a division at the specified height.
//===========================================================================
int Rend_CheckDiv(rendpoly_t *quad, int side, float height)
{
	int		i;

	for(i = 0; i < quad->divs[side].num; i++)
		if(quad->divs[side].pos[i] == height) return true;
	return false;
}

//===========================================================================
// Rend_PolyTextureBlend
//===========================================================================
void Rend_PolyTextureBlend(int texture, rendpoly_t *poly)
{
	translation_t *xlat = &texturetranslation[texture];

	// If fog is active, inter=0 is accepted as well. Otherwise flickering
	// may occur if the rendering passes don't match for blended and
	// unblended surfaces.
	if(!smoothTexAnim 
		|| numTexUnits < 2 
		|| !texture 
		|| xlat->current == xlat->next 
		|| (!useFog && xlat->inter <= 0)) 
	{
		// No blending for you, my friend.
		memset(&poly->intertex, 0, sizeof(poly->intertex));
		poly->interpos = 0;
		return;
	}

	// Get info of the blend target. The globals texw and texh are modified.
	poly->intertex.id = GL_PrepareTexture2(xlat->next, false);
	poly->intertex.width = texw;
	poly->intertex.height = texh;
	poly->intertex.detail = texdetail;
	poly->interpos = xlat->inter;
}

//===========================================================================
// Rend_PolyFlatBlend
//===========================================================================
void Rend_PolyFlatBlend(int flat, rendpoly_t *poly)
{
	flat_t *ptr = R_GetFlat(flat);
	
	// If fog is active, inter=0 is accepted as well. Otherwise flickering
	// may occur if the rendering passes don't match for blended and
	// unblended surfaces.
	if(!smoothTexAnim 
		|| numTexUnits < 2
		|| ptr->translation.current == ptr->translation.next
		|| (!useFog && ptr->translation.inter <= 0))
	{
		memset(&poly->intertex, 0, sizeof(poly->intertex));
		poly->interpos = 0;
		return;
	}

	// Get info of the blend target. The globals texw and texh are modified.
	poly->intertex.id = GL_PrepareFlat2(ptr->translation.next, false);
	poly->intertex.width = texw;
	poly->intertex.height = texh;
	poly->intertex.detail = texdetail;
	poly->interpos = ptr->translation.inter;
}

//===========================================================================
// Rend_WallHeightDivision
//	Division will only happen if it must be done.
//	Converts quads to divquads.
//===========================================================================
void Rend_WallHeightDivision
	(rendpoly_t *quad, seg_t *seg, sector_t *frontsec, int mode)
{
	int				i, k, vtx[2];		// Vertex indices.
	vertexowner_t	*own;
	sector_t		*sec;
	float			hi, low;
	float			sceil, sfloor;

	switch(mode)
	{
	case SEG_MIDDLE:
		hi = SECT_CEIL(frontsec);
		low = SECT_FLOOR(frontsec);
		break;

	case SEG_TOP:
		hi = SECT_CEIL(frontsec);
		low = SECT_CEIL(seg->backsector);
		if(SECT_FLOOR(frontsec) > low) low = SECT_FLOOR(frontsec);
		break;

	case SEG_BOTTOM:
		hi = SECT_FLOOR(seg->backsector);
		low = SECT_FLOOR(frontsec);
		if(SECT_CEIL(frontsec) < hi) hi = SECT_CEIL(frontsec);
		break;

	default:
		return;
	}

	vtx[0] = GET_VERTEX_IDX(seg->v1);
	vtx[1] = GET_VERTEX_IDX(seg->v2);
	quad->divs[0].num = 0;
	quad->divs[1].num = 0;

	// Check both ends.
	for(i = 0; i < 2; i++)
	{
		own = vertexowners + vtx[i];
		if(own->num > 1)
		{
			// More than one sectors! The checks must be made.
			for(k = 0; k < own->num; k++)
			{
				sec = SECTOR_PTR(own->list[k]);
				if(sec == frontsec) continue;	// Skip this sector.
				if(sec == seg->backsector) continue;

				sceil = SECT_CEIL(sec);
				sfloor = SECT_FLOOR(sec);

				// Divide at the sector's ceiling height?
				if(sceil > low && sceil < hi)
				{
					quad->type = RP_DIVQUAD;
					if(!Rend_CheckDiv(quad, i, sceil))
						quad->divs[i].pos[quad->divs[i].num++] = sceil;
				}
				// Do we need to break?
				if(quad->divs[i].num == RL_MAX_DIVS) break;

				// Divide at the sector's floor height?
				if(sfloor > low && sfloor < hi)
				{
					quad->type = RP_DIVQUAD;
					if(!Rend_CheckDiv(quad, i, sfloor))
						quad->divs[i].pos[quad->divs[i].num++] = sfloor;
				}					
				// Do we need to break?
				if(quad->divs[i].num == RL_MAX_DIVS) break;
			}
			// We need to sort the divisions for the renderer.
			if(quad->divs[i].num > 1)
			{
				// Sorting is required. This shouldn't take too long...
				// There seldom are more than one or two divisions.
				qsort(quad->divs[i].pos, quad->divs[i].num, sizeof(float), 
					i? DivSortDescend : DivSortAscend);
			}
#ifdef RANGECHECK
			for(k = 0; k < quad->divs[i].num; k++)
				if(quad->divs[i].pos[k] > hi || 
					quad->divs[i].pos[k] < low)
				{
					Con_Error("DivQuad: i=%i, pos (%f), hi (%f), "
						"low (%f), num=%i\n", i, quad->divs[i].pos[k], 
						hi, low, quad->divs[i].num);
				}
#endif
		}
	}
}

//===========================================================================
// Rend_MidTexturePos
//	Calculates the placement for a middle texture (top, bottom, offset).
//	Texture must be prepared so texh is known.
//	texoffy may be NULL.
//	Returns false if the middle texture isn't visible (in the opening).
//===========================================================================
int Rend_MidTexturePos(float *top, float *bottom, float *texoffy, 
					   float tcyoff, boolean lower_unpeg)
{
	float openingTop = *top, openingBottom = *bottom;

	if(openingTop <= openingBottom) return false;

	if(texoffy)	*texoffy = 0;
	
	// We don't allow vertical tiling.
	if(lower_unpeg)
	{
		*bottom += tcyoff;
		*top = *bottom + texh;
	}
	else
	{
		*top += tcyoff;
		*bottom = *top - texh;
	}

	// Clip it.
	if(*bottom < openingBottom) 
	{
		*bottom = openingBottom;
	}
	if(*top > openingTop) 
	{
		if(texoffy) *texoffy += *top - openingTop;
		*top = openingTop;
	}
	return true;
}

//===========================================================================
// Rend_RenderWallSeg
//	The sector height should've been checked by now.
//	This seriously needs to be rewritten! Witness the accumulation of hacks
//	on kludges...
//===========================================================================
void Rend_RenderWallSeg(seg_t *seg, sector_t *frontsec, int flags)
{
	int			segindex, sectorlight;
	sector_t	*backsec;
	side_t		*sid;
	line_t		*ldef;
	float		ffloor, fceil, bfloor, bceil, fsh, bsh, mceil, tcyoff;
	rendpoly_t	quad;
	float		*v1, *v2;
	byte		topvis, midvis, botvis;

	// Let's first check which way this seg is facing.
	if(!Rend_FixedSegFacingDir(seg)) return;

	BEGIN_PROF( PROF_REND_WALLSEG_1 );

	segindex = GET_SEG_IDX(seg);
	backsec  = seg->backsector;
	sid      = seg->sidedef;
	ldef     = seg->linedef;
	ffloor   = SECT_FLOOR(frontsec);
	fceil    = SECT_CEIL(frontsec); 
	fsh      = fceil - ffloor;

	// Init the quad.
	quad.type = RP_QUAD;
	quad.flags = 0;
	quad.tex.detail = NULL;
	quad.intertex.detail = NULL;
	quad.sector = frontsec;
	quad.numvertices = 2;

	v1 = quad.vertices[0].pos;
	v2 = quad.vertices[1].pos;

	// Get the start and end points. 
	v1[VX] = FIX2FLT(seg->v1->x);
	v1[VY] = FIX2FLT(seg->v1->y);
	v2[VX] = FIX2FLT(seg->v2->x);
	v2[VY] = FIX2FLT(seg->v2->y);

	// Calculate the distances.
	quad.vertices[0].dist = Rend_PointDist2D(v1);
	quad.vertices[1].dist = Rend_PointDist2D(v2);

	// This line is now seen in the map.
	ldef->flags |= ML_MAPPED;

	// Some texture coordinates.
	quad.length = seg->length;
	quad.texoffx = FIX2FLT(sid->textureoffset + seg->offset);
	tcyoff = FIX2FLT(sid->rowoffset);

	// Calculate the color at both vertices.
	sectorlight = Rend_SectorLight(frontsec);
	RL_VertexColors(&quad, sectorlight, R_GetSectorLightColor(frontsec));

	END_PROF( PROF_REND_WALLSEG_1 );

	// The middle texture, single sided.
	if(sid->midtexture && !backsec)
	{
		BEGIN_PROF( PROF_REND_WALLSEG_2 );

		quad.tex.id = curtex = GL_PrepareTexture(sid->midtexture);		
		quad.tex.detail = texdetail;
		quad.texoffy = tcyoff;
		if(ldef->flags & ML_DONTPEGBOTTOM)
			quad.texoffy += texh-fsh;

		// Fill in the remaining quad data.
		quad.flags = /*seg->flags & DDSEGF_DLIGHT? RPF_DLIT : */ 0;
		// What about glow?
		if(R_TextureFlags(sid->midtexture) & TXF_GLOW)
			quad.flags |= RPF_GLOW;
		quad.top = fceil;
		quad.bottom = ffloor;
		quad.tex.width = texw;
		quad.tex.height = texh;

		// Check for neighborhood division.
		Rend_WallHeightDivision(&quad, seg, frontsec, SEG_MIDDLE);
		
		// Dynamic lights.
		quad.lights = DL_GetSegLightLinks(segindex, SEG_MIDDLE);

		Rend_PolyTextureBlend(sid->midtexture, &quad);
		RL_AddPoly(&quad);
		Rend_RadioWallSection(seg, &quad);

		BEGIN_PROF( PROF_REND_WALLSEG_CADD );
		// This is guaranteed to be a solid segment.
		C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
		END_PROF( PROF_REND_WALLSEG_CADD );

		END_PROF( PROF_REND_WALLSEG_2 );
	}

	// Restore original type, height division may change this.
	quad.type = RP_QUAD;

	if(backsec)
	{
		bceil = SECT_CEIL(backsec);
		bfloor = SECT_FLOOR(backsec);
	}

	// The skyfix?
	if(frontsec->skyfix)
	{
		if(!backsec || (backsec && (bceil+backsec->skyfix < 
			fceil+frontsec->skyfix) ))
		{
			quad.flags = RPF_SKY_MASK;
			quad.top = fceil + frontsec->skyfix;
			quad.bottom = fceil;
			quad.tex.id = 0;
			quad.lights = NULL;
			quad.intertex.id = 0;
			RL_AddPoly(&quad);
		}
	}
	
	// If there is a back sector we may need upper and lower walls.
	if(backsec)	// A twosided seg?
	{
		boolean mid_covers_top = false;

		BEGIN_PROF( PROF_REND_WALLSEG_3 );

		bsh = bceil - bfloor;

		// Determine which parts of the segment are visible.
		midvis = (bsh > 0 && sid->midtexture);
		topvis = (bceil < fceil);
		botvis = (bfloor > ffloor);

		// Missing top or bottom textures don't occlude visibility.
		if((bsh <= 0 || bceil <= ffloor || bfloor >= fceil) 
			&& !(topvis && !sid->toptexture && sid->midtexture)
			&& !(botvis && !sid->bottomtexture && sid->midtexture)
			/*&& (sid->toptexture || sid->midtexture || sid->bottomtexture)*/)
		{
			// The backsector has no space. This is a solid segment.
			C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
		}

		// Needs skyfix?
		if(bsh <= 0 && frontsec->ceilingpic == skyflatnum
			&& fceil+frontsec->skyfix > bceil
			&& (!sid->toptexture || backsec->ceilingpic == skyflatnum))
		{
			quad.flags = RPF_SKY_MASK;
			quad.top = fceil + frontsec->skyfix;
			quad.bottom = bceil;
			quad.tex.id = 0;
			quad.lights = NULL;
			quad.intertex.id = 0;
			RL_AddPoly(&quad);
		}

		END_PROF( PROF_REND_WALLSEG_3 );

		// Quite probably a masked texture. Won't be drawn if a visible
		// top or bottom texture is missing.
		if(sid->midtexture)
		{
			// Use actual sector heights (non-linked).
			float rbceil = FIX2FLT(backsec->ceilingheight),
				rbfloor = FIX2FLT(backsec->floorheight),
				rfceil = FIX2FLT(frontsec->ceilingheight),
				rffloor = FIX2FLT(frontsec->floorheight);
			float gaptop, gapbottom;

			BEGIN_PROF( PROF_REND_WALLSEG_4 );

			quad.top = gaptop = MIN_OF(rbceil, rfceil);
			quad.bottom = gapbottom = MAX_OF(rbfloor, rffloor);

			quad.tex.id = curtex = GL_PrepareTexture(sid->midtexture);
			quad.tex.detail = texdetail;
			quad.tex.width = texw;
			quad.tex.height = texh;

			if(topvis && !sid->toptexture)
			{
				mceil = quad.top;
				// Extend to cover missing top texture.
				quad.top = MAX_OF(bceil, fceil);
				if(texh > quad.top - quad.bottom)
				{
					mid_covers_top = true; // At least partially...
					tcyoff -= quad.top - mceil;
				}
			}
			
			if( Rend_MidTexturePos(&quad.top, &quad.bottom, &quad.texoffy, 
				tcyoff, 0 != (ldef->flags & ML_DONTPEGBOTTOM)) )
			{
				quad.flags = texmask? RPF_MASKED : 0;

				// What about glow?
				if(R_TextureFlags(sid->midtexture) & TXF_GLOW)
					quad.flags |= RPF_GLOW;
				
				// Dynamic lights.
				quad.lights = DL_GetSegLightLinks(segindex, SEG_MIDDLE);

				// Should a solid segment be added here?
				if(!texmask && quad.top >= gaptop && quad.bottom <= gapbottom)
					C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);

				Rend_PolyTextureBlend(sid->midtexture, &quad);
				RL_AddPoly(&quad);
				if(!texmask) Rend_RadioWallSection(seg, &quad);
			}
			END_PROF( PROF_REND_WALLSEG_4 );
		}
		BEGIN_PROF( PROF_REND_WALLSEG_5 );
		
		// Upper wall.
		if(topvis && !(frontsec->ceilingpic == skyflatnum 
			&& backsec->ceilingpic == skyflatnum)
			&& !mid_covers_top)
		{
			float topwh = fceil - bceil;
			if(sid->toptexture)	// Has a texture?
			{
				quad.tex.id = curtex = GL_PrepareTexture(sid->toptexture);
			}
			else
			{
				// Texture missing? Take the ceiling texture.
				int replacement = frontsec->ceilingpic;
				if(replacement == skyflatnum)
				{
					// The skyflat is not an appropriate replacement!
					// Note that both the front and back ceilings can't be
					// sky (the upper seg is not visible in that case).
					replacement = backsec->ceilingpic;
				}
				quad.tex.id = curtex = GL_PrepareFlat(replacement);
			}
			quad.tex.detail = texdetail;
			// Calculate texture coordinates.
			quad.texoffy = tcyoff;
			if(!(ldef->flags & ML_DONTPEGTOP))
			{
				// Normal alignment to bottom.
				quad.texoffy += texh-topwh;
			}								
			quad.flags = 0;
			//if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;
			// What about glow?
			if(R_TextureFlags(sid->toptexture) & TXF_GLOW)
				quad.flags |= RPF_GLOW;
			quad.top = fceil;
			quad.bottom = bceil;
			if(quad.bottom < ffloor) quad.bottom = ffloor;
			quad.tex.width = texw;
			quad.tex.height = texh;

			// Might be necessary.
			Rend_WallHeightDivision(&quad, seg, frontsec, SEG_TOP);

			// Dynamic lights.
			quad.lights = DL_GetSegLightLinks(segindex, SEG_TOP);

			Rend_PolyTextureBlend(sid->toptexture, &quad);
			RL_AddPoly(&quad);
			Rend_RadioWallSection(seg, &quad);

			// Restore original type, height division may change this.
			quad.type = RP_QUAD;
		}
		
		// Lower wall.
		// If no textures have been assigned to the segment, we won't
		// draw anything.
		if(/*(sid->bottomtexture || sid->midtexture || sid->toptexture) &&*/
			bfloor > ffloor 
			&& !(frontsec->floorpic == skyflatnum 
				&& backsec->floorpic == skyflatnum)
			//&& (!sid->midtexture || sid->bottomtexture)
			)
		{
			if(sid->bottomtexture)	// There is a texture?
			{
				quad.tex.id = curtex = GL_PrepareTexture(sid->bottomtexture);
			}
			else
			{
				// No texture? Again!
				quad.tex.id = curtex = GL_PrepareFlat(frontsec->floorpic);
			}
			quad.tex.detail = texdetail;
			// Calculate texture coordinates.
			quad.texoffy = tcyoff;
			if(ldef->flags & ML_DONTPEGBOTTOM)
			{
				// Lower unpegged. Align with normal middle texture.
				quad.texoffy += fceil - bfloor;
			}
			quad.flags = 0;
//			if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;
			// What about glow?
			if(R_TextureFlags(sid->bottomtexture) & TXF_GLOW)
				quad.flags |= RPF_GLOW;
			quad.top = bfloor;
			if(quad.top > fceil)
			{
				// Can't go over front ceiling, would induce polygon flaws.
				quad.texoffy += quad.top - fceil;
				quad.top = fceil;
			}
			quad.bottom = ffloor;
			quad.tex.width = texw;
			quad.tex.height = texh;

			// Might be necessary.
			Rend_WallHeightDivision(&quad, seg, frontsec, SEG_BOTTOM);

			// Dynamic lights.
			quad.lights = DL_GetSegLightLinks(segindex, SEG_BOTTOM);
			
			Rend_PolyTextureBlend(sid->bottomtexture, &quad);
			RL_AddPoly(&quad);
			Rend_RadioWallSection(seg, &quad);
		}
		END_PROF( PROF_REND_WALLSEG_5 );
	}
}

//===========================================================================
// Rend_SectorLight
//===========================================================================
int Rend_SectorLight(sector_t *sec)
{
	int i;

	i = LevelFullBright? 255 : sec->lightlevel;
	if(i < r_ambient) i = r_ambient;
	if(i > 255) i = 255;
	return i;
}

//===========================================================================
// Rend_OccludeSubsector
//	Creates new occlusion planes from the subsector's sides.
//	Before testing, occlude subsector's backfaces. After testing occlude
//	the remaining faces, i.e. the forward facing segs. This is done before 
//	rendering segs, so solid segments cut out all unnecessary oranges.
//===========================================================================
void Rend_OccludeSubsector(subsector_t *sub, boolean forward_facing)
{
	sector_t *front = sub->sector, *back;
	int i, segfacing;
	float v1[2], v2[2], fronth[2], backh[2];
	float *startv, *endv;
	seg_t *seg;

	fronth[0] = FIX2FLT( front->floorheight );
	fronth[1] = FIX2FLT( front->ceilingheight );

	for(i = 0; i < sub->linecount; i++)
	{
		seg = SEG_PTR(sub->firstline + i);
		// Occlusions can only happen where two sectors contact.
		if(!seg->linedef || !seg->backsector) continue;
		back = seg->backsector;
		v1[VX] = FIX2FLT( seg->v1->x );
		v1[VY] = FIX2FLT( seg->v1->y );
		v2[VX] = FIX2FLT( seg->v2->x );
		v2[VY] = FIX2FLT( seg->v2->y );
		// Which way should it be facing?
		segfacing = Rend_SegFacingDir(v1, v2); // 1=front
		if(forward_facing != (segfacing != 0)) continue;
		backh[0] = FIX2FLT( back->floorheight );
		backh[1] = FIX2FLT( back->ceilingheight );
		// Choose start and end vertices so that it's facing forward.
		if(forward_facing)
		{
			startv = v1; 
			endv = v2;
		}
		else
		{
			startv = v2;
			endv = v1;
		}
		// Do not create an occlusion for sky floors.
		if(back->floorpic != skyflatnum || front->floorpic != skyflatnum)
		{
			// Do the floors create an occlusion?			
			if((backh[0] > fronth[0] && vy <= backh[0])	||
			   (backh[0] < fronth[0] && vy >= fronth[0]))
			{
				C_AddViewRelOcclusion(startv, endv, 
					MAX_OF(fronth[0], backh[0]), false); // Occlude down.
			}
		}
		// Do not create an occlusion for sky ceilings.
		if(back->ceilingpic != skyflatnum || front->ceilingpic != skyflatnum)
		{
			// Do the ceilings create an occlusion?
			if((backh[1] < fronth[1] && vy >= backh[1]) ||
			   (backh[1] > fronth[1] && vy <= fronth[1]))
			{
				C_AddViewRelOcclusion(startv, endv, 
					MIN_OF(fronth[1], backh[1]), true); // Occlude up.
			}
		}
	}
}

//===========================================================================
// Rend_RenderPlane
//===========================================================================
void Rend_RenderPlane
	(planeinfo_t *plane, dynlight_t *lights, subsector_t *subsector, 
	 sectorinfo_t *sin)
{
	rendpoly_t poly;
	sector_t *sector = subsector->sector, *link = NULL;
	int planepic;
	float height;

	// We're creating a flat.
	poly.type = RP_FLAT;
	poly.lights = lights;

	if(plane->isfloor)
	{
		// Determine the height of the floor.
		if(sin->linkedfloor)
		{
			poly.sector = link = R_GetLinkedSector(sin->linkedfloor, true);

			// This sector has an invisible floor.
			height = SECT_FLOOR(link);
			planepic = link->floorpic;
		}
		else
		{
			poly.sector = sector;
			height = sin->visfloor;
			planepic = sector->floorpic;
		}
	}
	else
	{
		// This is a ceiling plane.
		if(sin->linkedceil)
		{
			poly.sector = link = R_GetLinkedSector(sin->linkedceil, false);

			// This sector has an invisible ceiling.
			height = SECT_CEIL(link);
			planepic = link->ceilingpic;
		}
		else
		{
			poly.sector = sector;
			height = sin->visceil + sector->skyfix;
			planepic = sector->ceilingpic;
		}
	}
		
	BEGIN_PROF( PROF_REND_SUB_PLANE_1 );
	// Has the texture changed?
	if(planepic != plane->pic)
	{
		plane->pic = planepic;

		// The sky?
		if(planepic == skyflatnum)
			plane->flags |= RPF_SKY_MASK;
		else
			plane->flags &= ~RPF_SKY_MASK;

		// A glowing texture?
		if(R_FlatFlags(planepic) & TXF_GLOW) 
			plane->flags |= RPF_GLOW;
		else
			plane->flags &= ~RPF_GLOW;
	}
	poly.flags = plane->flags;
	END_PROF( PROF_REND_SUB_PLANE_1 );

	// Is the plane visible?
	if((plane->isfloor && vy > height) ||
	   (!plane->isfloor && vy < height))
	{
		// Check for sky.
		if(plane->pic == skyflatnum) 
		{
			poly.lights = NULL;
			skyhemispheres |= (plane->isfloor? SKYHEMI_LOWER : SKYHEMI_UPPER);
		}
		else
		{
			poly.tex.id = curtex = GL_PrepareFlat(planepic);
			poly.tex.detail = texdetail;
			poly.tex.width = texw;
			poly.tex.height = texh;
			if(plane->isfloor)
			{
				poly.texoffx = sector->flooroffx;
				poly.texoffy = sector->flooroffy;
			}
			else
			{
				poly.texoffx = sector->ceiloffx;
				poly.texoffy = sector->ceiloffy;
			}	
		}
		poly.top = height;

		BEGIN_PROF( PROF_REND_PREP_FLAT );
		RL_PrepareFlat(plane, &poly, subsector);
		END_PROF( PROF_REND_PREP_FLAT );

		BEGIN_PROF( PROF_REND_SUB_PLANE_2 );
		Rend_PolyFlatBlend(plane->pic, &poly);
		RL_AddPoly(&poly);
		END_PROF( PROF_REND_SUB_PLANE_2 );
	}
}

//===========================================================================
// Rend_RenderSubsector
//===========================================================================
void Rend_RenderSubsector(int ssecidx)
{
	subsector_t		*ssec = SUBSECTOR_PTR(ssecidx);
	int				i;
	byte			*seg;
	sector_t		*sect = ssec->sector;
	int				sectoridx = GET_SECTOR_IDX(sect);
	sectorinfo_t	*sin = secinfo + sectoridx;
	int				flags = 0;
	float			sceil = sin->visceil, sfloor = sin->visfloor;
	lumobj_t		*lumi;	// Lum Iterator, or 'snow' in Finnish. :-)
	subsectorinfo_t	*subin;
	
	if(sceil - sfloor <= 0 || ssec->numverts < 3) 
	{
		// Skip this, it has no volume.
		// Neighbors handle adding the solid clipper segments.
		return;	
	}

	if(!firstsubsector)
	{
		if(!C_CheckSubsector(ssec)) return;	// This isn't visible.
	}
	else
	{
		firstsubsector = false;
	}

	BEGIN_PROF( PROF_REND_SUB_LIGHTS );

	// Mark the sector visible for this frame.
	sin->flags |= SIF_VISIBLE;

	// Dynamic lights. 
	if(useDynLights) DL_ProcessSubsector(ssec);

	END_PROF( PROF_REND_SUB_LIGHTS );

	// Prepare for FakeRadio.
	Rend_RadioInitForSector(sect);
	Rend_RadioSubsectorEdges(ssec);

	BEGIN_PROF( PROF_REND_SUB_OCCLUDE );

	Rend_OccludeSubsector(ssec, false);
	// Determine which dynamic light sources in the subsector get clipped.
	for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
	{
		lumi->flags &= ~LUMF_CLIPPED;
		// FIXME: Determine the exact centerpoint of the light in 
		// DL_AddLuminous!
		if(!C_IsPointVisible(FIX2FLT(lumi->thing->x),
			FIX2FLT(lumi->thing->y), 
			FIX2FLT(lumi->thing->z) + lumi->center))
			lumi->flags |= LUMF_CLIPPED; // Won't have a halo.
	}
	Rend_OccludeSubsector(ssec, true);

	END_PROF( PROF_REND_SUB_OCCLUDE );

	// Mark the particle generators in the sector visible.
	PG_SectorIsVisible(sect);

	BEGIN_PROF( PROF_REND_SUB_ADD_SPRITES );

	// Sprites for this sector have to be drawn. This must be done before
	// the segments of this subsector are added to the clipper. Otherwise
	// the sprites would get clipped by them, and that wouldn't be right.
	R_AddSprites(sect);

	END_PROF( PROF_REND_SUB_ADD_SPRITES );

	BEGIN_PROF( PROF_REND_SUB_SEGS );

	// Draw the walls.
	for(i = 0, seg = segs+SEGIDX(ssec->firstline); i < ssec->linecount; 
		i++, seg += SEGSIZE)
	{
		if(((seg_t*)seg)->linedef)	// "minisegs" have no linedefs.
			Rend_RenderWallSeg((seg_t*)seg, sect, flags);
	}

	// Is there a polyobj on board?
	if(ssec->poly)
		for(i = 0; i < ssec->poly->numsegs; i++)
			Rend_RenderWallSeg(ssec->poly->segs[i], sect, 0);

	END_PROF( PROF_REND_SUB_SEGS );

	BEGIN_PROF( PROF_REND_SUB_PLANES );

	subin = &subsecinfo[ssecidx];
	Rend_RenderPlane(&subin->floor, floorLightLinks[ssecidx], ssec, sin);
	Rend_RenderPlane(&subin->ceil, ceilingLightLinks[ssecidx], ssec, sin);

	END_PROF( PROF_REND_SUB_PLANES );
}

//===========================================================================
// Rend_RenderNode
//===========================================================================
void Rend_RenderNode(int bspnum)
{
	node_t          *bsp;
	int             side;

	// If the clipper is full we're pretty much done.
	if(C_IsFull()) return;

	if(bspnum & NF_SUBSECTOR)
	{
		if (bspnum == -1)
			Rend_RenderSubsector(0);
		else
			Rend_RenderSubsector(bspnum&(~NF_SUBSECTOR));
		return;
	}

	bsp = NODE_PTR(bspnum);

	// Decide which side the view point is on.
	side = R_PointOnSide (viewx, viewy, bsp);
	
	Rend_RenderNode(bsp->children[side]); // Recursively divide front space.
	Rend_RenderNode(bsp->children[side^1]); // ...and back space.
}

//===========================================================================
// Rend_RenderMap
//===========================================================================
void Rend_RenderMap(void)
{
	binangle_t	viewside;

	BEGIN_PROF( PROF_REND_MAP );

	// Set to true if dynlights are inited for this frame.
	dlInited = false;

	// This is all the clearing we'll do.
	gl.Clear(DGL_DEPTH_BUFFER_BIT);

	// Setup the modelview matrix.
	Rend_ModelViewMatrix(true);

	if(!freezeRLs)
	{
		BEGIN_PROF( PROF_REND_INIT );

		// Prepare for rendering.
		R_UpdatePlanes();		// Update all planes.
		RL_ClearLists();		// Clear the lists for new quads.
		C_ClearRanges();		// Clear the clipper.
		R_ClearSectorFlags();
		DL_ClearForFrame();		// Zeroes the links.

		END_PROF( PROF_REND_INIT );

		BEGIN_PROF( PROF_REND_INIT_LIGHTS );
	
		// Generate surface decorations for the frame.
		Rend_InitDecorationsForFrame();

		// Maintain luminous objects.
		if(useDynLights || haloMode || litSprites || useDecorations) 
		{
			DL_InitForNewFrame(); 
		}

		END_PROF( PROF_REND_INIT_LIGHTS );

		// Add the backside clipping range (if vpitch allows).
		if(vpitch <= 90-yfov/2 && vpitch >= -90+yfov/2)
		{
			float a = fabs(vpitch) / (90-yfov/2);
			binangle_t startAngle = (binangle_t) (BANG_45*fieldOfView/90)*(1+a);
			binangle_t angLen = BANG_180 - startAngle;																					
			viewside = (viewangle>>(32-BAMS_BITS)) + startAngle;
			C_SafeAddRange(viewside, viewside+angLen);
			C_SafeAddRange(viewside+angLen, viewside+2*angLen);
		}
		// The viewside line for the depth cue.
		viewsidex = -FIX2FLT(viewsin);
		viewsidey = FIX2FLT(viewcos);

		// We don't want subsector clipchecking for the first subsector.
		firstsubsector = true;

		BEGIN_PROF( PROF_REND_NODES );
		Rend_RenderNode(numnodes - 1);
		END_PROF( PROF_REND_NODES );

		// Make vissprites of all the visible decorations.
		Rend_ProjectDecorations();

		BEGIN_PROF( PROF_REND_SHADOWS );
		Rend_RenderShadows();
		END_PROF( PROF_REND_SHADOWS );
	}
	RL_RenderAllLists();

	END_PROF( PROF_REND_MAP );
}


// Console commands.
int CCmdFog(int argc, char **argv)
{
	int		i;

	if(argc == 1)
	{
		Con_Printf( "Usage: %s (cmd) (args)\n", argv[0]);
		Con_Printf( "Commands: on, off, mode, color, start, end, density.\n");
		Con_Printf( "Modes: linear, exp, exp2.\n");
		//Con_Printf( "Hints: fastest, nicest, dontcare.\n");
		Con_Printf( "Color is given as RGB (0-255).\n");
		Con_Printf( "Start and end are for linear fog, density for exponential.\n");
		return true;		
	}
	if(!stricmp(argv[1], "on"))
	{
		GL_UseFog(true);
		Con_Printf( "Fog is now active.\n");
	}
	else if(!stricmp(argv[1], "off"))
	{
		GL_UseFog(false);
		Con_Printf( "Fog is now disabled.\n");
	}
	else if(!stricmp(argv[1], "mode") && argc == 3)
	{
		if(!stricmp(argv[2], "linear"))	
		{
			gl.Fog(DGL_FOG_MODE, DGL_LINEAR);
			Con_Printf( "Fog mode set to linear.\n");
		}
		else if(!stricmp(argv[2], "exp")) 
		{
			gl.Fog(DGL_FOG_MODE, DGL_EXP);
			Con_Printf( "Fog mode set to exp.\n");
		}
		else if(!stricmp(argv[2], "exp2")) 
		{
			gl.Fog(DGL_FOG_MODE, DGL_EXP2);
			Con_Printf( "Fog mode set to exp2.\n");
		}
		else return false;
	}
	else if(!stricmp(argv[1], "color") && argc == 5)
	{
		for(i=0; i<3; i++)
			fogColor[i] = strtol(argv[2+i], NULL, 0)/*/255.0f*/;
		fogColor[3] = 255;
		gl.Fogv(DGL_FOG_COLOR, fogColor);
		Con_Printf( "Fog color set.\n");
	}	
	else if(!stricmp(argv[1], "start") && argc == 3)
	{
		gl.Fog(DGL_FOG_START, strtod(argv[2], NULL));
		Con_Printf( "Fog start distance set.\n");
	}
	else if(!stricmp(argv[1], "end") && argc == 3)
	{
		gl.Fog(DGL_FOG_END, strtod(argv[2], NULL));
		Con_Printf( "Fog end distance set.\n");
	}
	else if(!stricmp(argv[1], "density") && argc == 3)
	{
		gl.Fog(DGL_FOG_DENSITY, strtod(argv[2], NULL));
		Con_Printf( "Fog density set.\n");
	}
	else return false;
	// Exit with a success.
	return true;
}

