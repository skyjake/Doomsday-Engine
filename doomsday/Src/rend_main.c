//**************************************************************************
//**
//** REND_MAIN.C
//**
//**************************************************************************

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

enum // For Wall Height Division.
{
	WHD_TOP,
	WHD_MIDDLE,
	WHD_BOTTOM
};

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

boolean		whitefog = false;		// Is the fog in use?
float		fieldOfView = 90.0f;
float		maxLightDist = 1024;

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
// Rend_PointDist2D
//	How far the point is from the viewside plane?
//===========================================================================
float Rend_PointDist2D(float c[2])
{
/*          (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
        s = -----------------------------
                        L**2
	Luckily, L**2 is one. dist = s*L. Even more luckily, L is also one.
*/
	float dist = (vz-c[VY])*viewsidex - (vx-c[VX])*viewsidey;
	if(dist < 0) return -dist;	// Always return positive.
	return dist;
}

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
	RL_DeleteLists();	// The rendering lists are destroyed.
	DL_Clear();
	//H_Clear();
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
	float nx = v1[VY]-v2[VY], ny = v2[VX]-v1[VX];
	float vvx = v1[VX]-vx, vvy = v1[VY]-vz;

	// The dot product.
	if(nx*vvx + ny*vvy > 0) return 1;	// Facing front.
	return 0;	// Facing away.
}

static int __cdecl DivSortAscend(const void *e1, const void *e2)
{
	float f1 = *(float*) e1, f2 = *(float*) e2;

	if(f1 > f2) return 1;
	if(f2 > f1) return -1;
	return 0;
}

static int __cdecl DivSortDescend(const void *e1, const void *e2)
{
	float f1 = *(float*) e1, f2 = *(float*) e2;

	if(f1 > f2) return -1;
	if(f2 > f1) return 1;
	return 0;
}

#ifdef _MSC_VER
// Bravo, Microsoft.
#pragma optimize("g",off)
#endif

// Called before rendering a frame to update sectorflags.
/*void R_CheckInvisPlanes(void)
{
	int i, k;
	boolean hackfloor, hackceil;
	sector_t *sec, *back;
	side_t *sid, *frontsid, *backsid;

	return;

	for(i=0; i<numsectors; i++)
	{
		sectorflags[i] &= ~(SECF_INVIS_FLOOR|SECF_INVIS_CEILING);
		sec = SECTOR_PTR(i);
		hackfloor = true;
		hackceil = true;
		for(k=0; k<sec->linecount; k++)
		{
			if(!hackfloor && !hackceil) break;
			if(!sec->lines[k]->backsector 
				|| !sec->lines[k]->frontsector) goto next_sector; 
			// Check which way the line is facing.		
			sid = SIDE_PTR(sec->lines[k]->sidenum[0]);
			if(sid->sector == sec)
			{
				frontsid = sid;
				backsid = SIDE_PTR(sec->lines[k]->sidenum[1]);
			}
			else
			{
				frontsid = SIDE_PTR(sec->lines[k]->sidenum[1]);
				backsid = sid;
			}
			back = backsid->sector;
			if(back == sec) continue;
			// Check the conditions that prevent the invis plane.
			if(back->floorheight == sec->floorheight)
				hackfloor = false;
			else
			{
				if(back->floorheight > sec->floorheight)
					sid = frontsid;
				else
					sid = backsid;
				if(sid->bottomtexture) hackfloor = false;
			}
			if(back->ceilingheight == sec->ceilingheight)
				hackceil = false;
			else 
			{
				if(back->ceilingheight < sec->ceilingheight)
					sid = frontsid;
				else
					sid = backsid;
				if(sid->toptexture) hackceil = false;
			}
		}
		if(hackfloor) sectorflags[i] |= SECF_INVIS_FLOOR;
		if(hackceil) sectorflags[i] |= SECF_INVIS_CEILING;

next_sector:;
	}
}*/

