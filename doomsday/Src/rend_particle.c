/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * rend_particle.c: Particle Effects
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Point + custom textures.
#define NUM_TEX_NAMES (1 + MAX_PTC_TEXTURES)

// TYPES -------------------------------------------------------------------

typedef struct pglink_s
{
	struct pglink_s	*next;
	ptcgen_t *gen;
} pglink_t;

typedef struct
{
	unsigned char	gen;		// Index of the generator (active_ptcgens)
	short			index;
	fixed_t			distance;
} porder_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_RotateVector(float vec[3], float yaw, float pitch);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float	vang, vpitch;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

DGLuint			ptctexname[NUM_TEX_NAMES]; 
int				rend_particle_nearlimit = 0;
float			rend_particle_diffuse = 4;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static pglink_t **pgLinks;	// Array of pointers to pgLinks in pgStore.
static pglink_t *pgStore;
static int		pgCursor, pgMax;
static uint		orderSize;
static porder_t	*order;
static int		numParts;
static boolean	hasPoints[NUM_TEX_NAMES], hasLines, hasNoBlend, hasBlend;
static boolean	hasModels;

// CODE --------------------------------------------------------------------

//===========================================================================
// PG_PointDist
//===========================================================================
static fixed_t PG_PointDist(fixed_t c[3])
{
	fixed_t dist = FixedMul(viewy - c[VY], -viewsin) //viewsidex 
		- FixedMul(viewx - c[VX], viewcos);
	if(dist < 0) return -dist;	// Always return positive.
	return dist;

	// Approximate the 3D distance to the point.
	/*return M_AproxDistance(M_AproxDistance(c[VX] - viewx, c[VY] - viewy),
		c[VZ] - viewz);*/
}

//===========================================================================
// PG_InitTextures
//	The particle texture is a modification of the dynlight texture.
//===========================================================================
void PG_InitTextures(void)
{
	// We need to generate the texture, I see.
	byte *image = Z_Malloc(64 * 64, PU_STATIC, 0);
	byte *data = W_CacheLumpName("DLIGHT", PU_CACHE);
	int i;
	boolean reported = false;

	if(ptctexname[0]) return; // Already been here.

	// Clear the texture names array.
	memset(ptctexname, 0, sizeof(ptctexname));

	if(!data) Con_Error("PG_InitTextures: No DLIGHT texture.\n");

	// Mipmap it down to 32x32 and create an alpha mask.
	memcpy(image, data, 64 * 64);
	GL_DownMipmap32(image, 64, 64, 1);
	memcpy(image + 32 * 32, image, 32 * 32);
	memset(image, 255, 32 * 32);		

	// No further mipmapping or resizing is needed, upload directly.
	// The zeroth texture is the default: a blurred point.
	ptctexname[0] = gl.NewTexture();
	gl.TexImage(DGL_LUMINANCE_PLUS_A8, 32, 32, 0, image);
	gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
	gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
	gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
	gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

	Z_Free(image);

	// Load any custom particle textures. They are loaded from the 
	// highres texture directory and are named "ParticleNN.(tga|png|pcx)".
	// The first is "Particle00". (based on Leesz' textured particles mod)
	for(i = 0; i < MAX_PTC_TEXTURES; i++)
	{
		char filename[80];
		image_t image;

		// Try to load the texture.
		sprintf(filename, "Particle%02i", i);
		if(GL_LoadTexture(&image, filename) == NULL)
		{
			// Just show the first 'not found'.
			if(verbose && !reported) 
			{
				Con_Message("PG_InitTextures: %s not found.\n", filename);
			}
			reported = true;
			continue;
		}

		VERBOSE( Con_Message("PG_InitTextures: Texture %02i: %i * %i * %i\n",
			i, image.width, image.height, image.pixelSize) );

		// If the source is 8-bit with no alpha, generate alpha automatically.
		if(image.originalBits == 8)
		{
			GL_ConvertToAlpha(&image, true);
		}

		// Create a new texture and upload the image.
		ptctexname[i + 1] = gl.NewTexture();

		gl.Disable(DGL_TEXTURE_COMPRESSION);
		gl.TexImage(image.pixelSize == 4? DGL_RGBA 
			: image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8
			: DGL_RGB, 
			image.width, image.height, 0, image.pixels);
		gl.Enable(DGL_TEXTURE_COMPRESSION);

		gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		// Free the buffer.
		GL_DestroyImage(&image);
	}
}

