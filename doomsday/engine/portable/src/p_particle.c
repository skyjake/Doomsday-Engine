/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
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

#define ORDER(x,y,a,b)      ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )
#define DOT2F(a,b)          ( FIX2FLT(a[VX])*FIX2FLT(b[VX]) + FIX2FLT(a[VY])*FIX2FLT(b[VY]) )
#define VECMUL(a,scalar)    ( a[VX]=FixedMul(a[VX],scalar), a[VY]=FixedMul(a[VY],scalar) )
#define VECADD(a,b)         ( a[VX]+=b[VX], a[VY]+=b[VY] )
#define VECMULADD(a,scal,b) ( a[VX]+=FixedMul(scal, b[VX]), a[VY]+=FixedMul(scal,b[VY]) )
#define VECSUB(a,b)         ( a[VX]-=b[VX], a[VY]-=b[VY] )
#define VECCPY(a,b)         ( a[VX]=b[VX], a[VY]=b[VY] )

BEGIN_PROF_TIMERS()
  PROF_PTCGEN_LINK
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct pglink_s {
    struct pglink_s* next;
    ptcgen_t*       gen;
} pglink_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void P_PtcGenThinker(ptcgen_t* gen);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_Uncertain(fixed_t* pos, fixed_t low, fixed_t high);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte useParticles = true;
int maxParticles = 0; // Unlimited.
float particleSpawnRate = 1; // Unmodified.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static ptcgen_t* activePtcGens[MAX_ACTIVE_PTCGENS];

static pglink_t** pgLinks = NULL; // Array of pointers to pgLinks in pgStore.
static pglink_t* pgStore;
static unsigned int pgCursor = 0, pgMax;

static vec2_t mbox[2];
static fixed_t tmpz, tmprad, tmpx1, tmpx2, tmpy1, tmpy2;
static boolean tmcross;
static linedef_t* ptcHitLine;

// CODE --------------------------------------------------------------------

static void freePtcGen(ptcgen_t* gen)
{
    Z_Free(gen->ptcs);
    gen->ptcs = NULL;
}

static void unlinkPtcGen(ptcgen_t* gen)
{
    ptcgenid_t          i;

    for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
    {
        if(activePtcGens[i] == gen)
        {
            activePtcGens[i] = NULL;
            break;
        }
    }
}

static void linkPtcGen(ptcgenid_t slot, ptcgen_t* gen)
{
    assert(slot < MAX_ACTIVE_PTCGENS);

    activePtcGens[slot] = gen;
}

static boolean iterateSectorLinkedPtcGens(sector_t* sector,
                                          boolean (*callback) (ptcgen_t*, void*),
                                          void* context)
{
    boolean             result = true;

    if(sector)
    {
        pglink_t*           it = pgLinks[GET_SECTOR_IDX(sector)];

        while(it)
        {
            if((result = callback(it->gen, context)) == 0)
                break;
            it = it->next;
        }
    }

    return result;
}

static uint findSlotForNewGen(void)
{
    ptcgenid_t          i, slot = 0;
    int                 maxage = 0;
    boolean             isEmpty = false;

    // Find a suitable spot in the active ptcgens list.
    i = 0;
    do
    {
        if(!activePtcGens[i])
        {   // An empty slot, put it here.
            slot = i + 1;
            isEmpty = true;
        }
        else if(!(activePtcGens[i]->flags & PGF_STATIC) &&
                (!slot || activePtcGens[i]->age > maxage))
        {
            slot = i + 1;
            maxage = activePtcGens[i]->age;
        }
    } while(!isEmpty && ++i < MAX_ACTIVE_PTCGENS);

    return slot;
}

static ptcgen_t* P_PtcGenCreate(void)
{
    ptcgen_t*           gen = Z_Calloc(sizeof(ptcgen_t), PU_MAP, 0);

    // Link the thinker to the list of (private) thinkers.
    gen->thinker.function = P_PtcGenThinker;
    P_ThinkerAdd(&gen->thinker, false);

    return gen;
}

static void P_PtcGenDestroy(ptcgen_t* gen)
{
    P_ThinkerRemove(&gen->thinker);

    unlinkPtcGen(gen);
    freePtcGen(gen);
}

/**
 * Allocates a new active ptcgen and adds it to the list of active ptcgens.
 *
 * \fixme Linear allocation when in-game is not good...
 */
static ptcgen_t* P_NewPtcGen(void)
{
    ptcgenid_t          slot = findSlotForNewGen();

    // Find a suitable slot in the active ptcgens list.
    if(slot)
    {
        ptcgen_t*           gen;

        // If there is already a generator here, destroy it.
        if(activePtcGens[slot-1])
            P_PtcGenDestroy(activePtcGens[slot-1]);

        // Allocate a new generator.
        gen = P_PtcGenCreate();
        linkPtcGen(slot-1, gen);

        return gen;
    }

    return 0; // Creation failed.
}

/**
 * Called once during startup.
 */
void P_PtcInit(void)
{
    memset(activePtcGens, 0, sizeof(activePtcGens));
}