#ifdef _MSC_VER
#pragma optimize("",on)
#endif

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
// Rend_WallHeightDivision
//	Division will only happen if it must be done.
//	Converts quads to divquads.
//===========================================================================
void Rend_WallHeightDivision(rendpoly_t *quad, seg_t *seg, sector_t *frontsec,
						  int mode)
{
	int				i, k, vtx[2];		// Vertex indices.
	vertexowner_t	*own;
	sector_t		*sec;
	float			hi, low;
	float			sceil, sfloor;

	switch(mode)
	{
	case WHD_MIDDLE:
		hi = SECT_CEIL(frontsec);
		low = SECT_FLOOR(frontsec);
		break;

	case WHD_TOP:
		hi = SECT_CEIL(frontsec);
		low = SECT_CEIL(seg->backsector);
		if(SECT_FLOOR(frontsec) > low) low = SECT_FLOOR(frontsec);
		break;

	case WHD_BOTTOM:
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
					quad->type = rp_divquad;
					if(!Rend_CheckDiv(quad, i, sceil))
						quad->divs[i].pos[quad->divs[i].num++] = sceil;
				}
				// Do we need to break?
				if(quad->divs[i].num == RL_MAX_DIVS) break;

				// Divide at the sector's floor height?
				if(sfloor > low && sfloor < hi)
				{
					quad->type = rp_divquad;
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
//	Returns false if the middle texture isn't visible (in the opening).
//	In:	quad.top = openingTop
//		quad.bottom = openingBottom
//	Out: quad
//===========================================================================
int Rend_MidTexturePos(rendpoly_t *quad, float tcyoff, boolean lower_unpeg)
{
	float openingTop = quad->top, openingBottom = quad->bottom;
	float offset = 0;	

	if(openingTop <= openingBottom) return false;

	quad->texoffy = 0;
	
	// We don't allow vertical tiling.
	if(lower_unpeg)
	{
		quad->bottom += tcyoff;
		quad->top = quad->bottom + texh;
	}
	else
	{
		quad->top += tcyoff;
		quad->bottom = quad->top - texh;
	}

	// Clip it.
	if(quad->bottom < openingBottom) 
	{
		quad->bottom = openingBottom;
	}
	if(quad->top > openingTop) 
	{
		quad->texoffy += quad->top - openingTop;
		quad->top = openingTop;
	}
	return true;
}

//===========================================================================
// Rend_RenderWallSeg
//	The sector height should've been checked by now.
//===========================================================================
void Rend_RenderWallSeg(seg_t *seg, sector_t *frontsec, int flags)
{
	sector_t		*backsec = seg->backsector;
	side_t			*sid = seg->sidedef;
	line_t			*ldef = seg->linedef;
	float			ffloor = SECT_FLOOR(frontsec); 
	float			fceil = SECT_CEIL(frontsec); 
	float			bfloor, bceil, fsh = fceil-ffloor, bsh;
	float			mceil, tcyoff;
	rendpoly_t		quad;
	float			*v1, *v2;
	int				sectorlight;
	byte			topvis, midvis, botvis;

	// Init the quad.
	memset(&quad, 0, sizeof(quad));		
	quad.type = rp_quad;
	quad.numvertices = 2;

	v1 = quad.vertices[0].pos;
	v2 = quad.vertices[1].pos;

	// Get the start and end points. 
	v1[VX] = FIX2FLT(seg->v1->x);
	v1[VY] = FIX2FLT(seg->v1->y);
	v2[VX] = FIX2FLT(seg->v2->x);
	v2[VY] = FIX2FLT(seg->v2->y);

	// Let's first check which way this seg is facing.
	if(!Rend_SegFacingDir(v1, v2)) return;	// The wrong direction?
	//if(!C_CheckViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY])) return;

	// Calculate the distances.
	quad.vertices[0].dist = Rend_PointDist2D(v1);
	quad.vertices[1].dist = Rend_PointDist2D(v2);

	// This line is now seen in the map.
	ldef->flags |= ML_MAPPED;

	// Some texture coordinates.
	quad.texoffx = FIX2FLT(sid->textureoffset) + FIX2FLT(seg->offset);
	quad.length = seg->length;
	tcyoff = FIX2FLT(sid->rowoffset);

	// Calculate the color at both vertices.
	sectorlight = LevelFullBright? 255 : frontsec->lightlevel;
	if(sectorlight < r_ambient) sectorlight = r_ambient;
	RL_VertexColors(&quad, sectorlight, frontsec->rgb);

	// The middle texture, single sided.
	if(sid->midtexture && !backsec)
	{
		quad.tex = curtex = GL_PrepareTexture(sid->midtexture);		
		quad.detail = texdetail;
		quad.texoffy = tcyoff;
		if(ldef->flags & ML_DONTPEGBOTTOM)
			quad.texoffy += texh-fsh;

		// Fill in the remaining quad data.
		quad.flags = seg->flags & DDSEGF_DLIGHT? RPF_DLIT : 0;
		// What about glow?
		if(R_TextureFlags(sid->midtexture) & TXF_GLOW)
			quad.flags |= RPF_GLOW;
		quad.top = fceil;
		quad.bottom = ffloor;
		quad.texw = texw;
		quad.texh = texh;

		// Check for neighborhood division.
		Rend_WallHeightDivision(&quad, seg, frontsec, WHD_MIDDLE);
		
		RL_AddPoly(&quad);

		// This is guaranteed to be a solid segment.
		C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
	}

	// Restore original type, height division may change this.
	quad.type = rp_quad;

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
			quad.tex = 0;
			RL_AddPoly(&quad);
		}
	}
	// If there is a back sector we may need upper and lower walls.
	if(backsec)	// A twosided seg?
	{
		boolean mid_covers_top = false;

		bsh = bceil - bfloor;

		// Determine which parts of the segment are visible.
		midvis = (bsh > 0 && sid->midtexture);
		topvis = (bceil < fceil);
		botvis = (bfloor > ffloor);

		// Missing top or bottom textures don't occlude visibility.
		if((bsh <= 0 || bceil <= ffloor || bfloor >= fceil) 
			&& !(topvis && !sid->toptexture && sid->midtexture)
			&& !(botvis && !sid->bottomtexture && sid->midtexture))
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
			quad.tex = 0;
			RL_AddPoly(&quad);
		}

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
			quad.top = gaptop = MIN_OF(rbceil, rfceil);
			quad.bottom = gapbottom = MAX_OF(rbfloor, rffloor);

			quad.tex = curtex = GL_PrepareTexture(sid->midtexture);
			quad.detail = texdetail;
			quad.texw = texw;
			quad.texh = texh;

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
			
			if(Rend_MidTexturePos(&quad, tcyoff, 
				0 != (ldef->flags & ML_DONTPEGBOTTOM)) )
			{
				quad.flags = texmask? RPF_MASKED : 0;

				// What about glow?
				if(R_TextureFlags(sid->midtexture) & TXF_GLOW)
					quad.flags |= RPF_GLOW;
				
				if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;

				RL_AddPoly(&quad);

				// Should a solid segment be added here?
				if(!texmask && quad.top >= gaptop && quad.bottom <= gapbottom)
					C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
			}
		}
		// Upper wall.
		if(topvis && !(frontsec->ceilingpic == skyflatnum 
			&& backsec->ceilingpic == skyflatnum)
			&& !mid_covers_top)
		{
			float topwh = fceil - bceil;
			if(sid->toptexture)	// Has a texture?
			{
				quad.tex = curtex = GL_PrepareTexture(sid->toptexture);
			}
			else
			{
				// Texture missing? Take the ceiling texture.
				quad.tex = curtex = GL_PrepareFlat(frontsec->ceilingpic);				
				//quad.flags = RPF_MISSING_WALL;
			}
			quad.detail = texdetail;
			// Calculate texture coordinates.
			quad.texoffy = tcyoff;
			if(!(ldef->flags & ML_DONTPEGTOP))
			{
				// Normal alignment to bottom.
				quad.texoffy += texh-topwh;
			}								
			quad.flags = 0;
			if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;
			// What about glow?
			if(R_TextureFlags(sid->toptexture) & TXF_GLOW)
				quad.flags |= RPF_GLOW;
			quad.top = fceil;
			quad.bottom = bceil;
			if(quad.bottom < ffloor) quad.bottom = ffloor;
			quad.texw = texw;
			quad.texh = texh;

			// Might be necessary.
			Rend_WallHeightDivision(&quad, seg, frontsec, WHD_TOP);

			RL_AddPoly(&quad);

			// Restore original type, height division may change this.
			quad.type = rp_quad;
		}
		// Lower wall.
		if(bfloor > ffloor && !(frontsec->floorpic == skyflatnum 
			&& backsec->floorpic == skyflatnum)
			//&& (!sid->midtexture || sid->bottomtexture)
			)
		{
			if(sid->bottomtexture)	// There is a texture?
			{
				quad.tex = curtex = GL_PrepareTexture(sid->bottomtexture);
			}
			else
			{
				// No texture? Again!
				quad.tex = curtex = GL_PrepareFlat(frontsec->floorpic);
				//quad.flags = RPF_MISSING_WALL;
			}
			quad.detail = texdetail;
			// Calculate texture coordinates.
			quad.texoffy = tcyoff;
			if(ldef->flags & ML_DONTPEGBOTTOM)
			{
				// Lower unpegged. Align with normal middle texture.
				quad.texoffy += fceil - bfloor;
			}
			quad.flags = 0;
			if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;
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
			quad.texw = texw;
			quad.texh = texh;

			// Might be necessary.
			Rend_WallHeightDivision(&quad, seg, frontsec, WHD_BOTTOM);
			
			RL_AddPoly(&quad);
		}
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
			if(backh[0] > fronth[0] && vy < backh[0]
				|| backh[0] < fronth[0] && vy > fronth[0])
			{
				C_AddViewRelOcclusion(startv, endv, 
					MAX_OF(fronth[0], backh[0]), false); // Occlude down.
			}
		}
		// Do not create an occlusion for sky ceilings.
		if(back->ceilingpic != skyflatnum || front->ceilingpic != skyflatnum)
		{
			// Do the ceilings create an occlusion?
			if(backh[1] < fronth[1] && vy > backh[1]
				|| backh[1] > fronth[1] && vy < fronth[1])
			{
				C_AddViewRelOcclusion(startv, endv, 
					MIN_OF(fronth[1], backh[1]), true); // Occlude up.
			}
		}
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
	sector_t		*sect = ssec->sector, *link;
	int				sectoridx = GET_SECTOR_IDX(sect);
	rendpoly_t		poly;
	int				sectorlight;
	float			height;
	int				flags = 0;
	short			planepic;
	sectorinfo_t	*sin = secinfo + GET_SECTOR_IDX(sect);
	float			sceil = sin->visceiling, sfloor = sin->visfloor;
	lumobj_t		*lumi;	// Lum Iterator, or 'snow' in Finnish.
	
	if(sceil - sfloor <= 0) 
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

	// Mark the sector visible for this frame.
	sin->flags |= SIF_VISIBLE;

	// Dynamic lights. Processes both the ceiling and the floor, and all
	// visible wall segments. First clear the necessary flags.
	ssec->flags &= ~DDSUBF_CLEAR_MASK;
	for(i = 0, seg = segs+SEGIDX(ssec->firstline); i < ssec->linecount; 
		i++, seg += SEGSIZE) ((seg_t*)seg)->flags &= ~DDSUBF_CLEAR_MASK;

	if(useDynLights) 
		DL_ProcessSubsector(&poly, ssec);

