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
 * p_particle.c: Particle Generator Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_audio.h"
#include "de_misc.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)		( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )
#define DOT2F(a,b)			( FIX2FLT(a[VX])*FIX2FLT(b[VX]) + FIX2FLT(a[VY])*FIX2FLT(b[VY]) )
#define VECMUL(a,scalar)	( a[VX]=FixedMul(a[VX],scalar), a[VY]=FixedMul(a[VY],scalar) )
#define VECADD(a,b)			( a[VX]+=b[VX], a[VY]+=b[VY] )
#define VECMULADD(a,scal,b)	( a[VX]+=FixedMul(scal, b[VX]), a[VY]+=FixedMul(scal,b[VY]) )
#define VECSUB(a,b)			( a[VX]-=b[VX], a[VY]-=b[VY] )
#define VECCPY(a,b)			( a[VX]=b[VX], a[VY]=b[VY] )

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void P_Uncertain(fixed_t *pos, fixed_t low, fixed_t high);
void P_PtcGenThinker(ptcgen_t *gen);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t		mapgravity;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int					r_use_particles = true;
int					r_max_particles = 0;		// Unlimited.
float				r_particle_spawn_rate = 1;	// Unmodified.

ptcgen_t			*active_ptcgens[MAX_ACTIVE_PTCGENS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int			mbox[4];
static int			tmpz, tmprad, tmcross, tmpx1, tmpx2, tmpy1, tmpy2;
static line_t		*ptc_hitline;

// CODE --------------------------------------------------------------------

//===========================================================================
// P_FreePtcGen
//===========================================================================
void P_FreePtcGen(ptcgen_t *gen)
{
	int i;

	for(i = 0; i < MAX_ACTIVE_PTCGENS; i++)
		if(active_ptcgens[i] == gen)
		{
			active_ptcgens[i] = NULL;
			// Destroy it.
			Z_Free(gen->ptcs);
			gen->ptcs = NULL;
			P_RemoveThinker(&gen->thinker);
			break;
		}
}

//===========================================================================
// P_NewPtcGen
//	Allocates a new active ptcgen and adds it to the list of active ptcgens.
//===========================================================================
ptcgen_t *P_NewPtcGen(void)
{
	ptcgen_t *gen = Z_Malloc(sizeof(ptcgen_t), PU_LEVEL, 0);
	int i, oldest = -1, maxage = 0;
	boolean ok = false;

	// Find a suitable spot in the active ptcgens list.
	for(i = 0; i < MAX_ACTIVE_PTCGENS; i++)
	{
		if(!active_ptcgens[i])
		{
			// Put it here.
			active_ptcgens[i] = gen;
			ok = true;
			break;
		}
		else if(!(active_ptcgens[i]->flags & PGF_STATIC)
			&& (oldest < 0 || active_ptcgens[i]->age > maxage))
		{
			oldest = i;
			maxage = active_ptcgens[i]->age;
		}
	}
	if(!ok) 
	{
		if(oldest < 0) 
		{
			Z_Free(gen);
			return 0; // Creation failed!
		} 
		// Replace the oldest generator.
		P_FreePtcGen(active_ptcgens[oldest]);
		active_ptcgens[oldest] = gen;
	}
	memset(gen, 0, sizeof(*gen));
	// Link the thinker to the list of thinkers.
	gen->thinker.function = P_PtcGenThinker;
	P_AddThinker(&gen->thinker);
	return gen;
}

//===========================================================================
// P_InitParticleGen
//	Set gen->count prior to calling this function.
//===========================================================================
void P_InitParticleGen(ptcgen_t *gen, ded_ptcgen_t *def)
{
	int i;
	
	if(gen->count <= 0) gen->count = 1;

	// Make sure no generator is type-triggered by default.
	gen->type = gen->type2 = -1; 

	gen->def = def;
	gen->flags = def->flags; 
	gen->ptcs = Z_Malloc(sizeof(particle_t) * gen->count, PU_LEVEL, 0);

	for(i = 0; i < MAX_PTC_STAGES; i++)
	{
		gen->stages[i].bounce = FRACUNIT * def->stages[i].bounce;
		gen->stages[i].resistance = FRACUNIT * (1 - def->stages[i].resistance);
		gen->stages[i].radius = FRACUNIT * def->stages[i].radius;
		gen->stages[i].gravity = FRACUNIT * def->stages[i].gravity;
		gen->stages[i].type = def->stages[i].type;
		gen->stages[i].flags = def->stages[i].flags; 
	}

	// Init some data.
	for(i = 0; i < 3; i++)
	{
		gen->center[i] = FRACUNIT * def->center[i];
		gen->vector[i] = FRACUNIT * def->vector[i];
	}

	// Apply a random component to the spawn vector.
	if(def->init_vec_variance > 0)
	{
		P_Uncertain(gen->vector, 0, def->init_vec_variance * FRACUNIT);
	}

	// Mark everything unused.
	memset(gen->ptcs, -1, sizeof(particle_t) * gen->count);

	for(i = 0; i < gen->count; i++) 
	{
		// Clear the contact pointers.
		gen->ptcs[i].contact = NULL;
	}
}

//===========================================================================
// P_PresimParticleGen
//===========================================================================
void P_PresimParticleGen(ptcgen_t *gen, int tics)
{
	for(; tics > 0; tics--) P_PtcGenThinker(gen);

	// Reset age so presim doesn't affect it.
	gen->age = 0;
}

//===========================================================================
// P_SpawnParticleGen
//	Creates a new mobj-triggered particle generator based on the given 
//	definition. The generator is added to the list of active ptcgens.
//===========================================================================
void P_SpawnParticleGen(ded_ptcgen_t *def, mobj_t *source)
{
	ptcgen_t *gen;

	if(isDedicated 
		|| !r_use_particles 
		|| !(gen = P_NewPtcGen())) return;

/*#if _DEBUG
	ST_Message("SpawnPtcGen: %s/%i (src:%s typ:%s mo:%p)\n", 
		def->state, def - defs.ptcgens, 
		defs.states[source->state - states].id,
		defs.mobjs[source->type].id, source);
#endif*/

	// Initialize the particle generator.
	gen->count = def->particles;
	P_InitParticleGen(gen, def);
	gen->source = source;
	gen->srcid = source->thinker.id;

	// Is there a need to pre-simulate?
	P_PresimParticleGen(gen, def->presim);
}

//===========================================================================
// P_SpawnPlaneParticleGen
//	Creates a new flat-triggered particle generator based on the given 
//	definition. The generator is added to the list of active ptcgens.
//===========================================================================
void P_SpawnPlaneParticleGen(ded_ptcgen_t *def, sector_t *sec,
								   boolean is_ceiling)
{
	ptcgen_t *gen;
	float *box, width, height;

	if(isDedicated 
		|| !r_use_particles 
		|| !(gen = P_NewPtcGen())) return;

	// Size of source sector might determine count.
	if(def->flags & PGF_PARTS_PER_128)
	{
		// This is rather a rough estimate of sector area.
		box = secinfo[GET_SECTOR_IDX(sec)].bounds;
		width = (box[BRIGHT] - box[BLEFT]) / 128;
		height = (box[BBOTTOM] - box[BTOP]) / 128;
		gen->area = width * height;
		gen->count = def->particles * gen->area;
	}
	else
		gen->count = def->particles;

	// Initialize the particle generator.
	P_InitParticleGen(gen, def);
	gen->sector = sec;
	gen->ceiling = is_ceiling;

	// Is there a need to pre-simulate?
	P_PresimParticleGen(gen, def->presim);
}

//===========================================================================
// P_Uncertain
//	The offset is spherical and random.
//	Low and High should be positive.
//===========================================================================
void P_Uncertain(fixed_t *pos, fixed_t low, fixed_t high)
{
	fixed_t off;
	angle_t theta, phi;
	fixed_t vec[3];
	int i;
	
	if(!low)
	{
		// The simple, cubic algorithm.
		for(i = 0; i < 3; i++) 
			pos[i] += (high * (M_Random() - M_Random())) / 255;
	}
	else
	{
		// The more complicated, spherical algorithm.
		off = ((high - low) * (M_Random() - M_Random())) / 255;
		off += off < 0? -low : low;
	
		theta = M_Random() << (24 - ANGLETOFINESHIFT);
		phi = acos(2*(M_Random()/255.0f) - 1)/PI * (ANGLE_180 >> ANGLETOFINESHIFT);

		vec[VZ] = FixedMul(finecosine[phi], FRACUNIT * 0.8333);
		vec[VX] = FixedMul(finecosine[theta], finesine[phi]);
		vec[VY] = FixedMul(finesine[theta], finesine[phi]);
		for(i = 0; i < 3; i++) pos[i] += FixedMul(vec[i], off);
	}
}

//===========================================================================
// P_SetParticleAngles
//===========================================================================
void P_SetParticleAngles(particle_t *pt, int flags)
{
	if(flags & PTCF_ZERO_YAW) pt->yaw = 0;
	if(flags & PTCF_ZERO_PITCH) pt->pitch = 0;
	if(flags & PTCF_RANDOM_YAW) pt->yaw = M_FRandom() * 65536;
	if(flags & PTCF_RANDOM_PITCH) pt->pitch = M_FRandom() * 65536;
}

//===========================================================================
// P_ParticleSound
//===========================================================================
void P_ParticleSound(fixed_t pos[3], ded_embsound_t *sound)
{
	int i;
	float orig[3];
	
	// Is there any sound to play?
	if(!sound->id || sound->volume <= 0) return;
	
	for(i = 0; i < 3; i++) orig[i] = FIX2FLT(pos[i]);
	S_LocalSoundAtVolumeFrom(sound->id, NULL, orig, sound->volume);
}

//===========================================================================
// P_NewParticle
//	Spawns a new particle.
//===========================================================================
void P_NewParticle(ptcgen_t *gen)
{
	ded_ptcgen_t *def = gen->def;
	particle_t *pt;
	int uncertain, len, i;
	angle_t ang, ang2;
	subsector_t *subsec;
	float *box, inter = -1;
	modeldef_t *mf = 0, *nextmf = 0;

	// Check for a model-only generators.
	if(gen->source)
	{
		inter = R_CheckModelFor(gen->source, &mf, &nextmf);
		if(((!mf || !useModels) && def->flags & PGF_MODEL_ONLY) ||
		   (mf && useModels && mf->flags & MFF_NO_PARTICLES)) return;
	}

	// Keep the spawn cursor in the valid range.
	if(++gen->spawncp >= gen->count) gen->spawncp -= gen->count;

	// Set the particle's data.
	pt = gen->ptcs + gen->spawncp;
	pt->stage = 0;
	if(M_FRandom() < def->alt_variance) pt->stage = def->alt_start;
	pt->tics = def->stages[pt->stage].tics 
		* (1 - def->stages[pt->stage].variance * M_FRandom());

	// Launch vector.
	pt->mov[VX] = gen->vector[VX] + FRACUNIT * (def->vec_variance 
		* (M_FRandom()-M_FRandom()) );
	pt->mov[VY] = gen->vector[VY] + FRACUNIT * (def->vec_variance
		* (M_FRandom()-M_FRandom()) );
	pt->mov[VZ] = gen->vector[VZ] + FRACUNIT * (def->vec_variance
		* (M_FRandom()-M_FRandom()) );

	// Apply some aspect ratio scaling to the momentum vector.
	// This counters the 200/240 difference nearly completely.
	pt->mov[VX] = FixedMul(pt->mov[VX], FRACUNIT * 1.1);
	pt->mov[VZ] = FixedMul(pt->mov[VZ], FRACUNIT * 1.1);
	pt->mov[VY] = FixedMul(pt->mov[VY], FRACUNIT * 0.95);

	// Set proper speed.
	uncertain = def->speed * (1 - def->spd_variance * M_FRandom()) * FRACUNIT;
	len = P_ApproxDistance(P_ApproxDistance(pt->mov[VX], pt->mov[VY]), 
		pt->mov[VZ]);
	if(!len) len = FRACUNIT;
	len = FixedDiv(uncertain, len);
	for(i = 0; i < 3; i++) pt->mov[i] = FixedMul(pt->mov[i], len);

	// The source is a mobj?
	if(gen->source) 
	{
		if(gen->flags & PGF_RELATIVE_VECTOR)
		{
			// Rotate the vector using the source angle.
			float temp[3] = { FIX2FLT(pt->mov[VX]), FIX2FLT(pt->mov[VY]), 0 };
			// Player visangles have some problems, let's not use them.
			M_RotateVector(temp, gen->source->angle 
				/ (float)ANG180 * -180 + 90, 0);
			pt->mov[VX] = temp[VX] * FRACUNIT;
			pt->mov[VY] = temp[VY] * FRACUNIT;
		}
		if(gen->flags & PGF_RELATIVE_VELOCITY)
		{
			pt->mov[VX] += gen->source->momx;
			pt->mov[VY] += gen->source->momy;
			pt->mov[VZ] += gen->source->momz;
		}

		// Position.
		pt->pos[VX] = gen->source->x;
		pt->pos[VY] = gen->source->y;
		pt->pos[VZ] = gen->source->z - gen->source->floorclip;
		P_Uncertain(pt->pos, FRACUNIT * def->min_spawn_radius, 
			FRACUNIT * def->spawn_radius);
		
		// Offset to the real center.
		pt->pos[VZ] += gen->center[VZ];

		// Calculate XY center with mobj angle.
		ang = (r_use_srvo_angle? (gen->source->visangle << 16)
			: gen->source->angle)
			+ FIX2FLT(gen->center[VY]) / 180.0f * ANG180;
		ang2 = (ang + ANG90) >> ANGLETOFINESHIFT;
		ang >>= ANGLETOFINESHIFT;
		pt->pos[VX] += FixedMul(finecosine[ang], gen->center[VX]);
		pt->pos[VY] += FixedMul(finesine[ang], gen->center[VX]);

		// There might be an offset from the model of the mobj.
		if(mf && (mf->sub[0].flags & MFF_PARTICLE_SUB1 || def->submodel >= 0))
		{
			int subidx = 1; // Default to submodel #1.
			float off[3];

			// Select the right submodel to use as the origin.
			if(def->submodel >= 0)
			{
				subidx = def->submodel;
			}
			memcpy(off, mf->ptcoffset[subidx], sizeof(off));
			
			// Interpolate the offset.
			if(inter > 0 && nextmf)
				for(i = 0; i < 3; i++)
				{
					off[i] += (nextmf->ptcoffset[subidx][i] 
						- mf->ptcoffset[subidx][i]) * inter;
				}
			
			// Apply it to the particle coords.
			pt->pos[VX] += FixedMul(finecosine[ang], FRACUNIT * off[VX]);
			pt->pos[VX] += FixedMul(finecosine[ang2], FRACUNIT * off[VZ]);
			pt->pos[VY] += FixedMul(finesine[ang], FRACUNIT * off[VX]);
			pt->pos[VY] += FixedMul(finesine[ang2], FRACUNIT * off[VZ]);
			pt->pos[VZ] += FRACUNIT * off[VY];
		}
	}
	else if(gen->sector) // The source is a plane?
	{
		i = gen->stages[pt->stage].radius;

		// Choose a random spot inside the sector, on the spawn plane.
		if(gen->flags & PGF_SPACE_SPAWN)
		{
			pt->pos[VZ] = gen->sector->floorheight + i
				+ FixedMul(M_Random() << 8, gen->sector->ceilingheight
				- gen->sector->floorheight - 2*i);
		}
		else if(gen->flags & PGF_FLOOR_SPAWN ||
				(!(gen->flags & (PGF_FLOOR_SPAWN | PGF_CEILING_SPAWN)) 
				 && !gen->ceiling))
		{
			// Spawn on the floor.
			pt->pos[VZ] = gen->sector->floorheight + i;
		}
		else 
		{
			// Spawn on the ceiling.
			pt->pos[VZ] = gen->sector->ceilingheight - i;
		}

		// Choosing the XY spot is a bit more difficult.
		// But we must be fast and only sufficiently accurate.
		
		// FIXME: Nothing prevents spawning on the wrong side (or inside)
		// of one-sided walls (large diagonal subsectors!).

		box = secinfo[GET_SECTOR_IDX(gen->sector)].bounds;
		for(i = 0; i < 5; i++)	// Try a couple of times (max).
		{
			subsec = R_PointInSubsector(FRACUNIT * (box[BLEFT] 
				+ M_FRandom()*(box[BRIGHT] - box[BLEFT])), FRACUNIT 
				* (box[BTOP] + M_FRandom()*(box[BBOTTOM] - box[BTOP])));
			if(subsec->sector == gen->sector) 
				break;
			else
				subsec = NULL;
		}
		if(!subsec) goto spawn_failed;
		
		// Try a couple of times to get a good random spot.
		for(i = 0; i < 10; i++) // Max this many tries before giving up.
		{
			pt->pos[VX] = FRACUNIT * (subsec->bbox[0].x + M_FRandom() 
				* (subsec->bbox[1].x - subsec->bbox[0].x));
			pt->pos[VY] = FRACUNIT * (subsec->bbox[0].y + M_FRandom()
				* (subsec->bbox[1].y - subsec->bbox[0].y));
			if(R_PointInSubsector(pt->pos[VX], pt->pos[VY])	== subsec) 
				break; // This is a good place.
		}
		if(i == 10) // No good place found?
		{
spawn_failed:
			pt->stage = -1;	// Damn.
			return;			
		}
	}
	else if(gen->flags & PGF_UNTRIGGERED)
	{
		// The center position is the spawn origin.
		pt->pos[VX] = gen->center[VX];
		pt->pos[VY] = gen->center[VY];
		pt->pos[VZ] = gen->center[VZ];
		P_Uncertain(pt->pos, FRACUNIT * def->min_spawn_radius,
			FRACUNIT * def->spawn_radius);
	}

	// Initial angles for the particle.
	P_SetParticleAngles(pt, def->stages[pt->stage].flags);

	// The other place where this gets updated is after moving over 
	// a two-sided line.
	pt->sector = gen->sector? gen->sector 
		: R_PointInSubsector(pt->pos[VX], pt->pos[VY])->sector;

	// Play a stage sound?
	P_ParticleSound(pt->pos, &def->stages[pt->stage].sound);
}

//===========================================================================
// PIT_ClientMobjParticles
//	Callback for the client mobj iterator, called from P_ManyNewParticles.
//===========================================================================
boolean PIT_ClientMobjParticles(clmobj_t *cmo, void *parm)
{
	ptcgen_t *gen = parm;

	// If the clmobj is not valid at the moment, don't do anything.
	if(cmo->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN))
	{
		return true;
	}

	if(cmo->mo.type != gen->type && cmo->mo.type != gen->type2) 
	{
		// Type mismatch.
		return true;
	}
	gen->source = &cmo->mo;
	P_NewParticle(gen);
	return true;
}