void P_PtcInitForMap(void)
{
    uint                startTime = Sys_GetRealTime();

    pgLinks = Z_Malloc(sizeof(pglink_t *) * numSectors, PU_MAP, 0);
    memset(pgLinks, 0, sizeof(pglink_t *) * numSectors);

    // We can link 64 generators each into four sectors before
    // running out of pgLinks.
    pgMax = 4 * MAX_ACTIVE_PTCGENS;
    pgStore = Z_Malloc(sizeof(pglink_t) * pgMax, PU_MAP, 0);
    pgCursor = 0;

    memset(activePtcGens, 0, sizeof(activePtcGens));

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    P_SpawnTypeParticleGens();
    P_SpawnMapParticleGens(mapID);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("P_PtcInitForMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * Convert a particle generator id to pointer.
 *
 * @return              Pointer to ptcgen iff found, ELSE @c NULL.
 */
const ptcgen_t* P_IndexToPtcGen(ptcgenid_t id)
{
    if(id >= 0 && id < MAX_ACTIVE_PTCGENS)
        return activePtcGens[id];

    return NULL; // Not found!?
}

/**
 * Convert a particle generator point to id.
 *
 * @return              @c -1 iff NOT found, ELSE id of the specifed ptcgen.
 */
ptcgenid_t P_PtcGenToIndex(const ptcgen_t* gen)
{
    if(gen)
    {
        ptcgenid_t              i;

        for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
        {
            if(activePtcGens[i] == gen)
                return i;
        }
    }

    return -1; // Not found!?
}

/**
 * Returns an unused link from the pgStore.
 */
static pglink_t* PG_GetLink(void)
{
    if(pgCursor < pgMax)
        return &pgStore[pgCursor++];

    VERBOSE(Con_Message("PG_GetLink: Out of PGen store.\n"));
    return NULL;
}

static void PG_LinkPtcGen(ptcgen_t* gen, uint secIDX)
{
    pglink_t*           link, *it;

    // Must check that it isn't already there...
    for(it = pgLinks[secIDX]; it; it = it->next)
        if(it->gen == gen)
            return; // No, no...

    // We need a new PG link.
    link = PG_GetLink();
    if(!link)
        return; // Out of links!

    link->gen = gen;
    link->next = pgLinks[secIDX];
    pgLinks[secIDX] = link;
}

/**
 * Link all active particle generators into the world.
 */
void P_CreatePtcGenLinks(void)
{
    ptcgenid_t          i;
#ifdef DD_PROFILE
    static int p;

    if(++p > 40)
    {
        p = 0;
        PRINT_PROF(PROF_PTCGEN_LINK);
    }
#endif

BEGIN_PROF(PROF_PTCGEN_LINK);

    if(pgLinks)
    {   // Clear the PG links.
        memset(pgLinks, 0, sizeof(pglink_t *) * numSectors);
        pgCursor = 0;
    }

    if(useParticles)
    {
        // Link all active generators to sectors.
        for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
        {
            ptcgen_t*           gen;

            if((gen = activePtcGens[i]) != NULL)
            {
                int                 k;

                // \fixme Overkill?
                for(k = 0; k < gen->count; ++k)
                    if(gen->ptcs[k].stage >= 0)
                        PG_LinkPtcGen(gen, GET_SECTOR_IDX(gen->ptcs[k].sector));
            }
        }
    }

END_PROF(PROF_PTCGEN_LINK);
}

/**
 * Set gen->count prior to calling this function.
 */
static void P_InitParticleGen(ptcgen_t* gen, const ded_ptcgen_t* def)
{
    int                 i;

    if(gen->count <= 0)
        gen->count = 1;

    // Make sure no generator is type-triggered by default.
    gen->type = gen->type2 = -1;

    gen->def = def;
    gen->flags = def->flags;
    gen->ptcs = Z_Calloc(sizeof(particle_t) * gen->count, PU_MAP, 0);
    gen->stages = Z_Calloc(sizeof(ptcstage_t) * def->stageCount.num,
                           PU_MAP, 0);

    for(i = 0; i < def->stageCount.num; ++i)
    {
        const ded_ptcstage_t* sdef = &def->stages[i];
        ptcstage_t*         s = &gen->stages[i];

        s->bounce = FLT2FIX(sdef->bounce);
        s->resistance = FLT2FIX(1 - sdef->resistance);
        s->radius = FLT2FIX(sdef->radius);
        s->gravity = FLT2FIX(sdef->gravity);
        s->type = sdef->type;
        s->flags = sdef->flags;
    }

    // Init some data.
    for(i = 0; i < 3; ++i)
    {
        gen->center[i] = FLT2FIX(def->center[i]);
        gen->vector[i] = FLT2FIX(def->vector[i]);
    }

    // Apply a random component to the spawn vector.
    if(def->initVectorVariance > 0)
    {
        P_Uncertain(gen->vector, 0, FLT2FIX(def->initVectorVariance));
    }

    // Mark unused.
    for(i = 0; i < gen->count; ++i)
    {
        particle_t*         pt = &gen->ptcs[i];

        pt->stage = -1;
    }
}

static void P_PresimParticleGen(ptcgen_t* gen, int tics)
{
    for(; tics > 0; tics--)
        P_PtcGenThinker(gen);

    // Reset age so presim doesn't affect it.
    gen->age = 0;
}

/**
 * Creates a new mobj-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
void P_SpawnParticleGen(const ded_ptcgen_t* def, mobj_t* source)
{
    ptcgen_t*           gen;

    if(isDedicated || !useParticles || !(gen = P_NewPtcGen()))
        return;

/*#if _DEBUG
Con_Message("SpawnPtcGen: %s/%i (src:%s typ:%s mo:%p)\n",
            def->state, def - defs.ptcgens, defs.states[source->state-states].id,
            defs.mobjs[source->type].id, source);
#endif*/

    // Initialize the particle generator.
    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & PGF_SCALED_RATE)
    {
        gen->spawnRateMultiplier = source->subsector->sector->approxArea;
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    P_InitParticleGen(gen, def);
    gen->source = source;
    gen->srcid = source->thinker.id;

    // Is there a need to pre-simulate?
    P_PresimParticleGen(gen, def->preSim);
}

/**
 * Creates a new flat-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
static void P_SpawnPlaneParticleGen(const ded_ptcgen_t* def, sector_t* sec,
                                    boolean isCeiling)
{
    ptcgen_t*           gen;

    if(isDedicated || !useParticles)
        return;

    if(!(gen = P_NewPtcGen()))
        return;

    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & PGF_PARTS_PER_128)
    {
        gen->spawnRateMultiplier = sec->approxArea;
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    // Initialize the particle generator.
    P_InitParticleGen(gen, def);
    gen->sector = sec;
    gen->ceiling = isCeiling;

    // Is there a need to pre-simulate?
    P_PresimParticleGen(gen, def->preSim);
}

/**
 * The offset is spherical and random.
 * Low and High should be positive.
 */
static void P_Uncertain(fixed_t* pos, fixed_t low, fixed_t high)
{
    int                 i;
    fixed_t             off, theta, phi, vec[3];

    if(!low)
    {
        // The simple, cubic algorithm.
        for(i = 0; i < 3; ++i)
            pos[i] += (high * (RNG_RandByte() - RNG_RandByte())) * reciprocal255;
    }
    else
    {
        // The more complicated, spherical algorithm.
        off = ((high - low) * (RNG_RandByte() - RNG_RandByte())) * reciprocal255;
        off += off < 0 ? -low : low;

        theta = RNG_RandByte() << (24 - ANGLETOFINESHIFT);
        phi =
            acos(2 * (RNG_RandByte() * reciprocal255) -
                 1) / PI * (ANGLE_180 >> ANGLETOFINESHIFT);

        vec[VX] = FixedMul(fineCosine[theta], finesine[phi]);
        vec[VY] = FixedMul(finesine[theta], finesine[phi]);
        vec[VZ] = FixedMul(fineCosine[phi], FLT2FIX(0.8333f));

        for(i = 0; i < 3; ++i)
            pos[i] += FixedMul(vec[i], off);
    }
}

static void P_SetParticleAngles(particle_t* pt, int flags)
{
    if(flags & PTCF_ZERO_YAW)
        pt->yaw = 0;
    if(flags & PTCF_ZERO_PITCH)
        pt->pitch = 0;
    if(flags & PTCF_RANDOM_YAW)
        pt->yaw = RNG_RandFloat() * 65536;
    if(flags & PTCF_RANDOM_PITCH)
        pt->pitch = RNG_RandFloat() * 65536;
}

static void P_ParticleSound(fixed_t pos[3], ded_embsound_t* sound)
{
    int                 i;
    float               orig[3];

    // Is there any sound to play?
    if(!sound->id || sound->volume <= 0)
        return;

    for(i = 0; i < 3; ++i)
        orig[i] = FIX2FLT(pos[i]);

    S_LocalSoundAtVolumeFrom(sound->id, NULL, orig, sound->volume);
}

/**
 * Spawns a new particle.
 */
static void P_NewParticle(ptcgen_t* gen)
{
    const ded_ptcgen_t* def = gen->def;
    particle_t*         pt;
    int                 i;
    fixed_t             uncertain, len;
    angle_t             ang, ang2;
    subsector_t*        subsec;
    float*              box, inter = -1;
    modeldef_t*         mf = 0, *nextmf = 0;

    // Check for model-only generators.
    if(gen->source)
    {
        inter = R_CheckModelFor(gen->source, &mf, &nextmf);
        if(((!mf || !useModels) && def->flags & PGF_MODEL_ONLY) ||
           (mf && useModels && mf->flags & MFF_NO_PARTICLES))
            return;
    }

    // Keep the spawn cursor in the valid range.
    if(++gen->spawnCP >= gen->count)
        gen->spawnCP -= gen->count;

    // Set the particle's data.
    pt = gen->ptcs + gen->spawnCP;
    pt->stage = 0;
    if(RNG_RandFloat() < def->altStartVariance)
        pt->stage = def->altStart;
    pt->tics = def->stages[pt->stage].tics *
        (1 - def->stages[pt->stage].variance * RNG_RandFloat());

    // Launch vector.
    pt->mov[VX] = gen->vector[VX];
    pt->mov[VY] = gen->vector[VY];
    pt->mov[VZ] = gen->vector[VZ];

    // Apply some random variance.
    pt->mov[VX] +=
        FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pt->mov[VY] +=
        FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pt->mov[VZ] +=
        FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));

    // Apply some aspect ratio scaling to the momentum vector.
    // This counters the 200/240 difference nearly completely.
    pt->mov[VX] = FixedMul(pt->mov[VX], FLT2FIX(1.1f));
    pt->mov[VY] = FixedMul(pt->mov[VY], FLT2FIX(0.95f));
    pt->mov[VZ] = FixedMul(pt->mov[VZ], FLT2FIX(1.1f));

    // Set proper speed.
    uncertain = FLT2FIX(
        def->speed * (1 - def->speedVariance * RNG_RandFloat()));
    len = FLT2FIX(P_ApproxDistance(
        P_ApproxDistance(FIX2FLT(pt->mov[VX]), FIX2FLT(pt->mov[VY])),
                                   FIX2FLT(pt->mov[VZ])));
    if(!len)
        len = FRACUNIT;
    len = FixedDiv(uncertain, len);

    pt->mov[VX] = FixedMul(pt->mov[VX], len);
    pt->mov[VY] = FixedMul(pt->mov[VY], len);
    pt->mov[VZ] = FixedMul(pt->mov[VZ], len);

    // The source is a mobj?
    if(gen->source)
    {
        if(gen->flags & PGF_RELATIVE_VECTOR)
        {
            // Rotate the vector using the source angle.
            float               temp[3];

            temp[VX] = FIX2FLT(pt->mov[VX]);
            temp[VY] = FIX2FLT(pt->mov[VY]);
            temp[VZ] = 0;

            // Player visangles have some problems, let's not use them.
            M_RotateVector(temp,
                           gen->source->angle / (float) ANG180 * -180 + 90,
                           0);
            pt->mov[VX] = FLT2FIX(temp[VX]);
            pt->mov[VY] = FLT2FIX(temp[VY]);
        }

        if(gen->flags & PGF_RELATIVE_VELOCITY)
        {
            pt->mov[VX] += FLT2FIX(gen->source->mom[MX]);
            pt->mov[VY] += FLT2FIX(gen->source->mom[MY]);
            pt->mov[VZ] += FLT2FIX(gen->source->mom[MZ]);
        }

        // Position.
        pt->pos[VX] = FLT2FIX(gen->source->pos[VX]);
        pt->pos[VY] = FLT2FIX(gen->source->pos[VY]);
        pt->pos[VZ] = FLT2FIX(gen->source->pos[VZ] - gen->source->floorClip);

        P_Uncertain(pt->pos, FLT2FIX(def->spawnRadiusMin),
                    FLT2FIX(def->spawnRadius));

        // Offset to the real center.
        pt->pos[VZ] += gen->center[VZ];

        // Calculate XY center with mobj angle.
        ang =
            (useSRVOAngle ? (gen->source->visAngle << 16) : gen->source->
             angle) + (fixed_t) (FIX2FLT(gen->center[VY]) / 180.0f * ANG180);
        ang2 = (ang + ANG90) >> ANGLETOFINESHIFT;
        ang >>= ANGLETOFINESHIFT;
        pt->pos[VX] += FixedMul(fineCosine[ang], gen->center[VX]);
        pt->pos[VY] += FixedMul(finesine[ang], gen->center[VX]);

        // There might be an offset from the model of the mobj.
        if(mf &&
           (mf->sub[0].flags & MFF_PARTICLE_SUB1 || def->subModel >= 0))
        {
            int                 subidx;
            float               off[3];

            // Select the right submodel to use as the origin.
            if(def->subModel >= 0)
                subidx = def->subModel;
            else
                subidx = 1; // Default to submodel #1.

            // Interpolate the offset.
            if(inter > 0 && nextmf)
            {
                off[VX] = nextmf->ptcOffset[subidx][VX] - mf->ptcOffset[subidx][VX];
                off[VY] = nextmf->ptcOffset[subidx][VY] - mf->ptcOffset[subidx][VY];
                off[VZ] = nextmf->ptcOffset[subidx][VZ] - mf->ptcOffset[subidx][VZ];

                off[VX] *= inter;
                off[VY] *= inter;
                off[VZ] *= inter;
            }

            off[VX] += mf->ptcOffset[subidx][VX];
            off[VY] += mf->ptcOffset[subidx][VY];
            off[VZ] += mf->ptcOffset[subidx][VZ];

            // Apply it to the particle coords.
            pt->pos[VX] += FixedMul(fineCosine[ang], FLT2FIX(off[VX]));
            pt->pos[VX] += FixedMul(fineCosine[ang2], FLT2FIX(off[VZ]));
            pt->pos[VY] += FixedMul(finesine[ang], FLT2FIX(off[VX]));
            pt->pos[VY] += FixedMul(finesine[ang2], FLT2FIX(off[VZ]));
            pt->pos[VZ] += FLT2FIX(off[VY]);
        }
    }
    else if(gen->sector) // The source is a plane?
    {
        fixed_t             radius = gen->stages[pt->stage].radius;

        // Choose a random spot inside the sector, on the spawn plane.
        if(gen->flags & PGF_SPACE_SPAWN)
        {
            pt->pos[VZ] =
                FLT2FIX(gen->sector->SP_floorheight) + radius +
                FixedMul(RNG_RandByte() << 8,
                         FLT2FIX(gen->sector->SP_ceilheight -
                                 gen->sector->SP_floorheight) - 2 * radius);
        }
        else if(gen->flags & PGF_FLOOR_SPAWN ||
                (!(gen->flags & (PGF_FLOOR_SPAWN | PGF_CEILING_SPAWN)) &&
                 !gen->ceiling))
        {
            // Spawn on the floor.
            pt->pos[VZ] = FLT2FIX(gen->sector->SP_floorheight) + radius;
        }
        else
        {
            // Spawn on the ceiling.
            pt->pos[VZ] = FLT2FIX(gen->sector->SP_ceilheight) - radius;
        }

        /**
         * Choosing the XY spot is a bit more difficult.
         * But we must be fast and only sufficiently accurate.
         *
         * \fixme Nothing prevents spawning on the wrong side (or inside)
         * of one-sided walls (large diagonal subsectors!).
         */
        box = gen->sector->bBox;
        for(i = 0; i < 5; ++i) // Try a couple of times (max).
        {
            float               x =
                (box[BOXLEFT]   + RNG_RandFloat() * (box[BOXRIGHT] - box[BOXLEFT]));
            float               y =
                (box[BOXBOTTOM] + RNG_RandFloat() * (box[BOXTOP]   - box[BOXBOTTOM]));

            subsec = R_PointInSubsector(x, y);

            if(subsec->sector == gen->sector)
                break;
            else
                subsec = NULL;
        }
        if(!subsec)
            goto spawn_failed;

        // Try a couple of times to get a good random spot.
        for(i = 0; i < 10; ++i) // Max this many tries before giving up.
        {
            float               x = subsec->bBox[0].pos[VX] +
                RNG_RandFloat() * (subsec->bBox[1].pos[VX] - subsec->bBox[0].pos[VX]);
            float               y = subsec->bBox[0].pos[VY] +
                RNG_RandFloat() * (subsec->bBox[1].pos[VY] - subsec->bBox[0].pos[VY]);

            pt->pos[VX] = FLT2FIX(x);
            pt->pos[VY] = FLT2FIX(y);

            if(R_PointInSubsector(x, y) == subsec)
                break; // This is a good place.
        }

        if(i == 10) // No good place found?
        {
          spawn_failed:
            pt->stage = -1; // Damn.
            return;
        }
    }
    else if(gen->flags & PGF_UNTRIGGERED)
    {
        // The center position is the spawn origin.
        pt->pos[VX] = gen->center[VX];
        pt->pos[VY] = gen->center[VY];
        pt->pos[VZ] = gen->center[VZ];
        P_Uncertain(pt->pos, FLT2FIX(def->spawnRadiusMin),
                    FLT2FIX(def->spawnRadius));
    }

    // Initial angles for the particle.
    P_SetParticleAngles(pt, def->stages[pt->stage].flags);

    // The other place where this gets updated is after moving over
    // a two-sided line.
    if(gen->sector)
        pt->sector = gen->sector;
    else
        pt->sector = R_PointInSubsector(FIX2FLT(pt->pos[VX]),
                                        FIX2FLT(pt->pos[VY]))->sector;

    // Play a stage sound?
    P_ParticleSound(pt->pos, &def->stages[pt->stage].sound);
}