//===========================================================================
// PG_ShutdownTextures
//===========================================================================
void PG_ShutdownTextures(void)
{
	gl.DeleteTextures(NUM_TEX_NAMES, ptctexname);
	memset(ptctexname, 0, sizeof(ptctexname));
}

//===========================================================================
// PG_InitForLevel
//===========================================================================
void PG_InitForLevel(void)
{
	pgLinks = Z_Malloc(sizeof(pglink_t*) * numsectors, PU_LEVEL, 0);
	memset(pgLinks, 0, sizeof(pglink_t*) * numsectors);

	// We can link 64 generators each into four sectors before
	// running out of pgLinks. 
	pgMax = 4 * MAX_ACTIVE_PTCGENS;
	pgStore = Z_Malloc(sizeof(pglink_t) * pgMax, PU_LEVEL, 0);
	pgCursor = 0;

	memset(active_ptcgens, 0, sizeof(active_ptcgens));

	// Allocate a small ordering buffer.
	orderSize = 256;
	order = Z_Malloc(sizeof(porder_t) * orderSize, PU_LEVEL, 0);
}

//===========================================================================
// PG_GetLink
//	Returns an unused link from the pgStore.
//===========================================================================
static pglink_t *PG_GetLink(void)
{
	if(pgCursor == pgMax) 
	{
		if(verbose) Con_Message("PG_GetLink: Out of PGen store.\n");
		return NULL;
	}
	return &pgStore[ pgCursor++ ];
}

//===========================================================================
// PG_LinkPtcGen
//===========================================================================
void PG_LinkPtcGen(ptcgen_t *gen, sector_t *sector)
{
	int si = GET_SECTOR_IDX(sector);
	pglink_t *link;
	pglink_t *it;

	// Must check that it isn't already there...
	for(it = pgLinks[si]; it; it = it->next)
		if(it->gen == gen) return; // No, no...
	// We need a new PG link.
	link = PG_GetLink();
	if(!link) return;  // Out of links!
	link->gen = gen;
	link->next = pgLinks[si];
	pgLinks[si] = link;
}

//===========================================================================
// PG_InitForNewFrame
//	Init all active particle generators for a new frame.
//===========================================================================
void PG_InitForNewFrame(void)
{
	int	i, k;
	ptcgen_t *gen;
	
	// Clear the PG links.
	memset(pgLinks, 0, sizeof(pglink_t*) * numsectors);
	pgCursor = 0;

	// Clear all visibility flags.
	for(i = 0; i < MAX_ACTIVE_PTCGENS; i++)
		if((gen = active_ptcgens[i]) != NULL)
		{
			gen->flags &= ~PGF_VISIBLE;
			// FIXME: Overkill? 
			for(k = 0; k < gen->count; k++)
				if(gen->ptcs[k].stage >= 0)
					PG_LinkPtcGen(gen, gen->ptcs[k].sector);
		}
}

//===========================================================================
// PG_SectorIsVisible
//	The given sector is visible. All PGs in it should be rendered.
//	Scans PG links.
//===========================================================================
void PG_SectorIsVisible(sector_t *sector)
{
	pglink_t *it = pgLinks[GET_SECTOR_IDX(sector)];
	for(; it; it = it->next) it->gen->flags |= PGF_VISIBLE;
}

//===========================================================================
// PG_Sorter
//	Sorts in descending order.
//===========================================================================
int C_DECL PG_Sorter(const void *pt1, const void *pt2)
{
	if(((porder_t*)pt1)->distance > ((porder_t*)pt2)->distance)
		return -1;
	else if(((porder_t*)pt1)->distance < ((porder_t*)pt2)->distance)
		return 1;
	// Highly unlikely (but possible)...
	return 0;
}