//===========================================================================
// P_ManyNewParticles
//	Spawn multiple new particles using all applicable sources.
//===========================================================================
void P_ManyNewParticles(ptcgen_t *gen)
{
	thinker_t	*it;
	mobj_t		*mo;

	// Client's should also check the client mobjs.
	if(isClient)
	{
		Cl_MobjIterator(PIT_ClientMobjParticles, gen);
	}

	// Scan all thinkers. 
	for(it = thinkercap.next; it != &thinkercap; it = it->next)
	{
		if(!P_IsMobjThinker(it->function)) continue;
		mo = (mobj_t*) it;				
		// Type match?
		if(mo->type != gen->type && mo->type != gen->type2) 
			continue; 
		// Someone might think this is a slight hack...
		gen->source = mo;
		P_NewParticle(gen);
	}
	// The generator has no real source.
	gen->source = NULL;
}

//===========================================================================
// PIT_CheckLinePtc
//===========================================================================
boolean PIT_CheckLinePtc(line_t *ld, void *data)
{
	fixed_t bbox[4];
	fixed_t ceil, floor;
	sector_t *front, *back;

	// Setup the bounding box for the line.
	ORDER(ld->v1->x, ld->v2->x, bbox[BOXLEFT], bbox[BOXRIGHT]);
	ORDER(ld->v1->y, ld->v2->y, bbox[BOXBOTTOM], bbox[BOXTOP]);

    if (mbox[BOXRIGHT] <= bbox[BOXLEFT]
		|| mbox[BOXLEFT] >= bbox[BOXRIGHT]
		|| mbox[BOXTOP] <= bbox[BOXBOTTOM]
		|| mbox[BOXBOTTOM] >= bbox[BOXTOP])
	{
		return true; // Bounding box misses the line completely.
	}

	// Movement must cross the line.
	if(P_PointOnLineSide(tmpx1, tmpy1, ld) 
		== P_PointOnLineSide(tmpx2, tmpy2, ld)) return true;

	// We are possibly hitting something here. 
	ptc_hitline = ld;	
	if(!ld->backsector) return false; // Boing!

	// Determine the opening we have here.
	front = ld->frontsector;
	back = ld->backsector;
	ceil = MIN_OF(front->ceilingheight, back->ceilingheight);
	floor = MAX_OF(front->floorheight, back->floorheight);

	// There is a backsector. We possibly might hit something.
	if(tmpz - tmprad < floor || tmpz + tmprad > ceil)
		return false; // Boing!

	// There is a possibility that the new position is in a new sector.
	tmcross = true; // Afterwards, update the sector pointer.

	// False alarm, continue checking.
	return true;
}