/**
 * Callback for the client mobj iterator, called from P_PtcGenThinker.
 */
boolean PIT_ClientMobjParticles(clmobj_t* cmo, void* context)
{
    ptcgen_t*           gen = (ptcgen_t*) context;

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

/**
 * Spawn multiple new particles using all applicable sources.
 */
static boolean manyNewParticles(thinker_t* th, void* context)
{
    ptcgen_t*           gen = (ptcgen_t*) context;
    mobj_t*             mo = (mobj_t *) th;

    // Type match?
    if(mo->type == gen->type || mo->type == gen->type2)
    {
        // Someone might think this is a slight hack...
        gen->source = mo;
        P_NewParticle(gen);
    }

    return true; // Continue iteration.
}

boolean PIT_CheckLinePtc(linedef_t* ld, void* data)
{
    fixed_t             ceil, floor;
    sector_t*           front, *back;

    if(mbox[1][VX] <= ld->bBox[BOXLEFT] || mbox[0][VX] >= ld->bBox[BOXRIGHT] ||
       mbox[1][VY] <= ld->bBox[BOXBOTTOM] || mbox[0][VY] >= ld->bBox[BOXTOP])
    {
        return true; // Bounding box misses the line completely.
    }

    // Movement must cross the line.
    if(P_PointOnLinedefSide(FIX2FLT(tmpx1), FIX2FLT(tmpy1), ld) ==
       P_PointOnLinedefSide(FIX2FLT(tmpx2), FIX2FLT(tmpy2), ld))
        return true;

    // We are possibly hitting something here.
    ptcHitLine = ld;
    if(!ld->L_backside)
        return false; // Boing!

    // Determine the opening we have here.
    front = ld->L_frontsector;
    back = ld->L_backsector;
    if(front->SP_ceilheight < back->SP_ceilheight)
        ceil = FLT2FIX(front->SP_ceilheight);
    else
        ceil = FLT2FIX(back->SP_ceilheight);

    if(front->SP_floorheight > back->SP_floorheight)
        floor = FLT2FIX(front->SP_floorheight);
    else
        floor = FLT2FIX(back->SP_floorheight);

    // There is a backsector. We possibly might hit something.
    if(tmpz - tmprad < floor || tmpz + tmprad > ceil)
        return false; // Boing!

    // There is a possibility that the new position is in a new sector.
    tmcross = true; // Afterwards, update the sector pointer.

    // False alarm, continue checking.
    return true;
}

/**
 * Particle touches something solid. Returns false iff the particle dies.
 */
static int P_TouchParticle(particle_t* pt, ptcstage_t* stage,
                           ded_ptcstage_t* stageDef, boolean touchWall)
{
    // Play a hit sound.
    P_ParticleSound(pt->pos, &stageDef->hitSound);

    if(stage->flags & PTCF_DIE_TOUCH)
    {
        // Particle dies from touch.
        pt->stage = -1;
        return false;
    }

    if((stage->flags & PTCF_STAGE_TOUCH) ||
       (touchWall && (stage->flags & PTCF_STAGE_WALL_TOUCH)) ||
       (!touchWall && (stage->flags & PTCF_STAGE_FLAT_TOUCH)))
    {
        // Particle advances to the next stage.
        pt->tics = 0;
    }

    // Particle survives the touch.
    return true;
}

/**
 * Semi-fixed, actually.
 */
#if 0 // Currently unused.
static void P_FixedCrossProduct(float* fa, fixed_t* b, fixed_t* result)
{
    result[VX] = FixedMul(FRACUNIT * fa[VY], b[VZ]) -
        FixedMul(FRACUNIT * fa[VZ], b[VY]);
    result[VY] = FixedMul(FRACUNIT * fa[VZ], b[VX]) -
        FixedMul(FRACUNIT * fa[VX], b[VZ]);
    result[VZ] = FixedMul(FRACUNIT * fa[VX], b[VY]) -
        FixedMul(FRACUNIT * fa[VY], b[VX]);
}
#endif

#if 0 // Currently unused.
fixed_t P_FixedDotProduct(fixed_t* a, fixed_t* b)
{
    return FixedMul(a[VX], b[VX]) + FixedMul(a[VY], b[VY]) +
           FixedMul(a[VZ], b[VZ]);
}
#endif

/**
 * Takes care of consistent variance.
 * Currently only used visually, collisions use the constant radius.
 * The variance can be negative (results will be larger).
 */
float P_GetParticleRadius(const ded_ptcstage_t* def, int ptcIDX)
{
    static const float rnd[16] = { .875f, .125f, .3125f, .75f, .5f, .375f,
        .5625f, .0625f, 1, .6875f, .625f, .4375f, .8125f, .1875f,
        .9375f, .25f
    };

    if(!def->radiusVariance)
        return def->radius;

    return (rnd[ptcIDX & 0xf] * def->radiusVariance +
            (1 - def->radiusVariance)) * def->radius;
}

/**
 * A particle may be attached to the floor or ceiling of the sector.
 */
float P_GetParticleZ(const particle_t* pt)
{
    if(pt->pos[VZ] == DDMAXINT)
        return pt->sector->SP_ceilvisheight - 2;
    else if(pt->pos[VZ] == DDMININT)
        return (pt->sector->SP_floorvisheight + 2);

    return FIX2FLT(pt->pos[VZ]);
}

static void P_SpinParticle(ptcgen_t* gen, particle_t* pt)
{
    static const int    yawSigns[4]   = { 1,  1, -1, -1 };
    static const int    pitchSigns[4] = { 1, -1,  1, -1 };

    ded_ptcstage_t*     stDef = &gen->def->stages[pt->stage];
    uint                index = pt - &gen->ptcs[P_PtcGenToIndex(gen) / 8];
    int                 yawSign, pitchSign;

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

    pt->yaw *= 1 - stDef->spinResistance[0];
    pt->pitch *= 1 - stDef->spinResistance[1];
}

/**
 * The movement is done in two steps:
 * Z movement is done first. Skyflat kills the particle.
 * XY movement checks for hits with solid walls (no backsector).
 * This is supposed to be fast and simple (but not too simple).
 */
static void P_MoveParticle(ptcgen_t* gen, particle_t* pt)
{
    ptcstage_t*         st = &gen->stages[pt->stage];
    ded_ptcstage_t*     stDef = &gen->def->stages[pt->stage];
    boolean             zBounce = false, hitFloor = false;
    vec2_t              point;
    fixed_t             x, y, z, hardRadius = st->radius / 2;

    // Particle rotates according to spin speed.
    P_SpinParticle(gen, pt);

    // Changes to momentum.
    pt->mov[VZ] -= FixedMul(FLT2FIX(mapGravity), st->gravity);

    // Vector force.
    if(stDef->vectorForce[VX] != 0 || stDef->vectorForce[VY] != 0 ||
       stDef->vectorForce[VZ] != 0)
    {
        int                 i;

        for(i = 0; i < 3; ++i)
            pt->mov[i] += FLT2FIX(stDef->vectorForce[i]);
    }

    // Sphere force pull and turn.
    // Only applicable to sourced or untriggered generators. For other
    // types it's difficult to define the center coordinates.
    if((st->flags & PTCF_SPHERE_FORCE) &&
       (gen->source || gen->flags & PGF_UNTRIGGERED))
    {
        int                 i;
        float               delta[3], dist;

        if(gen->source)
        {
            delta[VX] = FIX2FLT(pt->pos[VX]) - gen->source->pos[VX];
            delta[VY] = FIX2FLT(pt->pos[VY]) - gen->source->pos[VY];
            delta[VZ] = P_GetParticleZ(pt) - (gen->source->pos[VZ] +
                FIX2FLT(gen->center[VZ]));
        }
        else
        {
            for(i = 0; i < 3; ++i)
                delta[i] = FIX2FLT(pt->pos[i] - gen->center[i]);
        }

        // Apply the offset (to source coords).
        for(i = 0; i < 3; ++i)
            delta[i] -= gen->def->forceOrigin[i];

        // Counter the aspect ratio of old times.
        delta[VZ] *= 1.2f;

        dist =
            P_ApproxDistance(P_ApproxDistance(delta[VX], delta[VY]),
                             delta[VZ]);
        if(dist != 0)
        {
            // Radial force pushes the particles on the surface of a sphere.
            if(gen->def->force)
            {
                // Normalize delta vector, multiply with (dist - forceRadius),
                // multiply with radial force strength.
                for(i = 0; i < 3; ++i)
                    pt->mov[i] -= FLT2FIX(
                        ((delta[i] / dist) * (dist - gen->def->forceRadius)) *
                        gen->def->force);
            }

            // Rotate!
            if(gen->def->forceAxis[VX] || gen->def->forceAxis[VY] ||
               gen->def->forceAxis[VZ])
            {
                float               cross[3];

                M_CrossProduct(gen->def->forceAxis, delta, cross);
                for(i = 0; i < 3; ++i)
                    pt->mov[i] += FLT2FIX(cross[i]) >> 8;
            }
        }
    }

    if(st->resistance != FRACUNIT)
    {
        int                 i;

        for(i = 0; i < 3; ++i)
            pt->mov[i] = FixedMul(pt->mov[i], st->resistance);
    }

    // The particle is 'soft': half of radius is ignored.
    // The exception is plane flat particles, which are rendered flat
    // against planes. They are almost entirely soft when it comes to plane
    // collisions.
    if(st->flags & PTCF_PLANE_FLAT)
        hardRadius = FRACUNIT;

    // Check the new Z position only if not stuck to a plane.
    z = pt->pos[VZ] + pt->mov[VZ];
    if(pt->pos[VZ] != DDMININT && pt->pos[VZ] != DDMAXINT)
    {
        if(z > FLT2FIX(pt->sector->SP_ceilheight) - hardRadius)
        {
            // The Z is through the roof!
            if(R_IsSkySurface(&pt->sector->SP_ceilsurface))
            {
                // Special case: particle gets lost in the sky.
                pt->stage = -1;
                return;
            }

            if(!P_TouchParticle(pt, st, stDef, false))
                return;

            z = FLT2FIX(pt->sector->SP_ceilheight) - hardRadius;
            zBounce = true;
            hitFloor = false;
        }

        // Also check the floor.
        if(z < FLT2FIX(pt->sector->SP_floorheight) + hardRadius)
        {
            if(R_IsSkySurface(&pt->sector->SP_floorsurface))
            {
                pt->stage = -1;
                return;
            }

            if(!P_TouchParticle(pt, st, stDef, false))
                return;

            z = FLT2FIX(pt->sector->SP_floorheight) + hardRadius;
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
                    z = hitFloor ? DDMININT : DDMAXINT;
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
            sector_t*           front, *back;

            front = (pt->contact->L_frontside? pt->contact->L_frontsector : NULL);
            back = (pt->contact->L_backside? pt->contact->L_backsector : NULL);

            if(front && back && abs(pt->mov[VZ]) < FRACUNIT / 2)
            {
                float               pz = P_GetParticleZ(pt);
                float               fz, cz;

                //// \fixme $nplanes
                if(front->SP_floorheight > back->SP_floorheight)
                    fz = front->SP_floorheight;
                else
                    fz = back->SP_floorheight;

                if(front->SP_ceilheight < back->SP_ceilheight)
                    cz = front->SP_ceilheight;
                else
                    cz = back->SP_ceilheight;

                // If the particle is in the opening of a 2-sided line, it's
                // quite likely that it shouldn't be here...
                if(pz > fz && pz < cz)
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
    V2_Set(point, FIX2FLT(MIN_OF(x, pt->pos[VX]) - st->radius),
                  FIX2FLT(MIN_OF(y, pt->pos[VY]) - st->radius));
    V2_InitBox(mbox, point);
    V2_Set(point, FIX2FLT(MAX_OF(x, pt->pos[VX]) + st->radius),
                  FIX2FLT(MAX_OF(y, pt->pos[VY]) + st->radius));
    V2_AddToBox(mbox, point);

    // Iterate the lines in the contacted blocks.

    validCount++;
    if(!P_AllLinesBoxIteratorv(mbox, PIT_CheckLinePtc, 0))
    {
        fixed_t             normal[2], dotp;

        // Must survive the touch.
        if(!P_TouchParticle(pt, st, stDef, true))
            return;

        // There was a hit! Calculate bounce vector.
        // - Project movement vector on the normal of hitline.
        // - Calculate the difference to the point on the normal.
        // - Add the difference to movement vector, negate movement.
        // - Multiply with bounce.

        // Calculate the normal.
        normal[VX] = -FLT2FIX(ptcHitLine->dX);
        normal[VY] = -FLT2FIX(ptcHitLine->dY);

        if(!normal[VX] && !normal[VY])
            goto quit_iteration;

        // Calculate as floating point so we don't overflow.
        dotp = FRACUNIT * (DOT2F(pt->mov, normal) /
                           DOT2F(normal, normal));
        VECMUL(normal, dotp);
        VECSUB(normal, pt->mov);
        VECMULADD(pt->mov, 2 * FRACUNIT, normal);
        VECMUL(pt->mov, st->bounce);

        // Continue from the old position.
        x = pt->pos[VX];
        y = pt->pos[VY];
        tmcross = false; // Sector can't change if XY doesn't.

        // This line is the latest contacted line.
        pt->contact = ptcHitLine;
        goto quit_iteration;
    }

  quit_iteration:
    // The move is now OK.
    pt->pos[VX] = x;
    pt->pos[VY] = y;

    // Should we update the sector pointer?
    if(tmcross)
        pt->sector = R_PointInSubsector(FIX2FLT(x), FIX2FLT(y))->sector;
}

/**
 * Spawn and move particles.
 */
void P_PtcGenThinker(ptcgen_t* gen)
{
    int                 i;
    particle_t*         pt;
    float               newparts;
    const ded_ptcgen_t* def = gen->def;

    // Source has been destroyed?
    if(!(gen->flags & PGF_UNTRIGGERED) && !P_IsUsedMobjID(gen->srcid))
    {
        // Blasted... Spawning new particles becomes impossible.
        gen->source = NULL;
    }

    // Time to die?
    if(++gen->age > def->maxAge && def->maxAge >= 0)
    {
        P_PtcGenDestroy(gen);
        return;
    }

    // Spawn new particles?
    if((gen->age <= def->spawnAge || def->spawnAge < 0) &&
       (gen->source || gen->sector || gen->type >= 0 ||
        (gen->flags & PGF_UNTRIGGERED)))
    {
        newparts = def->spawnRate * gen->spawnRateMultiplier;

        newparts *= particleSpawnRate *
            (1 - def->spawnRateVariance * RNG_RandFloat());

        gen->spawnCount += newparts;
        while(gen->spawnCount >= 1)
        {
            // Spawn a new particle.
            if(gen->type >= 0) // Type-triggered?
            {
                // Client's should also check the client mobjs.
                if(isClient)
                {
                    Cl_MobjIterator(PIT_ClientMobjParticles, gen);
                }

                P_IterateThinkers(gx.MobjThinker, 0x1, // All mobjs are public
                                  manyNewParticles, gen);

                // The generator has no real source.
                gen->source = NULL;
            }
            else
            {
                P_NewParticle(gen);
            }

            gen->spawnCount--;
        }
    }

    // Move particles.
    for(i = 0, pt = gen->ptcs; i < gen->count; ++i, pt++)
    {
        if(pt->stage < 0)
            continue;           // Not in use.
        if(pt->tics-- <= 0)
        {
            // Advance to next stage.
            if(++pt->stage == def->stageCount.num ||
               gen->stages[pt->stage].type == PTC_NONE)
            {
                // Kill the particle.
                pt->stage = -1;
                continue;
            }

            pt->tics =
                def->stages[pt->stage].tics * (1 - def->stages[pt->stage].
                                               variance * RNG_RandFloat());

            // Change in particle angles?
            P_SetParticleAngles(pt, def->stages[pt->stage].flags);

            // A sound?
            P_ParticleSound(pt->pos, &def->stages[pt->stage].sound);
        }

        // Try to move.
        P_MoveParticle(gen, pt);
    }
}

/**
 * Returns true iff there is an active ptcgen for the given plane.
 */
static boolean P_HasActivePtcGen(sector_t* sector, int isCeiling)
{
    ptcgenid_t          i;

    for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
        if(activePtcGens[i] && activePtcGens[i]->sector == sector &&
           activePtcGens[i]->ceiling == isCeiling)
            return true;
    return false;
}

/**
 * Spawns new ptcgens for planes, if necessary.
 */
void P_CheckPtcPlanes(void)
{
    uint                i, p;
    sector_t*           sector;

    if(isDedicated || !useParticles)
        return;

    // There is no need to do this on every tic.
    if(SECONDS_TO_TICKS(gameTime) % 4)
        return;

    for(i = 0; i < numSectors; ++i)
    {
        sector = SECTOR_PTR(i);
        for(p = 0; p < 2; ++p)
        {
            uint                plane = p;
            material_t*         mat = sector->SP_planematerial(plane);
            const ded_ptcgen_t* def = Material_GetPtcGen(mat);

            if(!def)
                continue;

            if(def->flags & PGF_CEILING_SPAWN)
                plane = 1;
            if(def->flags & PGF_FLOOR_SPAWN)
                plane = 0;

            if(!P_HasActivePtcGen(sector, plane))
            {
                // Spawn it!
                P_SpawnPlaneParticleGen(def, sector, plane);
            }
        }
    }
}

/**
 * Spawns all type-triggered particle generators, regardless of whether
 * the type of mobj exists in the map or not (mobjs might be dynamically
 * created).
 */
void P_SpawnTypeParticleGens(void)
{
    int                 i;
    ded_ptcgen_t*       def;
    ptcgen_t*           gen;

    if(isDedicated || !useParticles)
        return;

    for(i = 0, def = defs.ptcGens; i < defs.count.ptcGens.num; ++i, def++)
    {
        if(def->typeNum < 0)
            continue;
        if(!(gen = P_NewPtcGen()))
            return;             // No more generators.

        // Initialize the particle generator.
        gen->count = def->particles;
        gen->spawnRateMultiplier = 1;

        P_InitParticleGen(gen, def);
        gen->type = def->typeNum;
        gen->type2 = def->type2Num;

        // Is there a need to pre-simulate?
        P_PresimParticleGen(gen, def->preSim);
    }
}

void P_SpawnMapParticleGens(const char* mapId)
{
    int                 i;
    ded_ptcgen_t*       def;
    ptcgen_t*           gen;

    if(isDedicated || !useParticles)
        return;

    for(i = 0, def = defs.ptcGens; i < defs.count.ptcGens.num; ++i, def++)
    {
        if(!def->map[0] || stricmp(def->map, mapId))
            continue;

        if(def->spawnAge > 0 && ddMapTime > def->spawnAge)
            continue; // No longer spawning this generator.

        if(!(gen = P_NewPtcGen()))
            return; // No more generators.

        // Initialize the particle generator.
        gen->count = def->particles;
        gen->spawnRateMultiplier = 1;

        P_InitParticleGen(gen, def);
        gen->flags |= PGF_UNTRIGGERED;

        // Is there a need to pre-simulate?
        P_PresimParticleGen(gen, def->preSim);
    }
}

/**
 * A public function (games can call this directly).
 */
void P_SpawnDamageParticleGen(mobj_t* mo, mobj_t* inflictor, int amount)
{
    const ded_ptcgen_t* def;

    // Are particles allowed?
    if(isDedicated || !useParticles)
        return;

    if(!mo || !inflictor || amount <= 0)
        return;

    if((def = Def_GetDamageGenerator(mo->type)))
    {
        ptcgen_t*           gen;
        vec3_t              vector, vecDelta;

        // Create it.
        if(!(gen = P_NewPtcGen()))
            return; // No more generators.

        gen->count = def->particles;
        P_InitParticleGen(gen, def);

        gen->flags |= PGF_UNTRIGGERED;
        gen->spawnRateMultiplier = MAX_OF(amount, 1);

        // Calculate appropriate center coordinates.
        gen->center[VX] += FLT2FIX(mo->pos[VX]);
        gen->center[VY] += FLT2FIX(mo->pos[VY]);
        gen->center[VZ] += FLT2FIX(mo->pos[VZ] + mo->height / 2);

        // Calculate launch vector.
        V3_Set(vecDelta, inflictor->pos[VX] - mo->pos[VX],
               inflictor->pos[VY] - mo->pos[VY],
               (inflictor->pos[VZ] - inflictor->height / 2) -
                  (mo->pos[VZ] + mo->height / 2));

        V3_SetFixed(vector, gen->vector[VX], gen->vector[VY], gen->vector[VZ]);
        V3_Sum(vector, vector, vecDelta);
        V3_Normalize(vector);

        gen->vector[VX] = FLT2FIX(vector[VX]);
        gen->vector[VY] = FLT2FIX(vector[VY]);
        gen->vector[VZ] = FLT2FIX(vector[VZ]);

        // Is there a need to pre-simulate?
        P_PresimParticleGen(gen, def->preSim);
    }
}

/**
 * Called after a reset once the definitions have been re-read.
 */
void P_UpdateParticleGens(void)
{
    ptcgenid_t          i;

    for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
    {
        int                 j;
        ded_ptcgen_t*       def;
        boolean             found;
        ptcgen_t*           gen;

        if(!activePtcGens[i])
            continue;

        gen = activePtcGens[i];

        // Map generators cannot be updated (we have no means to reliably
        // identify them), so destroy them.
        // Flat generators will be spawned automatically within a few tics so we'll
        // just destroy those too.
        if((gen->flags & PGF_UNTRIGGERED) || gen->sector)
        {
            P_PtcGenDestroy(gen);
            continue;
        }

        // Search for a suitable definition.
        j = 0;
        def = defs.ptcGens;
        found = false;
        while(j < defs.count.ptcGens.num && !found)
        {
            // A type generator?
            if(def->typeNum >= 0 &&
               (gen->type == def->typeNum || gen->type2 == def->type2Num))
            {
                found = true;
            }
            // A damage generator?
            else if(gen->source && gen->source->type == def->damageNum)
            {
                found = true;
            }
            // A state generator?
            else if(gen->source && def->state[0] &&
                    gen->source->state - states == Def_GetStateNum(def->state))
            {
                found = true;
            }
            else
            {
                j++;
                def++;
            }
        }

        if(found)
        {   // Update the generator using the new definition.
            gen->def = def;
        }
        else
        {   // Nothing else we can do, destroy it.
            P_PtcGenDestroy(gen);
        }
    }

    // Re-spawn map generators.
    P_SpawnMapParticleGens(mapID);
}

/**
 * Walk the entire list of particle generators.
 */
boolean P_IteratePtcGens(boolean (*callback) (ptcgen_t*, void*),
                         void* context)
{
    ptcgenid_t          i;
    boolean             result = true;

    for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
    {
        if(!activePtcGens[i])
            continue;

        if((result = callback(activePtcGens[i], context)) == 0)
            break;
    }

    return result;
}

/**
 * Walk the list of sector-linked particle generators.
 */
boolean P_IterateSectorLinkedPtcGens(sector_t* sector,
                                     boolean (*callback) (ptcgen_t*, void*),
                         void* context)
{
    return iterateSectorLinkedPtcGens(sector, callback, context);
}