//===========================================================================
// PG_CheckOrderBuffer
//	Allocate more memory for the particle ordering buffer, if necessary.
//===========================================================================
void PG_CheckOrderBuffer(uint max)
{
	while(max > orderSize) orderSize *= 2;
	order = Z_Realloc(order, sizeof(*order) * orderSize, PU_LEVEL);
}

//===========================================================================
// PG_ListVisibleParticles
//	Returns true iff there are particles to render.
//===========================================================================
int PG_ListVisibleParticles(void)
{
	int i, p, m, stagetype;
	fixed_t maxdist, mindist = FRACUNIT * rend_particle_nearlimit; 
	ptcgen_t *gen;
	ded_ptcgen_t *def;
	particle_t *pt;

	hasModels = hasLines = hasBlend = hasNoBlend = false;
	memset(hasPoints, 0, sizeof(hasPoints));

	// First count how many particles are in the visible generators.
	for(i = 0, numParts = 0; i < MAX_ACTIVE_PTCGENS; i++)
	{
		gen = active_ptcgens[i];
		if(!gen || !(gen->flags & PGF_VISIBLE)) continue;
		for(p = 0; p < gen->count; p++)
			if(gen->ptcs[p].stage >= 0) numParts++;
	}
	if(!numParts) return false;
	
	// Allocate the rendering order list.
	PG_CheckOrderBuffer(numParts);

	// Fill in the order list and see what kind of particles we'll
	// need to render.
	for(i = 0, m = 0; i < MAX_ACTIVE_PTCGENS; i++)
	{
		gen = active_ptcgens[i];
		if(!gen || !(gen->flags & PGF_VISIBLE)) continue;
		def = gen->def;
		maxdist = FRACUNIT * def->maxdist;
		for(p = 0, pt = gen->ptcs; p < gen->count; p++, pt++)
		{
			if(pt->stage < 0) continue;
			// Is the particle's sector visible?
			if(!(secinfo[GET_SECTOR_IDX(pt->sector)].flags & SIF_VISIBLE))
				continue; // No; this particle can't be seen.
			order[m].gen = i;
			order[m].index = p;
			order[m].distance = PG_PointDist(pt->pos);
			// Don't allow zero distance.
			if(!order[m].distance) order[m].distance = 1;
			if(maxdist && order[m].distance > maxdist) continue; // Too far.
			if(order[m].distance < mindist) continue; // Too near.

			stagetype = gen->stages[pt->stage].type;
			if(stagetype == PTC_POINT)
			{
				hasPoints[0] = true;
			}
			else if(stagetype == PTC_LINE)
			{
				hasLines = true;
			}
			else if(stagetype >= PTC_TEXTURE
				&& stagetype < PTC_TEXTURE + MAX_PTC_TEXTURES)
			{
				hasPoints[stagetype - PTC_TEXTURE + 1] = true;
			}
			else if(stagetype >= PTC_MODEL
				&& stagetype < PTC_MODEL + MAX_PTC_MODELS)
			{
				hasModels = true;
			}

			if(gen->flags & PGF_ADD_BLEND)
				hasBlend = true;
			else
				hasNoBlend = true;
			m++;
		}
	}
	if(!m) 
	{
		// No particles left (all too far?).
		return false; 
	}

	// This is the real number of possibly visible particles.
	numParts = m;

	// Sort the order list back->front. A quicksort is fast enough.
	qsort(order, numParts, sizeof(*order), PG_Sorter);
	return true;
}

