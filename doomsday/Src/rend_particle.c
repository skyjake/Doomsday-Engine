
//**************************************************************************
//**
//** REND_PARTICLE.C
//**
//** Rendering of particle generators.
//**
//** $Log$
//** Revision 1.11  2003/08/15 22:18:44  skyjake
//** Added subtractive blending, 8-bit noalpha textures converted to pure alpha
//**
//** Revision 1.10  2003/08/10 00:01:54  skyjake
//** Follow the Begin/End semantics
//**
//** Revision 1.9  2003/06/03 15:21:30  skyjake
//** Fixed font problem
//**
//** Revision 1.8  2003/05/30 16:34:11  skyjake
//** image_t added, extres for raw screens
//**
//** Revision 1.7  2003/05/25 19:28:38  skyjake
//** Fixed bug 743201
//**
//** Revision 1.6  2003/05/23 22:07:54  skyjake
//** Modified verbosity
//**
//** Revision 1.5  2003/04/16 09:49:42  skyjake
//** -nohightex must only affect walls/flats
//**
//** Revision 1.4  2003/03/14 15:42:38  skyjake
//** Only report first missing Particle*
//**
//** Revision 1.3  2003/02/28 13:44:34  skyjake
//** Removed code that was commented out
//**
//** Revision 1.2  2003/02/28 10:42:15  skyjake
//** Added textured particles, planeflat sticks to planes
//**
//**************************************************************************

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

static pglink_t **pglinks;	// Array of pointers to pglinks in pgstore.
static pglink_t *pgstore;
static int		pgcursor, pgmax;
static porder_t	*order;
static int		numparts;
static boolean	haspoints[NUM_TEX_NAMES], haslines, hasnoblend, hasblend;

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
				Con_Message("PG_InitTextures: %s not found.\n", filename);
			reported = true;
			continue;
		}

		VERBOSE( Con_Message("PG_InitTextures: Texture %02i: %i * %i * %i\n",
			i, image.width, image.height, image.pixelSize) );

		// If there is no alpha data, generate it automatically.
		if(image.pixelSize == 3)
		{
			int p, total = image.width * image.height;
			byte *ptr = image.pixels;
			for(p = 0; p < total; p++, ptr += 3)
			{
				image.pixels[p] = (ptr[0] + ptr[1] + ptr[2]) / 3;
			}
			for(p = 0; p < total; p++)
			{
				// Move the average color to the alpha channel, make the
				// actual color white.
				image.pixels[total + p] = image.pixels[p];
				image.pixels[p] = 255;
			}
			image.pixelSize = 2;
		}

		// Create a new texture and upload the image.
		ptctexname[i + 1] = gl.NewTexture();

		gl.TexImage(image.pixelSize == 4? DGL_RGBA 
			: image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8
			: DGL_RGB, 
			image.width, image.height, 0, image.pixels);
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
	pglinks = Z_Malloc(sizeof(pglink_t*) * numsectors, PU_LEVEL, 0);
	memset(pglinks, 0, sizeof(pglink_t*) * numsectors);

	// We can link 64 generators each into four sectors before
	// running out of pglinks. 
	pgmax = 4 * MAX_ACTIVE_PTCGENS;
	pgstore = Z_Malloc(sizeof(pglink_t) * pgmax, PU_LEVEL, 0);
	pgcursor = 0;

	memset(active_ptcgens, 0, sizeof(active_ptcgens));
}

//===========================================================================
// PG_GetLink
//	Returns an unused link from the pgstore.
//===========================================================================
static pglink_t *PG_GetLink(void)
{
	if(pgcursor == pgmax) 
	{
		if(verbose) Con_Message("PG_GetLink: Out of PGen store.\n");
		return NULL;
	}
	return &pgstore[ pgcursor++ ];
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
	for(it = pglinks[si]; it; it = it->next)
		if(it->gen == gen) return; // No, no...
	// We need a new PG link.
	link = PG_GetLink();
	if(!link) return;  // Out of links!
	link->gen = gen;
	link->next = pglinks[si];
	pglinks[si] = link;
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
	memset(pglinks, 0, sizeof(pglink_t*) * numsectors);
	pgcursor = 0;

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
	pglink_t *it = pglinks[GET_SECTOR_IDX(sector)];
	for(; it; it = it->next) it->gen->flags |= PGF_VISIBLE;
}