//===========================================================================
// P_TouchParticle
//	Particle touches something solid. Returns false iff the particle dies.
//===========================================================================
int P_TouchParticle
	(particle_t *pt, ptcstage_t *stage, ded_ptcstage_t *stageDef, 
	 boolean touchWall)
{
	// Play a hit sound.
	P_ParticleSound(pt->pos, &stageDef->hit_sound);

	if(stage->flags & PTCF_DIE_TOUCH) 
	{
		// Particle dies from touch.
		pt->stage = -1;
		return false;
	}
	if(stage->flags & PTCF_STAGE_TOUCH ||
	   (touchWall && stage->flags & PTCF_STAGE_WALL_TOUCH) ||
	   (!touchWall && stage->flags & PTCF_STAGE_FLAT_TOUCH))
	{
		// Particle advances to the next stage.
		pt->tics = 0;
	}
	// Particle survives the touch.
	return true;
}

//===========================================================================
// P_FixedCrossProduct
//	Semi-fixed, actually.
//===========================================================================
void P_FixedCrossProduct(float *fa, fixed_t *b, fixed_t *result)
{
	result[VX] = FixedMul(FRACUNIT * fa[VY], b[VZ]) - FixedMul(FRACUNIT * fa[VZ], b[VY]);
	result[VY] = FixedMul(FRACUNIT * fa[VZ], b[VX]) - FixedMul(FRACUNIT * fa[VX], b[VZ]);
	result[VZ] = FixedMul(FRACUNIT * fa[VX], b[VY]) - FixedMul(FRACUNIT * fa[VY], b[VX]);
}