//===========================================================================
// PG_RenderParticles
//===========================================================================
void PG_RenderParticles(int rtype, boolean with_blend)
{
	float			leftoff[3], rightoff[3], mark, inv_mark;
	float			size, color[4], center[3];
	float			dist, maxdist;
	float			line[2], projected[2];
	fixed_t			fixline[2];
	ptcgen_t		*gen;
	ptcstage_t		*st;
	particle_t		*pt;
	ded_ptcstage_t	*dst, *next_dst;
	int				i, c;
	int				using_texture = -1;
	int				prim_type;
	blendmode_t		mode = BM_NORMAL, new_mode;
	vissprite_t		vis;

	if(rtype == PTC_MODEL)
	{
		// Prepare a dummy vissprite.
		memset(&vis, 0, sizeof(vis));
	}

	// viewsidevec points to the left.
	for(i = 0; i < 3; i++)
	{
		leftoff[i] = viewupvec[i] + viewsidevec[i];
		rightoff[i] = viewupvec[i] - viewsidevec[i];
	}

	// Should we use a texture?
	if(rtype == PTC_POINT)
		using_texture = 0;
	else if(rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES)
		using_texture = rtype - PTC_TEXTURE + 1;

	if(rtype == PTC_MODEL)
	{
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
	}
	else if(using_texture >= 0)
	{
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Disable(DGL_CULL_FACE);
		gl.Bind(ptctexname[using_texture]);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		gl.Begin(prim_type = DGL_QUADS); 
	}
	else
	{
		gl.Disable(DGL_TEXTURING);	// Lines don't use textures.
		gl.Begin(prim_type = DGL_LINES);
	}

	// How many particles can we render?
	if(r_max_particles)
	{
		i = numParts - r_max_particles;
		if(i < 0) i = 0;
	}
	else i = 0;
	for(; i < numParts; i++)
	{
		gen = active_ptcgens[order[i].gen];
		pt = gen->ptcs + order[i].index;
		st = &gen->stages[pt->stage];
		dst = &gen->def->stages[pt->stage];		

		// Only render one type of particles.
		if((rtype == PTC_MODEL && dst->model < 0) ||
		   (rtype != PTC_MODEL && st->type != rtype))
		{
			continue;
		}	
		if(!(gen->flags & PGF_ADD_BLEND) == with_blend) continue;

		if(rtype != PTC_MODEL && !with_blend)
		{
			// We may need to change the blending mode.
			new_mode = 
				 (gen->flags & PGF_SUB_BLEND? BM_SUBTRACT
				: gen->flags & PGF_REVSUB_BLEND? BM_REVERSE_SUBTRACT
				: gen->flags & PGF_MUL_BLEND? BM_MUL
				: gen->flags & PGF_INVMUL_BLEND? BM_INVERSE_MUL
				: BM_NORMAL);
			if(new_mode != mode)
			{
				gl.End();
				GL_BlendMode(mode = new_mode);
				gl.Begin(prim_type);
			}
		}

		// Is there a next stage for this particle?
		if(pt->stage >= MAX_PTC_STAGES-1
			|| !gen->stages[pt->stage + 1].type)
		{
			// There is no "next stage". Use the current one.
			next_dst = gen->def->stages + pt->stage;
		}
		else
			next_dst = gen->def->stages + (pt->stage + 1);
		
		// Where is intermark?
		inv_mark = pt->tics / (float) dst->tics;
		mark = 1 - inv_mark;		
		
		// Calculate size and color.
		size = P_GetParticleRadius(dst, order[i].index) * inv_mark 
			+ P_GetParticleRadius(next_dst, order[i].index) * mark;
		if(!size) continue; // Infinitely small.
		for(c = 0; c < 4; c++)
		{
			color[c] = dst->color[c]*inv_mark + next_dst->color[c]*mark;
			if(!(st->flags & PTCF_BRIGHT) && c < 3 && !LevelFullBright) 
			{
				// This is a simplified version of sectorlight (no distance
				// attenuation).
				color[c] *= pt->sector->lightlevel / 255.0f;
			}
		}

		maxdist = gen->def->maxdist;
		dist = FIX2FLT(order[i].distance);
		// Far diffuse?
		if(maxdist)
		{
			if(dist > maxdist*.75f) 
				color[3] *= 1 - (dist - maxdist*.75f) / (maxdist*.25f);
		}
		// Near diffuse?
		if(rend_particle_diffuse > 0)
		{			
			if(dist < rend_particle_diffuse * size)
				color[3] -= 1 - dist/(rend_particle_diffuse * size);
		}
	
		// Fully transparent?
		if(color[3] <= 0) continue; 

		gl.Color4fv(color);

		center[VX] = FIX2FLT(pt->pos[VX]);
		center[VZ] = FIX2FLT(pt->pos[VY]);
		center[VY] = FIX2FLT(P_GetParticleZ(pt));

		// Model particles are rendered using the normal model rendering 
		// routine.
		if(rtype == PTC_MODEL && dst->model >= 0)
		{
			int frame;
			// Render the particle as a model.
			vis.type = VSPR_PARTICLE_MODEL;
			vis.distance = dist;
			vis.data.mo.subsector = R_PointInSubsector(pt->pos[VX],
													   pt->pos[VY]);
			vis.data.mo.gx = FRACUNIT * center[VX];
			vis.data.mo.gy = FRACUNIT * center[VZ];
			vis.data.mo.gz = vis.data.mo.gzt = FRACUNIT * center[VY];
			vis.data.mo.v1[0] = center[VX];
			vis.data.mo.v1[1] = center[VZ];
			vis.data.mo.v2[0] = size; // Extra scaling factor.
			vis.data.mo.mf = &models[dst->model];
			if(dst->end_frame < 0)
			{
				frame = dst->frame;
				vis.data.mo.inter = 0;
			}
			else
			{
				frame = dst->frame + (dst->end_frame - dst->frame)
					* mark;
				vis.data.mo.inter = M_CycleIntoRange(mark * (dst->end_frame
					- dst->frame), 1);
			}
			R_SetModelFrame(vis.data.mo.mf, frame);
			// Set the correct orientation for the particle.
			if(vis.data.mo.mf->sub[0].flags & MFF_MOVEMENT_YAW)
			{
				vis.data.mo.yaw = R_MovementYaw(pt->mov[0], pt->mov[1]);
			}
			else
			{
				vis.data.mo.yaw = pt->yaw / 32768.0f * 180;
			}
			if(vis.data.mo.mf->sub[0].flags & MFF_MOVEMENT_PITCH)
			{
				vis.data.mo.pitch = R_MovementPitch(pt->mov[0], pt->mov[1],
					pt->mov[2]);
			}
			else
			{
				vis.data.mo.pitch = pt->pitch / 32768.0f * 180;
			}
			if(st->flags & PTCF_BRIGHT || LevelFullBright) 
				vis.data.mo.lightlevel = -1; // Fullbright.
			else 
				vis.data.mo.lightlevel = pt->sector->lightlevel;
			memcpy(vis.data.mo.rgb, R_GetSectorLightColor(pt->sector), 3);
			vis.data.mo.alpha = color[3];

			Rend_RenderModel(&vis);
			continue;
		}

		// The vertices, please.
		if(using_texture >= 0)
		{
			// Should the particle be flat against a plane?			
			if(st->flags & PTCF_PLANE_FLAT
				&& pt->sector
				&& (pt->sector->floorheight + 2*FRACUNIT >= pt->pos[VZ]
					|| pt->sector->ceilingheight - 2*FRACUNIT <= pt->pos[VZ]))
			{
				gl.TexCoord2f(0, 0);
				gl.Vertex3f(center[VX] - size, center[VY], center[VZ] - size);

				gl.TexCoord2f(1, 0);
				gl.Vertex3f(center[VX] + size, center[VY], center[VZ] - size);

				gl.TexCoord2f(1, 1);
				gl.Vertex3f(center[VX] + size, center[VY], center[VZ] + size);

				gl.TexCoord2f(0, 1);
				gl.Vertex3f(center[VX] - size, center[VY], center[VZ] + size);
			}
			// Flat against a wall, then?
			else if(st->flags & PTCF_WALL_FLAT 
				&& pt->contact
				&& !pt->mov[VX] && !pt->mov[VY])
			{
				// There will be a slight approximation on the XY plane since
				// the particles aren't that accurate when it comes to wall
				// collisions.

				// Calculate a new center point (project onto the wall).
				// Also move 1 unit away from the wall to avoid the worst
				// Z-fighting.
				fixline[VX] = pt->contact->dx;
				fixline[VY] = pt->contact->dy;
				M_ProjectPointOnLinef(pt->pos, &pt->contact->v1->x, 
					fixline, 1, projected);
								
				P_LineUnitVector(pt->contact, line);

				gl.TexCoord2f(0, 0);
				gl.Vertex3f(projected[VX] - size*line[VX], 
					center[VY] - size, 
					projected[VY] - size*line[VY]);

				gl.TexCoord2f(1, 0);
				gl.Vertex3f(projected[VX] - size*line[VX], 
					center[VY] + size, 
					projected[VY] - size*line[VY]);

				gl.TexCoord2f(1, 1);
				gl.Vertex3f(projected[VX] + size*line[VX], 
					center[VY] + size, 
					projected[VY] + size*line[VY]);

				gl.TexCoord2f(0, 1);
				gl.Vertex3f(projected[VX] + size*line[VX], 
					center[VY] - size, 
					projected[VY] + size*line[VY]);
			}
			else
			{
				gl.TexCoord2f(0, 0);
				gl.Vertex3f(center[VX] + size*leftoff[VX],
					center[VY] + size*leftoff[VY]/1.2f,
					center[VZ] + size*leftoff[VZ]);

				gl.TexCoord2f(1, 0);
				gl.Vertex3f(center[VX] + size*rightoff[VX],
					center[VY] + size*rightoff[VY]/1.2f,
					center[VZ] + size*rightoff[VZ]);

				gl.TexCoord2f(1, 1);
				gl.Vertex3f(center[VX] - size*leftoff[VX],
					center[VY] - size*leftoff[VY]/1.2f,
					center[VZ] - size*leftoff[VZ]);

				gl.TexCoord2f(0, 1);
				gl.Vertex3f(center[VX] - size*rightoff[VX],
					center[VY] - size*rightoff[VY]/1.2f,
					center[VZ] - size*rightoff[VZ]);
			}
		}
		else // It's a line.
		{
			gl.Vertex3f(center[VX], center[VY], center[VZ]);
			gl.Vertex3f(center[VX] - FIX2FLT(pt->mov[VX]),
				center[VY] - FIX2FLT(pt->mov[VZ]),
				center[VZ] - FIX2FLT(pt->mov[VY]));
		}
	}
	
	if(rtype != PTC_MODEL) 
	{
		gl.End();

		if(using_texture >= 0) 
		{
			gl.Enable(DGL_DEPTH_WRITE);
			gl.Enable(DGL_CULL_FACE);
			gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		}
		else
		{
			gl.Enable(DGL_TEXTURING);
		}
	}

	if(!with_blend) 
	{
		// We may have rendered subtractive stuff.
		GL_BlendMode(BM_NORMAL);
	}
}