/*	if(useShadows)
		Rend_SubsectorShadows(ssec); */

	Rend_OccludeSubsector(ssec, false);
	// Determine which dynamic light sources in the subsector get clipped.
	for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssnext)
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

/*	else if(haloMode)
	{
		// Flares are on, but dynamic lights aren't processed. We must
		// mark the rendered luminous objects ourselves.
		for(i = 0; i < numLuminous; i++)
		{
			if(luminousList[i].thing->subsector == ssec)
				luminousList[i].flags |= LUMF_RENDERED;
		}
	}*/
	// Mark the particle generators in the sector visible.
	PG_SectorIsVisible(sect);

	// Sprites for this sector have to be drawn. This must be done before
	// the segments of this subsector are added to the clipper. Otherwise
	// the sprites would get clipped by them, and that wouldn't be right.
	R_AddSprites(sect);

	// Draw the walls.
	for(i = 0, seg = segs+SEGIDX(ssec->firstline); i < ssec->linecount; 
		i++, seg += SEGSIZE)
	{
		if(((seg_t*)seg)->linedef)	// "minisegs" have no linedefs.
			Rend_RenderWallSeg((seg_t*)seg, sect, flags);
	}

	// Is there a polyobj on board?
	if(ssec->poly)
		for(i=0; i<ssec->poly->numsegs; i++)
			Rend_RenderWallSeg(ssec->poly->segs[i], sect, 0);

	sectorlight = Rend_SectorLight(sect);

	// The floor.
	memset(&poly, 0, sizeof(poly));
	poly.type = rp_flat;
	if(ssec->flags & DDSUBF_DLIGHT_FLOOR) poly.flags = RPF_DLIT;
	// Determine the height of the floor.
	if(sin->linkedfloor)
	{
		link = R_GetLinkedSector(sin->linkedfloor, true);
		// This sector has an invisible floor.
		height = SECT_FLOOR(link);
		poly.vertices[0].color.rgb[CR] = Rend_SectorLight(link);
		planepic = link->floorpic;
	}
	else
	{
		height = sfloor;
		poly.vertices[0].color.rgb[CR] = sectorlight;
		planepic = sect->floorpic;
	}
	if(R_FlatFlags(planepic) & TXF_GLOW) poly.flags |= RPF_GLOW;
	if(vy > height) 
	{
		// Check for sky... in the floor?
		if(planepic == skyflatnum) 
		{
			poly.flags |= RPF_SKY_MASK;
			skyhemispheres |= SKYHEMI_LOWER;
		}
		else
		{
			poly.tex = curtex = GL_PrepareFlat(planepic);
			poly.detail = texdetail;
			poly.texw = texw;
			poly.texh = texh;
			poly.texoffx = sect->flooroffx;
			poly.texoffy = sect->flooroffy;
		}
		poly.top = height;
		RL_PrepareFlat(&poly, 0, 0, RLPF_NORMAL, ssec);
		RL_AddPoly(&poly);
	}
	// And the roof.
	poly.flags = 0;
	if(ssec->flags & DDSUBF_DLIGHT_CEILING) poly.flags = RPF_DLIT;
	// Determine the height of the ceiling.
	if(sin->linkedceiling)
	{
		link = R_GetLinkedSector(sin->linkedceiling, false);
		// This sector has an invisible ceiling.
		height = SECT_CEIL(link);
		poly.vertices[0].color.rgb[CR] = Rend_SectorLight(link);
		planepic = link->ceilingpic;
	}
	else
	{
		height = sceil;
		poly.vertices[0].color.rgb[CR] = sectorlight;
		planepic = sect->ceilingpic;
	}
	if(R_FlatFlags(planepic) & TXF_GLOW) poly.flags |= RPF_GLOW;
	if(vy < height) 
	{
		// Check for sky.
		if(planepic == skyflatnum) 
		{
			poly.flags |= RPF_SKY_MASK;
			skyhemispheres |= SKYHEMI_UPPER;
		}
		else
		{
			poly.tex = curtex = GL_PrepareFlat(planepic);
			poly.detail = texdetail;
			poly.texw = texw;
			poly.texh = texh;
			poly.texoffx = sect->ceiloffx;
			poly.texoffy = sect->ceiloffy;
		}
		poly.top = height + sect->skyfix;
		// The first vertex is always the last in the whole list.
		RL_PrepareFlat(&poly, 0, 0, RLPF_REVERSE, ssec);
		RL_AddPoly(&poly);
	}
}