#if 0
//===========================================================================
// P_FixedDotProduct
//===========================================================================
fixed_t P_FixedDotProduct(fixed_t *a, fixed_t *b)
{
	return FixedMul(a[VX], b[VX]) + FixedMul(a[VY], b[VY])
		+ FixedMul(a[VZ], b[VZ]);
}
#endif

//===========================================================================
// P_GetParticleRadius
//	Takes care of consistent variance.
//	Currently only used visually, collisions use the constant radius.
//	The variance can be negative (results will be larger).
//===========================================================================
float P_GetParticleRadius(ded_ptcstage_t *stage_def, int ptc_index)
{
	float rnd[16] = { .875f, .125f, .3125f, .75f, .5f, .375f, 
		.5625f, .0625f, 1, .6875f, .625f, .4375f, .8125f, .1875f, 
		.9375f, .25f };

	if(!stage_def->radius_variance) return stage_def->radius;
	return (rnd[ptc_index & 0xf] * stage_def->radius_variance
		+ (1 - stage_def->radius_variance)) * stage_def->radius;
}

//===========================================================================
// P_GetParticleZ
//	A particle may be attached to the floor or ceiling of the sector.
//===========================================================================
fixed_t P_GetParticleZ(particle_t *pt)
{
	if(pt->pos[VZ] == DDMAXINT)
		return pt->sector->ceilingheight - 2*FRACUNIT;
	if(pt->pos[VZ] == DDMININT)
		return pt->sector->floorheight + 2*FRACUNIT;
	return pt->pos[VZ];
}