//===========================================================================
// PG_RenderPass
//===========================================================================
void PG_RenderPass(boolean use_blending)
{
	int i;

	// Set blending mode.
	if(use_blending) GL_BlendMode(BM_ADD);

	if(hasModels)
		PG_RenderParticles(PTC_MODEL, use_blending);

	if(hasLines) 
		PG_RenderParticles(PTC_LINE, use_blending);
	
	for(i = 0; i < NUM_TEX_NAMES; i++)
		if(hasPoints[i]) 
		{
			PG_RenderParticles(!i? PTC_POINT : PTC_TEXTURE + i - 1, 
				use_blending);
		}

	// Restore blending mode.
	if(use_blending) GL_BlendMode(BM_NORMAL);
}

//===========================================================================
// PG_Render
//	Render all the visible particle generators. 
//	We must render all particles ordered back->front, or otherwise 
//	particles from one generator will obscure particles from another.
//	This would be especially bad with smoke trails.
//===========================================================================
void PG_Render(void)
{
	if(!r_use_particles) return;
	if(!PG_ListVisibleParticles()) return; // No visible particles at all?
	
	// Render all the visible particles.
	if(hasNoBlend)
	{
		PG_RenderPass(false);
	}
	if(hasBlend)
	{
		// A second pass with additive blending. 
		// This makes the additive particles 'glow' through all other 
		// particles.
		PG_RenderPass(true);
	}
}