//===========================================================================
// PG_Sorter
//	Sorts in descending order.
//===========================================================================
int __cdecl PG_Sorter(const void *pt1, const void *pt2)
{
	if(((porder_t*)pt1)->distance > ((porder_t*)pt2)->distance)
		return -1;
	else if(((porder_t*)pt1)->distance < ((porder_t*)pt2)->distance)
		return 1;
	// Highly unlikely (but possible)...
	return 0;
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

	haslines = hasblend = hasnoblend = false;
	memset(haspoints, 0, sizeof(haspoints));

	// First count how many particles are in the visible generators.
	for(i = 0, numparts = 0; i < MAX_ACTIVE_PTCGENS; i++)
	{
		gen = active_ptcgens[i];
		if(!gen || !(gen->flags & PGF_VISIBLE)) continue;
		for(p = 0; p < gen->count; p++)
			if(gen->ptcs[p].stage >= 0) numparts++;
	}
	if(!numparts) return false;
	
	// Allocate the rendering order list.
	order = Z_Malloc(sizeof(*order) * numparts, PU_STATIC, 0);

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
				haspoints[0] = true;
			}
			else if(stagetype == PTC_LINE)
			{
				haslines = true;
			}
			else if(stagetype >= PTC_TEXTURE
				&& stagetype < PTC_TEXTURE + MAX_PTC_TEXTURES)
			{
				haspoints[stagetype - PTC_TEXTURE + 1] = true;
			}

			if(gen->flags & PGF_ADD_BLEND)
				hasblend = true;
			else
				hasnoblend = true;
			m++;
		}
	}
	if(!m) 
	{
		Z_Free(order);
		return false; // No particles left (all too far?).
	}

	// This is the real number of possibly visible particles.
	numparts = m;

	// Sort the order list back->front. A quicksort is fast enough.
	qsort(order, numparts, sizeof(*order), PG_Sorter);
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

	if(using_texture >= 0)
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
		i = numparts - r_max_particles;
		if(i < 0) i = 0;
	}
	else i = 0;
	for(; i < numparts; i++)
	{
		gen = active_ptcgens[order[i].gen];
		pt = gen->ptcs + order[i].index;

		// Only render one type of particles.
		if(gen->stages[pt->stage].type != rtype) continue;
		if(!(gen->flags & PGF_ADD_BLEND) == with_blend) continue;

		if(!with_blend)
		{
			// We may need to change the blending mode.
			new_mode = 
				 (gen->flags & PGF_SUB_BLEND? BM_SUBTRACT
				: gen->flags & PGF_REVSUB_BLEND? BM_REVERSE_SUBTRACT
				: BM_NORMAL);
			if(new_mode != mode)
			{
				gl.End();
				Rend_BlendMode(mode = new_mode);
				gl.Begin(prim_type);
			}
		}

		dst = &gen->def->stages[pt->stage];
		st = &gen->stages[pt->stage];

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
	Rend_BlendMode(BM_NORMAL);
}

//===========================================================================
// PG_RenderPass
//===========================================================================
void PG_RenderPass(boolean use_blending)
{
	int i;

	// Set blending mode.
	if(use_blending) 
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);

	if(haslines) 
		PG_RenderParticles(PTC_LINE, use_blending);
	
	for(i = 0; i < NUM_TEX_NAMES; i++)
		if(haspoints[i]) 
		{
			PG_RenderParticles(!i? PTC_POINT : PTC_TEXTURE + i - 1, 
				use_blending);
		}

	// Restore blending mode.
	if(use_blending)
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
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
	if(hasnoblend)
	{
		PG_RenderPass(false);
	}
	if(hasblend)
	{
		// A second pass with additive blending. 
		// This makes the additive particles 'glow' through all other 
		// particles.
		PG_RenderPass(true);
	}

	// Free the list allocated in PG_ListVisibleParticles.
	// FIXME: Is this necessary?
	Z_Free(order);
}