//===========================================================================
// P_SpinParticle
//===========================================================================
void P_SpinParticle(ptcgen_t *gen, particle_t *pt)
{
	ded_ptcstage_t *stDef = gen->def->stages + pt->stage;	
	uint index = pt - gen->ptcs + (int)gen/8;
	static int yawSigns[4] = { 1, 1, -1, -1 };
	static int pitchSigns[4] = { 1, -1, 1, -1 };
	int yawSign, pitchSign;

	yawSign = yawSigns[index % 4];
	pitchSign = pitchSigns[index % 4];

	if(stDef->spin[0] != 0)
	{
		pt->yaw += 65536 * yawSign * stDef->spin[0] / (360 * TICSPERSEC);
	}
	if(stDef->spin[1] != 0)
	{
		pt->pitch += 65536 * pitchSign * stDef->spin[1] / (360 * TICSPERSEC);
	}
}

//===========================================================================
// P_MoveParticle
//	The movement is done in two steps: 
//	Z movement is done first. Skyflat kills the particle.
//	XY movement checks for hits with solid walls (no backsector).
//	This is supposed to be fast and simple (but not too simple).
//===========================================================================
void P_MoveParticle(ptcgen_t *gen, particle_t *pt)
{
	int x, y, z, xl, xh, yl, yh, bx, by;
	ptcstage_t *st = gen->stages + pt->stage;
	ded_ptcstage_t *stDef = gen->def->stages + pt->stage;
	boolean zBounce = false;
	boolean hitFloor;
	fixed_t hardRadius = st->radius/2;

	// Particle rotates according to spin speed.
	P_SpinParticle(gen, pt);

	// Changes to momentum.
	pt->mov[VZ] -= FixedMul(mapgravity, st->gravity);

	// Vector force.
	if(stDef->vector_force[VX] != 0
		|| stDef->vector_force[VY] != 0
		|| stDef->vector_force[VZ] != 0)
	{
		int i;
		for(i = 0; i < 3; i++)
			pt->mov[i] += FRACUNIT * stDef->vector_force[i];
	}

	// Sphere force pull and turn.
	// Only applicable to sourced or untriggered generators. For other
	// types it's difficult to define the center coordinates.
	if(st->flags & PTCF_SPHERE_FORCE
		&& (gen->source || gen->flags & PGF_UNTRIGGERED))
	{
		fixed_t delta[3], dist;
		fixed_t cross[3];
		if(gen->source)
		{
			delta[VX] = pt->pos[VX] - gen->source->x;
			delta[VY] = pt->pos[VY] - gen->source->y;
			delta[VZ] = P_GetParticleZ(pt) 
				- (gen->source->z + gen->center[VZ]);
		}
		else
		{
			for(x = 0; x < 3; x++) delta[x] = pt->pos[x] - gen->center[x];
		}

		// Apply the offset (to source coords).
		for(x = 0; x < 3; x++) 
			delta[x] -= FRACUNIT * gen->def->force_origin[x];

		// Counter the aspect ratio of old times.
		delta[VZ] = FixedMul(delta[VZ], 1.2 * FRACUNIT);

		dist = P_ApproxDistance(P_ApproxDistance(delta[VX], delta[VY]), 
			delta[VZ]);
		if(dist)
		{
			// Radial force pushes the particles on the surface of a sphere.
			if(gen->def->force)
			{
				// Normalize delta vector, multiply with (dist - forceRadius),
				// multiply with radial force strength.
				for(x = 0; x < 3; x++)
					pt->mov[x] -= FixedMul( FixedMul(FixedDiv(delta[x], dist),
						dist - FRACUNIT * gen->def->force_radius ), 
						FRACUNIT * gen->def->force);
			}

			// Rotate!
			if(gen->def->force_axis[VX]
				|| gen->def->force_axis[VY]
				|| gen->def->force_axis[VZ])
			{
				P_FixedCrossProduct(gen->def->force_axis, delta, cross);
				for(x = 0; x < 3; x++) pt->mov[x] += cross[x] >> 8;
			}
		}
	}

	if(st->resistance != FRACUNIT)
	{
		for(x = 0; x < 3; x++)
			pt->mov[x] = FixedMul(pt->mov[x], st->resistance);
	}

	// The particle is 'soft': half of radius is ignored.
	// The exception is plane flat particles, which are rendered flat 
	// against planes. They are almost entirely soft when it comes to plane
	// collisions.
	if(st->flags & PTCF_PLANE_FLAT) hardRadius = FRACUNIT;

	// Check the new Z position only if not stuck to a plane.
	if(pt->pos[VZ] != DDMININT && pt->pos[VZ] != DDMAXINT)
	{
		z = pt->pos[VZ] + pt->mov[VZ];
		if(z > pt->sector->ceilingheight - hardRadius)
		{
			// The Z is through the roof! 
			if(pt->sector->ceilingpic == skyflatnum) 
			{
				// Special case: particle gets lost in the sky.
				pt->stage = -1;
				return;
			}
			if(!P_TouchParticle(pt, st, stDef, false)) return;
			z = pt->sector->ceilingheight - hardRadius;
			zBounce = true;
			hitFloor = false;
		}
		// Also check the floor.
		if(z < pt->sector->floorheight + hardRadius)
		{
			if(pt->sector->floorpic == skyflatnum) 
			{
				pt->stage = -1;
				return;
			}
			if(!P_TouchParticle(pt, st, stDef, false)) return;
			z = pt->sector->floorheight + hardRadius;
			zBounce = true;
			hitFloor = true;
		}
		if(zBounce) 
		{
			pt->mov[VZ] = FixedMul(-pt->mov[VZ], st->bounce);
			if(!pt->mov[VZ])
			{
				// The particle has stopped moving. This means its Z-movement 
				// has ceased because of the collision with a plane. Plane-flat
				// particles will stick to the plane.
				if(st->flags & PTCF_PLANE_FLAT)
				{
					z = hitFloor? DDMININT : DDMAXINT;
				}
			}
		}
		
		// Move to the new Z coordinate.
		pt->pos[VZ] = z;
	}

	// Now check the XY direction. 
	// - Check if the movement crosses any solid lines.
	// - If it does, quit when first one contacted and apply appropriate
	//   bounce (result depends on the angle of the contacted wall).
	x = pt->pos[VX] + pt->mov[VX];
	y = pt->pos[VY] + pt->mov[VY];

	tmcross = false; // Has crossed potential sector boundary?

	// XY movement can be skipped if the particle is not moving on the
	// XY plane.
	if(!pt->mov[VX] && !pt->mov[VY]) 
	{
		// If the particle is contacting a line, there is a chance that the
		// particle should be killed (if it's moving slowly at max).
		if(pt->contact)
		{
			sector_t *front = pt->contact->frontsector;
			sector_t *back = pt->contact->backsector;
			if(front && back && abs(pt->mov[VZ]) < FRACUNIT/2)
			{
				// If the particle is in the opening of a 2-sided line, it's
				// quite likely that it shouldn't be here...
				if(P_GetParticleZ(pt) > MAX_OF(front->floorheight, 
					back->floorheight) 
					&& P_GetParticleZ(pt) < MIN_OF(front->ceilingheight, 
					back->ceilingheight))
				{
					// Kill the particle.
					pt->stage = -1;
					return;
				}
			}
		}
		// Still not moving on the XY plane...
		goto quit_iteration;
	}

	// We're moving in XY, so if we don't hit anything there can't be any 
	// line contact.
	pt->contact = NULL;

	// Bounding box of the movement line.
	tmpz = z;
	tmprad = hardRadius;
	tmpx1 = pt->pos[VX];
	tmpx2 = x;
	tmpy1 = pt->pos[VY];
	tmpy2 = y;
	mbox[BOXTOP] = MAX_OF(y, pt->pos[VY]) + st->radius;
	mbox[BOXBOTTOM] = MIN_OF(y, pt->pos[VY]) - st->radius;
	mbox[BOXRIGHT] = MAX_OF(x, pt->pos[VX]) + st->radius;
	mbox[BOXLEFT] = MIN_OF(x, pt->pos[VX]) - st->radius;

	// Iterate the lines in the contacted blocks.	
	xl = (mbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
	xh = (mbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
	yl = (mbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
	yh = (mbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

	validcount++;
	for(bx = xl; bx <= xh; bx++)
		for(by = yl; by <= yh; by++)
			if(!P_BlockLinesIterator(bx, by, PIT_CheckLinePtc, 0))
			{
				int normal[2], dotp;
				
				// Must survive the touch.
				if(!P_TouchParticle(pt, st, stDef, true)) return;

				// There was a hit! Calculate bounce vector.
				// - Project movement vector on the normal of hitline.
				// - Calculate the difference to the point on the normal.
				// - Add the difference to movement vector, negate movement.
				// - Multiply with bounce.

				// Calculate the normal. 
				normal[VX] = -ptc_hitline->dx;
				normal[VY] = -ptc_hitline->dy;

				if(!normal[VX] && !normal[VY]) goto quit_iteration;

				// Calculate as floating point so we don't overflow.
				dotp = FRACUNIT * (DOT2F(pt->mov, normal) 
					/ DOT2F(normal, normal));
				VECMUL(normal, dotp);
				VECSUB(normal, pt->mov);
				VECMULADD(pt->mov, 2*FRACUNIT, normal);
				VECMUL(pt->mov, st->bounce);

				// Continue from the old position.
				x = pt->pos[VX];
				y = pt->pos[VY];
				tmcross = false; // Sector can't change if XY doesn't.

				// This line is the latest contacted line.
				pt->contact = ptc_hitline;
				goto quit_iteration;
			}

quit_iteration:
	// The move is now OK.
	pt->pos[VX] = x;
	pt->pos[VY] = y;
	
	// Should we update the sector pointer?
	if(tmcross) pt->sector = R_PointInSubsector(x, y)->sector;
}

//===========================================================================
// P_PtcGenThinker
//	Spawn and move particles.
//===========================================================================
void P_PtcGenThinker(ptcgen_t *gen)
{
	ded_ptcgen_t *def = gen->def;
	particle_t *pt;
	float newparts;
	int i;

	// Source has been destroyed?
	if(!(gen->flags & PGF_UNTRIGGERED) 
		&& !P_IsUsedMobjID(gen->srcid))
	{
		// Blasted... Spawning new particles becomes impossible.
		gen->source = NULL;
	}

	// Time to die?
	if(++gen->age > def->max_age && def->max_age >= 0) 
	{
		P_FreePtcGen(gen);
		return;
	}

	// Spawn new particles?
	if((gen->age <= def->spawn_age || def->spawn_age < 0)
		&& (gen->source || gen->sector || gen->type >= 0 
			|| gen->flags & PGF_UNTRIGGERED))
	{
		if(gen->flags & PGF_PARTS_PER_128
			|| gen->flags & PGF_SCALED_RATE)
		{
			// Density spawning.
			newparts = def->spawn_rate * gen->area;
		}
		else
		{
			// Normal spawning.
			newparts = def->spawn_rate; 
		}
		newparts *= r_particle_spawn_rate 
			* (1 - def->spawn_variance * M_FRandom());
		gen->spawncount += newparts;
		while(gen->spawncount >= 1)
		{
			// Spawn a new particle.
			if(gen->type >= 0) // Type-triggered?
				P_ManyNewParticles(gen);
			else
				P_NewParticle(gen);
			gen->spawncount--;
		}
	}

	// Move particles.
	for(i = 0, pt = gen->ptcs; i < gen->count; i++, pt++)
	{
		if(pt->stage < 0) continue; // Not in use.
		if(pt->tics-- <= 0)
		{
			// Advance to next stage.
			if(++pt->stage == MAX_PTC_STAGES
				|| gen->stages[pt->stage].type == PTC_NONE)
			{
				// Kill the particle.
				pt->stage = -1;
				continue;
			}
			pt->tics = def->stages[pt->stage].tics
				* (1 - def->stages[pt->stage].variance * M_FRandom());

			// Change in particle angles?
			P_SetParticleAngles(pt, def->stages[pt->stage].flags);

			// A sound?
			P_ParticleSound(pt->pos, &def->stages[pt->stage].sound);
		}
		// Try to move.
		P_MoveParticle(gen, pt);
	}
}

//===========================================================================
// P_GetPtcGenForFlat
//	Returns the ptcgen definition for the given flat.
//===========================================================================
ded_ptcgen_t *P_GetPtcGenForFlat(int flatpic)
{
	int	i;
	ded_ptcgen_t *def;

	for(i = 0, def = defs.ptcgens; i < defs.count.ptcgens.num; i++, def++)
	{
		if(def->flags & PGF_GROUP)
		{
			// This generator is triggered by all the flats in 
			// the animation group.
			flat_t *defFlat = R_GetFlat(def->flat_num);
			flat_t *usedFlat = R_GetFlat(flatpic);

			// We only need to search if we know both the real used flat
			// and the flat of this definition belong in an animgroup.
			if(defFlat->ingroup && usedFlat->ingroup)
			{
				int g;
				for(g = 0; g < numgroups; g++)
				{
					// Precache groups don't apply.
					if(groups[g].flags & AGF_PRECACHE) continue;

					if(R_IsInAnimGroup(groups[g].id, DD_FLAT, def->flat_num)
						&& R_IsInAnimGroup(groups[g].id, DD_FLAT, flatpic))
					{
						// Both are in this group! This def will do.
						return def;
					}
				}
			}
		}
		if(def->flat_num == flatpic) return def;
	}
	return NULL;
}

//===========================================================================
// P_HasActivePtcGen
//	Returns true iff there is an active ptcgen for the given plane.
//===========================================================================
boolean P_HasActivePtcGen(sector_t *sector, int is_ceiling)
{
	int i;

	for(i = 0; i < MAX_ACTIVE_PTCGENS; i++)
		if(active_ptcgens[i] 
			&& active_ptcgens[i]->sector == sector
			&& active_ptcgens[i]->ceiling == is_ceiling) return true;
	return false;
}

//===========================================================================
// P_CheckPtcPlanes
//	Spawns new ptcgens for planes, if necessary.
//===========================================================================
void P_CheckPtcPlanes(void)
{
	int	i, p, plane;
	sector_t *sector;
	ded_ptcgen_t *def;

	// There is no need to do this on every tic.
	if(isDedicated || gametic % 4) return; 

	for(i = 0; i < numsectors; i++)
	{
		sector = SECTOR_PTR(i);
		for(p = 0; p < 2; p++)
		{
			plane = p;
			def = P_GetPtcGenForFlat(plane? sector->ceilingpic 
				: sector->floorpic);
			if(!def) continue;
			if(def->flags & PGF_CEILING_SPAWN) plane = 1;
			if(def->flags & PGF_FLOOR_SPAWN) plane = 0;
			if(!P_HasActivePtcGen(sector, plane))
			{
				// Spawn it!
				P_SpawnPlaneParticleGen(def, sector, plane);
			}
		}
	}
}

//===========================================================================
// P_SpawnTypeParticleGens
//	Spawns all type-triggered particle generators, regardless of whether
//	the type of thing exists in the level or not (things might be 
//	dynamically created).
//===========================================================================
void P_SpawnTypeParticleGens(void)
{
	int i;
	ded_ptcgen_t *def;
	ptcgen_t *gen;

	if(isDedicated || !r_use_particles) return;

	for(i = 0, def = defs.ptcgens; i < defs.count.ptcgens.num; i++, def++)
	{
		if(def->type_num < 0) continue;
		if(!(gen = P_NewPtcGen())) return; // No more generators.

		// Initialize the particle generator.
		gen->count = def->particles;
		P_InitParticleGen(gen, def);
		gen->type = def->type_num;
		gen->type2 = def->type2_num;

		// Is there a need to pre-simulate?
		P_PresimParticleGen(gen, def->presim);
	}
}

//===========================================================================
// P_SpawnMapParticleGens
//===========================================================================
void P_SpawnMapParticleGens(char *map_id)
{
	int i;
	ded_ptcgen_t *def;
	ptcgen_t *gen;

	if(isDedicated || !r_use_particles) return;

	for(i = 0, def = defs.ptcgens; i < defs.count.ptcgens.num; i++, def++)
	{
		if(!def->map[0] || stricmp(def->map, map_id)) continue;
		if(!(gen = P_NewPtcGen())) return; // No more generators.

		// Initialize the particle generator.
		gen->count = def->particles;
		P_InitParticleGen(gen, def);
		gen->flags |= PGF_UNTRIGGERED;

		// Is there a need to pre-simulate?
		P_PresimParticleGen(gen, def->presim);
	}
}

//===========================================================================
// P_SpawnDamageParticleGen
//	A public function (games can call this directly).
//===========================================================================
void P_SpawnDamageParticleGen(mobj_t *mo, mobj_t *inflictor, int amount)
{
	ptcgen_t *gen;
	ded_ptcgen_t *def;
	int i;
	fixed_t len;

	// Are particles allowed?
	if(isDedicated || !r_use_particles || !mo || !inflictor || amount <= 0) 
		return;
		
	// Search for a suitable definition.
	for(i = 0, def = defs.ptcgens; i < defs.count.ptcgens.num; i++, def++)
	{
		// It must be for this type of mobj.
		if(def->damage_num != mo->type) continue; 

		// Create it.
		if(!(gen = P_NewPtcGen())) return; // No more generators.
		gen->count = def->particles;
		P_InitParticleGen(gen, def);
		gen->flags |= PGF_UNTRIGGERED;
		gen->area = amount;
		if(gen->area < 1) gen->area = 1;

		// Calculate appropriate center coordinates and the vector.
		gen->center[VX] += mo->x;
		gen->center[VY] += mo->y;
		gen->center[VZ] += mo->z + mo->height/2;
		gen->vector[VX] += mo->x - inflictor->x;
		gen->vector[VY] += mo->y - inflictor->y;
		gen->vector[VZ] += mo->z + mo->height/2 - inflictor->z 
			- inflictor->height/2;
		
		// Normalize the vector.
		len = P_ApproxDistance(P_ApproxDistance(gen->vector[VX], 
			gen->vector[VY]), gen->vector[VZ]);
		if(len) 
		{
			int k;
			for(k = 0; k < 3; k++) 
				gen->vector[k] = FixedDiv(gen->vector[k], len);
		}

		// Is there a need to pre-simulate?
		P_PresimParticleGen(gen, def->presim);
	}
}