//===========================================================================
// Rend_RenderNode
//===========================================================================
void Rend_RenderNode(int bspnum)
{
	node_t          *bsp;
	int             side;

	// If the clipper is full we're pretty much done.
	if(cliphead)
		if(cliphead->start == 0 && cliphead->end == BANG_MAX)
			return;

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

	// Set to true if dynlights are inited for this frame.
	dlInited = false;

	// This is all the clearing we'll do.
	gl.Clear(DGL_DEPTH_BUFFER_BIT);

	// Setup the modelview matrix.
	Rend_ModelViewMatrix(true);

	if(!freezeRLs)
	{
		// Prepare for rendering.
		R_UpdatePlanes();		// Update all planes.
		RL_ClearLists();		// Clear the lists for new quads.
		C_ClearRanges();		// Clear the clipper.
		R_ClearSectorFlags();
		DL_ClearForFrame();		// Zeroes the links.

		// Maintain luminous objects.
		if(useDynLights || haloMode || litSprites) DL_InitForNewFrame(); 

		// Add the backside clipping range (if vpitch allows).
		if(vpitch <= 90-yfov/2 && vpitch >= -90+yfov/2)
		{
			float a = fabs(vpitch) / (90-yfov/2);
			//binangle_t startAngle = (binangle_t) BANG_45*(1+a);
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

		Rend_RenderNode(numnodes-1);		

		Rend_RenderShadows();
	}
	RL_RenderAllLists();
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
		byte col[4];
		for(i=0; i<3; i++)
			col[i] = strtol(argv[2+i], NULL, 0)/*/255.0f*/;
		col[3] = 1;
		gl.Fogv(DGL_FOG_COLOR, col);
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
