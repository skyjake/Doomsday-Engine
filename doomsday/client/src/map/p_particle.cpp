/** @file p_particle.cpp Generator Management (Particles).
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cmath>

#include "de_base.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

#include <de/String>
#include <de/Time>
#include <de/fixedpoint.h>
#include <de/memoryzone.h>

#include "map/generators.h"
#include "render/r_main.h" // validCount
#include "resource/models.h"
#include "api_map.h"
#include "m_misc.h"
#include "m_profiler.h"

#include "map/p_particle.h"

using namespace de;

#define DOT2F(a,b)          ( FIX2FLT(a[VX]) * FIX2FLT(b[VX]) + FIX2FLT(a[VY]) * FIX2FLT(b[VY]) )
#define VECMUL(a,scalar)    ( a[VX] = FixedMul(a[VX], scalar), a[VY] = FixedMul(a[VY], scalar) )
#define VECADD(a,b)         ( a[VX] += b[VX], a[VY] += b[VY] )
#define VECMULADD(a,scal,b) ( a[VX] += FixedMul(scal, b[VX]), a[VY] += FixedMul(scal, b[VY]) )
#define VECSUB(a,b)         ( a[VX] -= b[VX], a[VY] -= b[VY] )
#define VECCPY(a,b)         ( a[VX] = b[VX], a[VY] = b[VY] )

BEGIN_PROF_TIMERS()
  PROF_PTCGEN_LINK
END_PROF_TIMERS()

void P_PtcGenThinker(ptcgen_t *gen);

static void P_Uncertain(fixed_t *pos, fixed_t low, fixed_t high);

byte useParticles = true;
int maxParticles = 0; // Unlimited.
float particleSpawnRate = 1; // Unmodified.

static AABoxd mbox;
static fixed_t tmpz, tmprad, tmpx1, tmpx2, tmpy1, tmpy2;
static boolean tmcross;
static Line *ptcHitLine;

static int releaseGeneratorParticles(ptcgen_t *gen, void *parameters)
{
    DENG_ASSERT(gen);
    DENG_UNUSED(parameters);
    if(gen->ptcs)
    {
        Z_Free(gen->ptcs);
        gen->ptcs = 0;
    }
    return false; // Can be used as an iterator, so continue.
}

void PtcGen_Delete(ptcgen_t *gen)
{
    DENG_ASSERT(gen);
    releaseGeneratorParticles(gen, NULL/*no parameters*/);
    // The generator itself is free'd when it's next turn for thinking comes.
}

static int destroyGenerator(ptcgen_t *gen, void *parameters)
{
    DENG_ASSERT(gen);
    DENG_UNUSED(parameters);

    GameMap *map = theMap; /// @todo Do not assume generator is from the CURRENT map.

    Generators_Unlink(map->generators(), gen);
    GameMap_ThinkerRemove(map, &gen->thinker);

    PtcGen_Delete(gen);
    return false; // Can be used as an iterator, so continue.
}

static int findOldestGenerator(ptcgen_t *gen, void *parameters)
{
    DENG_ASSERT(parameters);
    ptcgen_t **oldest = (ptcgen_t **)parameters;
    if(!(gen->flags & PGF_STATIC) && (!(*oldest) || gen->age > (*oldest)->age))
    {
        *oldest = gen;
    }
    return false; // Continue iteration.
}

static ptcgenid_t findIdForNewGenerator(Generators *gens)
{
    if(!gens) return 0; // None found.

    // Prefer allocating a new generator if we've a spare id.
    ptcgenid_t id = Generators_NextAvailableId(gens);
    if(id >= 0) return id+1;

    // See if there is an existing generator we can supplant.
    /// @todo Optimize: Generators could maintain an age-sorted list.
    ptcgen_t *oldest = NULL;
    Generators_Iterate(gens, findOldestGenerator, (void *)&oldest);
    if(oldest) return Generators_GeneratorId(gens, oldest) + 1; // 1-based index.

    return 0; // None found.
}

/**
 * Allocates a new active ptcgen and adds it to the list of active ptcgens.
 */
static ptcgen_t *P_NewGenerator()
{
    GameMap *map = theMap;
    Generators *gens = map->generators();
    ptcgenid_t id = findIdForNewGenerator(gens);
    if(id)
    {
        // If there is already a generator with that id - remove it.
        ptcgen_t *gen = Generators_Generator(gens, id-1);
        if(gen)
        {
            destroyGenerator(gen, NULL/*no parameters*/);
        }

        /// @todo Linear allocation when in-game is not good...
        gen = (ptcgen_t *) Z_Calloc(sizeof(ptcgen_t), PU_MAP, 0);

        // Link the thinker to the list of (private) thinkers.
        gen->thinker.function = (thinkfunc_t) P_PtcGenThinker;
        GameMap_ThinkerAdd(map, &gen->thinker, false);

        // Link the generator into this collection.
        Generators_Link(gens, id-1, gen);

        return gen;
    }
    return 0; // Creation failed.
}

void P_PtcInitForMap()
{
    de::Time begunAt;

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    P_SpawnTypeParticleGens();
    P_SpawnMapParticleGens();

    LOG_INFO(String("P_PtcInitForMap: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void P_MapSpawnPlaneParticleGens()
{
    if(isDedicated || !useParticles) return;
    if(!theMap) return;

    foreach(Sector *sector, theMap->sectors())
    {
        // Only planes of sectors with volume on the world X/Y axis support generators.
        if(!sector->lineCount()) continue;

        for(uint i = 0; i < 2; ++i)
        {
            Plane &plane = sector->plane(i);
            if(!plane.surface().hasMaterial()) continue;

            de::Uri uri = plane.surface().material().manifest().composeUri();
            ded_ptcgen_t const *def = Def_GetGenerator(reinterpret_cast<uri_s *>(&uri));
            P_SpawnPlaneParticleGen(def, &plane);
        }
    }
}

static int linkGeneratorParticles(ptcgen_t *gen, void *parameters)
{
    Generators *gens = (Generators *)parameters;
    /// @todo Overkill?
    for(int i = 0; i < gen->count; ++i)
    {
        if(gen->ptcs[i].stage < 0 || !gen->ptcs[i].sector) continue;

        /// @todo Do not assume sector is from the CURRENT map.
        Generators_LinkToList(gens, gen, theMap->sectorIndex(gen->ptcs[i].sector));
    }
    return false; // Continue iteration.
}

void P_CreatePtcGenLinks()
{
#ifdef DD_PROFILE
    static int p;
    if(++p > 40)
    {
        p = 0;
        PRINT_PROF(PROF_PTCGEN_LINK);
    }
#endif

    if(!theMap) return;

BEGIN_PROF(PROF_PTCGEN_LINK);

    Generators *gens = theMap->generators();
    Generators_EmptyLists(gens);

    if(useParticles)
    {
        Generators_Iterate(gens, linkGeneratorParticles, gens);
    }

END_PROF(PROF_PTCGEN_LINK);
}

/**
 * Set gen->count prior to calling this function.
 */
static void P_InitParticleGen(ptcgen_t *gen, ded_ptcgen_t const *def)
{
    DENG_ASSERT(gen && def);

    if(gen->count <= 0)
        gen->count = 1;

    // Make sure no generator is type-triggered by default.
    gen->type   = gen->type2 = -1;

    gen->def    = def;
    gen->flags  = def->flags;
    gen->ptcs   = (particle_t *) Z_Calloc(sizeof(particle_t) * gen->count, PU_MAP, 0);
    gen->stages = (ptcstage_t *) Z_Calloc(sizeof(ptcstage_t) * def->stageCount.num, PU_MAP, 0);

    for(int i = 0; i < def->stageCount.num; ++i)
    {
        ded_ptcstage_t const *sdef = &def->stages[i];
        ptcstage_t *s = &gen->stages[i];

        s->bounce     = FLT2FIX(sdef->bounce);
        s->resistance = FLT2FIX(1 - sdef->resistance);
        s->radius     = FLT2FIX(sdef->radius);
        s->gravity    = FLT2FIX(sdef->gravity);
        s->type       = sdef->type;
        s->flags      = sdef->flags;
    }

    // Init some data.
    for(int i = 0; i < 3; ++i)
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
    for(int i = 0; i < gen->count; ++i)
    {
        particle_t* pt = &gen->ptcs[i];
        pt->stage = -1;
    }
}

static void P_PresimParticleGen(ptcgen_t *gen, int tics)
{
    for(; tics > 0; tics--)
    {
        P_PtcGenThinker(gen);
    }

    // Reset age so presim doesn't affect it.
    gen->age = 0;
}

void P_SpawnMobjParticleGen(ded_ptcgen_t const *def, mobj_t *source)
{
    if(isDedicated || !useParticles)return;

    /// @todo Do not assume the source mobj is from the CURRENT map.
    ptcgen_t *gen = P_NewGenerator();
    if(!gen) return;

    /*LOG_INFO("SpawnPtcGen: %s/%i (src:%s typ:%s mo:%p)")
        << def->state << (def - defs.ptcgens) << defs.states[source->state-states].id
        << defs.mobjs[source->type].id << source;*/

    // Initialize the particle generator.
    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & PGF_SCALED_RATE)
    {
        gen->spawnRateMultiplier = source->bspLeaf->sector().roughArea();
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

struct generatorbyplaneiterator_params_t
{
    Plane *plane;
    ptcgen_t *found;
};

static int generatorByPlaneIterator(ptcgen_t *gen, void *parameters)
{
    generatorbyplaneiterator_params_t *p = (generatorbyplaneiterator_params_t *)parameters;
    if(gen->plane == p->plane)
    {
        p->found = gen;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

static ptcgen_t *generatorByPlane(Plane *plane)
{
    /// @todo Do not assume plane is from the CURRENT map.
    Generators *gens = theMap->generators();
    generatorbyplaneiterator_params_t parm;
    parm.plane = plane;
    parm.found = NULL;
    Generators_Iterate(gens, generatorByPlaneIterator, (void *)&parm);
    return parm.found;
}

void P_SpawnPlaneParticleGen(ded_ptcgen_t const *def, Plane *plane)
{
    if(isDedicated || !useParticles) return;
    if(!def || !plane) return;
    // Only planes in sectors with volume on the world X/Y axis can support generators.
    if(!plane->sector().lineCount()) return;

    // Plane we spawn relative to may not be this one.
    Plane::Type relPlane = plane->type();
    if(def->flags & PGF_CEILING_SPAWN)
        relPlane = Plane::Ceiling;
    if(def->flags & PGF_FLOOR_SPAWN)
        relPlane = Plane::Floor;

    plane = &plane->sector().plane(relPlane);

    // Only one generator per plane.
    if(generatorByPlane(plane)) return;

    // Are we out of generators?
    ptcgen_t *gen = P_NewGenerator();
    if(!gen) return;

    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & PGF_PARTS_PER_128)
    {
        gen->spawnRateMultiplier = plane->sector().roughArea();
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    // Initialize the particle generator.
    P_InitParticleGen(gen, def);
    gen->plane = plane;

    // Is there a need to pre-simulate?
    P_PresimParticleGen(gen, def->preSim);
}

/**
 * The offset is spherical and random.
 * Low and High should be positive.
 */
static void P_Uncertain(fixed_t *pos, fixed_t low, fixed_t high)
{
    if(!low)
    {
        // The simple, cubic algorithm.
        for(int i = 0; i < 3; ++i)
        {
            pos[i] += (high * (RNG_RandByte() - RNG_RandByte())) * reciprocal255;
        }
    }
    else
    {
        // The more complicated, spherical algorithm.
        fixed_t off = ((high - low) * (RNG_RandByte() - RNG_RandByte())) * reciprocal255;
        off += off < 0 ? -low : low;

        fixed_t theta = RNG_RandByte() << (24 - ANGLETOFINESHIFT);
        fixed_t phi = acos(2 * (RNG_RandByte() * reciprocal255) - 1) / PI * (ANGLE_180 >> ANGLETOFINESHIFT);

        fixed_t vec[3];
        vec[VX] = FixedMul(fineCosine[theta], finesine[phi]);
        vec[VY] = FixedMul(finesine[theta], finesine[phi]);
        vec[VZ] = FixedMul(fineCosine[phi], FLT2FIX(0.8333f));

        for(int i = 0; i < 3; ++i)
        {
            pos[i] += FixedMul(vec[i], off);
        }
    }
}

static void P_SetParticleAngles(particle_t *pt, int flags)
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

static void P_ParticleSound(fixed_t pos[3], ded_embsound_t *sound)
{
    // Is there any sound to play?
    if(!sound->id || sound->volume <= 0) return;

    coord_t orig[3];
    for(int i = 0; i < 3; ++i)
    {
        orig[i] = FIX2FLT(pos[i]);
    }

    S_LocalSoundAtVolumeFrom(sound->id, NULL, orig, sound->volume);
}

/**
 * Spawns a new particle.
 */
static void P_NewParticle(ptcgen_t *gen)
{
#ifdef __SERVER__
    DENG_UNUSED(gen);
#else
    ded_ptcgen_t const *def = gen->def;
    particle_t *pt;
    int i;
    fixed_t uncertain, len;
    angle_t ang, ang2;
    BspLeaf *subsec;
    float inter = -1;
    modeldef_t *mf = 0, *nextmf = 0;

    // Check for model-only generators.
    if(gen->source)
    {
        inter = Models_ModelForMobj(gen->source, &mf, &nextmf);
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
    {
        pt->stage = def->altStart;
    }

    pt->tics = def->stages[pt->stage].tics *
        (1 - def->stages[pt->stage].variance * RNG_RandFloat());

    // Launch vector.
    pt->mov[VX] = gen->vector[VX];
    pt->mov[VY] = gen->vector[VY];
    pt->mov[VZ] = gen->vector[VZ];

    // Apply some random variance.
    pt->mov[VX] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pt->mov[VY] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pt->mov[VZ] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));

    // Apply some aspect ratio scaling to the momentum vector.
    // This counters the 200/240 difference nearly completely.
    pt->mov[VX] = FixedMul(pt->mov[VX], FLT2FIX(1.1f));
    pt->mov[VY] = FixedMul(pt->mov[VY], FLT2FIX(0.95f));
    pt->mov[VZ] = FixedMul(pt->mov[VZ], FLT2FIX(1.1f));

    // Set proper speed.
    uncertain = FLT2FIX(def->speed * (1 - def->speedVariance * RNG_RandFloat()));

    len = FLT2FIX(M_ApproxDistancef(
        M_ApproxDistancef(FIX2FLT(pt->mov[VX]), FIX2FLT(pt->mov[VY])), FIX2FLT(pt->mov[VZ])));
    if(!len) len = FRACUNIT;
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
            float temp[3];

            temp[VX] = FIX2FLT(pt->mov[VX]);
            temp[VY] = FIX2FLT(pt->mov[VY]);
            temp[VZ] = 0;

            // Player visangles have some problems, let's not use them.
            M_RotateVector(temp, gen->source->angle / (float) ANG180 * -180 + 90, 0);

            pt->mov[VX] = FLT2FIX(temp[VX]);
            pt->mov[VY] = FLT2FIX(temp[VY]);
        }

        if(gen->flags & PGF_RELATIVE_VELOCITY)
        {
            pt->mov[VX] += FLT2FIX(gen->source->mom[MX]);
            pt->mov[VY] += FLT2FIX(gen->source->mom[MY]);
            pt->mov[VZ] += FLT2FIX(gen->source->mom[MZ]);
        }

        // Origin.
        pt->origin[VX] = FLT2FIX(gen->source->origin[VX]);
        pt->origin[VY] = FLT2FIX(gen->source->origin[VY]);
        pt->origin[VZ] = FLT2FIX(gen->source->origin[VZ] - gen->source->floorClip);

        P_Uncertain(pt->origin, FLT2FIX(def->spawnRadiusMin), FLT2FIX(def->spawnRadius));

        // Offset to the real center.
        pt->origin[VZ] += gen->center[VZ];

        // Calculate XY center with mobj angle.
        ang = Mobj_AngleSmoothed(gen->source) + (fixed_t) (FIX2FLT(gen->center[VY]) / 180.0f * ANG180);
        ang2 = (ang + ANG90) >> ANGLETOFINESHIFT;
        ang >>= ANGLETOFINESHIFT;
        pt->origin[VX] += FixedMul(fineCosine[ang], gen->center[VX]);
        pt->origin[VY] += FixedMul(finesine[ang], gen->center[VX]);

        // There might be an offset from the model of the mobj.
        if(mf && (mf->sub[0].flags & MFF_PARTICLE_SUB1 || def->subModel >= 0))
        {
            float off[3] = { 0, 0, 0 };
            int subidx;

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
            pt->origin[VX] += FixedMul(fineCosine[ang], FLT2FIX(off[VX]));
            pt->origin[VX] += FixedMul(fineCosine[ang2], FLT2FIX(off[VZ]));
            pt->origin[VY] += FixedMul(finesine[ang], FLT2FIX(off[VX]));
            pt->origin[VY] += FixedMul(finesine[ang2], FLT2FIX(off[VZ]));
            pt->origin[VZ] += FLT2FIX(off[VY]);
        }
    }
    else if(gen->plane)
    {
        fixed_t radius = gen->stages[pt->stage].radius;
        Plane const *plane = gen->plane;
        Sector const *sector = &gen->plane->sector();

        // Choose a random spot inside the sector, on the spawn plane.
        if(gen->flags & PGF_SPACE_SPAWN)
        {
            pt->origin[VZ] =
                FLT2FIX(sector->floor().height()) + radius +
                FixedMul(RNG_RandByte() << 8,
                         FLT2FIX(sector->ceiling().height() -
                                 sector->floor().height()) - 2 * radius);
        }
        else if(gen->flags & PGF_FLOOR_SPAWN ||
                (!(gen->flags & (PGF_FLOOR_SPAWN | PGF_CEILING_SPAWN)) &&
                 plane->type() == Plane::Floor))
        {
            // Spawn on the floor.
            pt->origin[VZ] = FLT2FIX(plane->height()) + radius;
        }
        else
        {
            // Spawn on the ceiling.
            pt->origin[VZ] = FLT2FIX(plane->height()) - radius;
        }

        /**
         * Choosing the XY spot is a bit more difficult.
         * But we must be fast and only sufficiently accurate.
         *
         * @todo Nothing prevents spawning on the wrong side (or inside)
         * of one-sided walls (large diagonal BSP leafs!).
         */
        for(i = 0; i < 5; ++i) // Try a couple of times (max).
        {
            float x = sector->aaBox().minX +
                RNG_RandFloat() * (sector->aaBox().maxX - sector->aaBox().minX);
            float y = sector->aaBox().minY +
                RNG_RandFloat() * (sector->aaBox().maxY - sector->aaBox().minY);

            subsec = P_BspLeafAtPointXY(x, y);
            if(subsec->sectorPtr() == sector) break;

            subsec = NULL;
        }
        if(!subsec)
            goto spawn_failed;

        // Try a couple of times to get a good random spot.
        for(i = 0; i < 10; ++i) // Max this many tries before giving up.
        {
            float x = subsec->aaBox().minX +
                RNG_RandFloat() * (subsec->aaBox().maxX - subsec->aaBox().minX);
            float y = subsec->aaBox().minY +
                RNG_RandFloat() * (subsec->aaBox().maxY - subsec->aaBox().minY);

            pt->origin[VX] = FLT2FIX(x);
            pt->origin[VY] = FLT2FIX(y);

            if(P_BspLeafAtPointXY(x, y) == subsec)
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
        pt->origin[VX] = gen->center[VX];
        pt->origin[VY] = gen->center[VY];
        pt->origin[VZ] = gen->center[VZ];
        P_Uncertain(pt->origin, FLT2FIX(def->spawnRadiusMin),
                    FLT2FIX(def->spawnRadius));
    }

    // Initial angles for the particle.
    P_SetParticleAngles(pt, def->stages[pt->stage].flags);

    // The other place where this gets updated is after moving over
    // a two-sided line.
    if(gen->plane)
        pt->sector = &gen->plane->sector();
    else
        pt->sector = P_BspLeafAtPointXY(FIX2FLT(pt->origin[VX]),
                                        FIX2FLT(pt->origin[VY]))->sectorPtr();

    // Play a stage sound?
    P_ParticleSound(pt->origin, &def->stages[pt->stage].sound);

#endif // !__SERVER__
}

#ifdef __CLIENT__

/**
 * Callback for the client mobj iterator, called from P_PtcGenThinker.
 */
int PIT_ClientMobjParticles(mobj_t *cmo, void *context)
{
    ptcgen_t *gen = (ptcgen_t *) context;
    clmoinfo_t *info = ClMobj_GetInfo(cmo);

    // If the clmobj is not valid at the moment, don't do anything.
    if(info->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN))
    {
        return false;
    }

    if(cmo->type != gen->type && cmo->type != gen->type2)
    {
        // Type mismatch.
        return false;
    }

    gen->source = cmo;
    P_NewParticle(gen);
    return false;
}

#endif

/**
 * Spawn multiple new particles using all applicable sources.
 */
static int manyNewParticles(thinker_t *th, void *context)
{
    ptcgen_t *gen = (ptcgen_t *) context;
    mobj_t *mo = (mobj_t *) th;

    // Type match?
    if(gen->type == DED_PTCGEN_ANY_MOBJ_TYPE || mo->type == gen->type || mo->type == gen->type2)
    {
        // Someone might think this is a slight hack...
        gen->source = mo;
        P_NewParticle(gen);
    }

    return false; // Continue iteration.
}

int PIT_CheckLinePtc(Line *ld, void *parameters)
{
    DENG_ASSERT(ld);
    DENG_UNUSED(parameters);

    // Does the bounding box miss the line completely?
    if(mbox.maxX <= ld->aaBox().minX || mbox.minX >= ld->aaBox().maxX ||
       mbox.maxY <= ld->aaBox().minY || mbox.minY >= ld->aaBox().maxY)
        return false;

    // Movement must cross the line.
    if((ld->pointOnSide(FIX2FLT(tmpx1), FIX2FLT(tmpy1)) < 0) ==
       (ld->pointOnSide(FIX2FLT(tmpx2), FIX2FLT(tmpy2)) < 0))
        return false;

    /*
     * We are possibly hitting something here.
     */

    // Bounce if we hit a one-sided line.
    ptcHitLine = ld;
    if(!ld->hasBackSideDef()) return true; // Boing!

    Sector *front = ld->frontSectorPtr();
    Sector *back  = ld->backSectorPtr();

    // Determine the opening we have here.
    /// @todo Use R_OpenRange()
    fixed_t ceil;
    if(front->ceiling().height() < back->ceiling().height())
        ceil = FLT2FIX(front->ceiling().height());
    else
        ceil = FLT2FIX(back->ceiling().height());

    fixed_t floor;
    if(front->floor().height() > back->floor().height())
        floor = FLT2FIX(front->floor().height());
    else
        floor = FLT2FIX(back->floor().height());

    // There is a backsector. We possibly might hit something.
    if(tmpz - tmprad < floor || tmpz + tmprad > ceil)
        return true; // Boing!

    // There is a possibility that the new position is in a new sector.
    tmcross = true; // Afterwards, update the sector pointer.

    // False alarm, continue checking.
    return false;
}

/**
 * Particle touches something solid. Returns false iff the particle dies.
 */
static int P_TouchParticle(particle_t *pt, ptcstage_t *stage, ded_ptcstage_t *stageDef,
                           bool touchWall)
{
    // Play a hit sound.
    P_ParticleSound(pt->origin, &stageDef->hitSound);

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

float P_GetParticleRadius(ded_ptcstage_t const *def, int ptcIDX)
{
    static float const rnd[16] = { .875f, .125f, .3125f, .75f, .5f, .375f,
        .5625f, .0625f, 1, .6875f, .625f, .4375f, .8125f, .1875f,
        .9375f, .25f
    };

    if(!def->radiusVariance)
        return def->radius;

    return (rnd[ptcIDX & 0xf] * def->radiusVariance +
            (1 - def->radiusVariance)) * def->radius;
}

float P_GetParticleZ(particle_t const *pt)
{
    if(pt->origin[VZ] == DDMAXINT)
        return pt->sector->ceiling().visHeight() - 2;
    else if(pt->origin[VZ] == DDMININT)
        return (pt->sector->floor().visHeight() + 2);

    return FIX2FLT(pt->origin[VZ]);
}

static void P_SpinParticle(ptcgen_t *gen, particle_t *pt)
{
    static int const yawSigns[4]   = { 1,  1, -1, -1 };
    static int const pitchSigns[4] = { 1, -1,  1, -1 };

    Generators *gens = theMap->generators(); /// @todo Do not assume generator is from the CURRENT map.
    ded_ptcstage_t const *stDef = &gen->def->stages[pt->stage];
    uint const index = pt - &gen->ptcs[Generators_GeneratorId(gens, gen) / 8];

    int yawSign   =   yawSigns[index % 4];
    int pitchSign = pitchSigns[index % 4];

    if(stDef->spin[0] != 0)
    {
        pt->yaw   += 65536 * yawSign   * stDef->spin[0] / (360 * TICSPERSEC);
    }
    if(stDef->spin[1] != 0)
    {
        pt->pitch += 65536 * pitchSign * stDef->spin[1] / (360 * TICSPERSEC);
    }

    pt->yaw   *= 1 - stDef->spinResistance[0];
    pt->pitch *= 1 - stDef->spinResistance[1];
}

/**
 * The movement is done in two steps:
 * Z movement is done first. Skyflat kills the particle.
 * XY movement checks for hits with solid walls (no backsector).
 * This is supposed to be fast and simple (but not too simple).
 */
static void P_MoveParticle(ptcgen_t *gen, particle_t *pt)
{
    ptcstage_t *st = &gen->stages[pt->stage];
    ded_ptcstage_t *stDef = &gen->def->stages[pt->stage];
    bool zBounce = false, hitFloor = false;
    vec2d_t point;
    fixed_t x, y, z, hardRadius = st->radius / 2;

    // Particle rotates according to spin speed.
    P_SpinParticle(gen, pt);

    // Changes to momentum.
    /// @todo Do not assume generator is from the CURRENT map.
    pt->mov[VZ] -= FixedMul(FLT2FIX(theMap->gravity()), st->gravity);

    // Vector force.
    if(stDef->vectorForce[VX] != 0 || stDef->vectorForce[VY] != 0 ||
       stDef->vectorForce[VZ] != 0)
    {
        for(int i = 0; i < 3; ++i)
        {
            pt->mov[i] += FLT2FIX(stDef->vectorForce[i]);
        }
    }

    // Sphere force pull and turn.
    // Only applicable to sourced or untriggered generators. For other
    // types it's difficult to define the center coordinates.
    if((st->flags & PTCF_SPHERE_FORCE) &&
       (gen->source || gen->flags & PGF_UNTRIGGERED))
    {
        float delta[3];

        if(gen->source)
        {
            delta[VX] = FIX2FLT(pt->origin[VX]) - gen->source->origin[VX];
            delta[VY] = FIX2FLT(pt->origin[VY]) - gen->source->origin[VY];
            delta[VZ] = P_GetParticleZ(pt) - (gen->source->origin[VZ] +
                FIX2FLT(gen->center[VZ]));
        }
        else
        {
            for(int i = 0; i < 3; ++i)
            {
                delta[i] = FIX2FLT(pt->origin[i] - gen->center[i]);
            }
        }

        // Apply the offset (to source coords).
        for(int i = 0; i < 3; ++i)
        {
            delta[i] -= gen->def->forceOrigin[i];
        }

        // Counter the aspect ratio of old times.
        delta[VZ] *= 1.2f;

        float dist = M_ApproxDistancef(M_ApproxDistancef(delta[VX], delta[VY]), delta[VZ]);

        if(dist != 0)
        {
            // Radial force pushes the particles on the surface of a sphere.
            if(gen->def->force)
            {
                // Normalize delta vector, multiply with (dist - forceRadius),
                // multiply with radial force strength.
                for(int i = 0; i < 3; ++i)
                {
                    pt->mov[i] -= FLT2FIX(
                        ((delta[i] / dist) * (dist - gen->def->forceRadius)) * gen->def->force);
                }
            }

            // Rotate!
            if(gen->def->forceAxis[VX] || gen->def->forceAxis[VY] ||
               gen->def->forceAxis[VZ])
            {
                float cross[3];
                V3f_CrossProduct(cross, gen->def->forceAxis, delta);

                for(int i = 0; i < 3; ++i)
                {
                    pt->mov[i] += FLT2FIX(cross[i]) >> 8;
                }
            }
        }
    }

    if(st->resistance != FRACUNIT)
    {
        for(int i = 0; i < 3; ++i)
        {
            pt->mov[i] = FixedMul(pt->mov[i], st->resistance);
        }
    }

    // The particle is 'soft': half of radius is ignored.
    // The exception is plane flat particles, which are rendered flat
    // against planes. They are almost entirely soft when it comes to plane
    // collisions.
    if((st->type == PTC_POINT || (st->type >= PTC_TEXTURE && st->type < PTC_TEXTURE + MAX_PTC_TEXTURES)) &&
       (st->flags & PTCF_PLANE_FLAT))
        hardRadius = FRACUNIT;

    // Check the new Z position only if not stuck to a plane.
    z = pt->origin[VZ] + pt->mov[VZ];
    if(pt->origin[VZ] != DDMININT && pt->origin[VZ] != DDMAXINT && pt->sector)
    {
        if(z > FLT2FIX(pt->sector->ceiling().height()) - hardRadius)
        {
            // The Z is through the roof!
            if(pt->sector->ceilingSurface().hasSkyMaskedMaterial())
            {
                // Special case: particle gets lost in the sky.
                pt->stage = -1;
                return;
            }

            if(!P_TouchParticle(pt, st, stDef, false)) return;

            z = FLT2FIX(pt->sector->ceiling().height()) - hardRadius;
            zBounce = true;
            hitFloor = false;
        }

        // Also check the floor.
        if(z < FLT2FIX(pt->sector->floor().height()) + hardRadius)
        {
            if(pt->sector->floorSurface().hasSkyMaskedMaterial())
            {
                pt->stage = -1;
                return;
            }

            if(!P_TouchParticle(pt, st, stDef, false)) return;

            z = FLT2FIX(pt->sector->floor().height()) + hardRadius;
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
                if((st->type == PTC_POINT || (st->type >= PTC_TEXTURE && st->type < PTC_TEXTURE + MAX_PTC_TEXTURES)) &&
                   (st->flags & PTCF_PLANE_FLAT))
                {
                    z = hitFloor ? DDMININT : DDMAXINT;
                }
            }
        }

        // Move to the new Z coordinate.
        pt->origin[VZ] = z;
    }

    // Now check the XY direction.
    // - Check if the movement crosses any solid lines.
    // - If it does, quit when first one contacted and apply appropriate
    //   bounce (result depends on the angle of the contacted wall).
    x = pt->origin[VX] + pt->mov[VX];
    y = pt->origin[VY] + pt->mov[VY];

    tmcross = false; // Has crossed potential sector boundary?

    // XY movement can be skipped if the particle is not moving on the
    // XY plane.
    if(!pt->mov[VX] && !pt->mov[VY])
    {
        // If the particle is contacting a line, there is a chance that the
        // particle should be killed (if it's moving slowly at max).
        if(pt->contact)
        {
            Sector *front = (pt->contact->hasFrontSideDef()? pt->contact->frontSectorPtr() : NULL);
            Sector *back  = (pt->contact->hasBackSideDef()?  pt->contact->backSectorPtr()  : NULL);

            if(front && back && abs(pt->mov[VZ]) < FRACUNIT / 2)
            {
                coord_t pz = P_GetParticleZ(pt);
                coord_t fz, cz;

                /// @todo $nplanes
                if(front->floor().height() > back->floor().height())
                    fz = front->floor().height();
                else
                    fz = back->floor().height();

                if(front->ceiling().height() < back->ceiling().height())
                    cz = front->ceiling().height();
                else
                    cz = back->ceiling().height();

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
    tmpx1 = pt->origin[VX];
    tmpx2 = x;
    tmpy1 = pt->origin[VY];
    tmpy2 = y;
    V2d_Set(point, FIX2FLT(MIN_OF(x, pt->origin[VX]) - st->radius),
                   FIX2FLT(MIN_OF(y, pt->origin[VY]) - st->radius));
    V2d_InitBox(mbox.arvec2, point);
    V2d_Set(point, FIX2FLT(MAX_OF(x, pt->origin[VX]) + st->radius),
                   FIX2FLT(MAX_OF(y, pt->origin[VY]) + st->radius));
    V2d_AddToBox(mbox.arvec2, point);

    // Iterate the lines in the contacted blocks.

    validCount++;
    if(P_AllLinesBoxIterator(&mbox, PIT_CheckLinePtc, 0))
    {
        fixed_t normal[2], dotp;

        // Must survive the touch.
        if(!P_TouchParticle(pt, st, stDef, true)) return;

        // There was a hit! Calculate bounce vector.
        // - Project movement vector on the normal of hitline.
        // - Calculate the difference to the point on the normal.
        // - Add the difference to movement vector, negate movement.
        // - Multiply with bounce.

        // Calculate the normal.
        normal[VX] = -FLT2FIX(ptcHitLine->direction().x);
        normal[VY] = -FLT2FIX(ptcHitLine->direction().y);

        if(!normal[VX] && !normal[VY])
            goto quit_iteration;

        // Calculate as floating point so we don't overflow.
        dotp = FRACUNIT * (DOT2F(pt->mov, normal) / DOT2F(normal, normal));
        VECMUL(normal, dotp);
        VECSUB(normal, pt->mov);
        VECMULADD(pt->mov, 2 * FRACUNIT, normal);
        VECMUL(pt->mov, st->bounce);

        // Continue from the old position.
        x = pt->origin[VX];
        y = pt->origin[VY];
        tmcross = false; // Sector can't change if XY doesn't.

        // This line is the latest contacted line.
        pt->contact = ptcHitLine;
        goto quit_iteration;
    }

  quit_iteration:
    // The move is now OK.
    pt->origin[VX] = x;
    pt->origin[VY] = y;

    // Should we update the sector pointer?
    if(tmcross)
        pt->sector = P_BspLeafAtPointXY(FIX2FLT(x), FIX2FLT(y))->sectorPtr();
}

/**
 * Spawn and move particles.
 */
void P_PtcGenThinker(ptcgen_t *gen)
{
    DENG_ASSERT(gen);
    ded_ptcgen_t const *def = gen->def;

    // Source has been destroyed?
    if(!(gen->flags & PGF_UNTRIGGERED) && !GameMap_IsUsedMobjID(theMap, gen->srcid))
    {
        // Blasted... Spawning new particles becomes impossible.
        gen->source = NULL;
    }

    // Time to die?
    if(++gen->age > def->maxAge && def->maxAge >= 0)
    {
        destroyGenerator(gen, 0);
        return;
    }

    // Spawn new particles?
    float newparts = 0;
    if((gen->age <= def->spawnAge || def->spawnAge < 0) &&
       (gen->source || gen->plane || gen->type >= 0 || gen->type == DED_PTCGEN_ANY_MOBJ_TYPE ||
        (gen->flags & PGF_UNTRIGGERED)))
    {
        newparts = def->spawnRate * gen->spawnRateMultiplier;

        newparts *= particleSpawnRate *
            (1 - def->spawnRateVariance * RNG_RandFloat());

        gen->spawnCount += newparts;
        while(gen->spawnCount >= 1)
        {
            // Spawn a new particle.
            if(gen->type == DED_PTCGEN_ANY_MOBJ_TYPE || gen->type >= 0) // Type-triggered?
            {
#ifdef __CLIENT__
                // Client's should also check the client mobjs.
                if(isClient)
                {
                    theMap->clMobjIterator(PIT_ClientMobjParticles, gen);
                }
#endif
                GameMap_IterateThinkers(theMap, reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                        0x1 /*mobjs are public*/,
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
    particle_t *pt = gen->ptcs;
    for(int i = 0; i < gen->count; ++i, pt++)
    {
        if(pt->stage < 0) continue; // Not in use.

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

            pt->tics = def->stages[pt->stage].tics * (1 - def->stages[pt->stage].variance * RNG_RandFloat());

            // Change in particle angles?
            P_SetParticleAngles(pt, def->stages[pt->stage].flags);

            // Play a sound?
            P_ParticleSound(pt->origin, &def->stages[pt->stage].sound);
        }

        // Try to move.
        P_MoveParticle(gen, pt);
    }
}

void P_SpawnTypeParticleGens()
{
    if(isDedicated || !useParticles) return;

    ded_ptcgen_t *def = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, def++)
    {
        if(def->typeNum != DED_PTCGEN_ANY_MOBJ_TYPE && def->typeNum < 0) continue;

        ptcgen_t *gen = P_NewGenerator();
        if(!gen) return; // No more generators.

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

void P_SpawnMapParticleGens()
{
    if(isDedicated || !useParticles) return;
    if(!theMap) return;

    ded_ptcgen_t* def = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, def++)
    {
        if(!def->map) continue;

        de::Uri mapUri = theMap->uri();
        if(!Uri_Equality(def->map, reinterpret_cast<uri_s *>(&mapUri)))
            continue;

        // Are we still spawning using this generator?
        if(def->spawnAge > 0 && ddMapTime > def->spawnAge) continue;

        ptcgen_t *gen = P_NewGenerator();
        if(!gen) return; // No more generators.

        // Initialize the particle generator.
        gen->count = def->particles;
        gen->spawnRateMultiplier = 1;

        P_InitParticleGen(gen, def);
        gen->flags |= PGF_UNTRIGGERED;

        // Is there a need to pre-simulate?
        P_PresimParticleGen(gen, def->preSim);
    }
}

#undef P_SpawnDamageParticleGen
DENG_EXTERN_C void P_SpawnDamageParticleGen(mobj_t *mo, mobj_t *inflictor, int amount)
{
    // Are particles allowed?
    if(isDedicated || !useParticles) return;

    if(!mo || !inflictor || amount <= 0) return;

    ded_ptcgen_t const *def = Def_GetDamageGenerator(mo->type);
    if(def)
    {
        ptcgen_t* gen = P_NewGenerator();
        if(!gen) return; // No more generators.

        gen->count = def->particles;
        P_InitParticleGen(gen, def);

        gen->flags |= PGF_UNTRIGGERED;
        gen->spawnRateMultiplier = MAX_OF(amount, 1);

        // Calculate appropriate center coordinates.
        gen->center[VX] += FLT2FIX(mo->origin[VX]);
        gen->center[VY] += FLT2FIX(mo->origin[VY]);
        gen->center[VZ] += FLT2FIX(mo->origin[VZ] + mo->height / 2);

        // Calculate launch vector.
        vec3f_t vecDelta;
        V3f_Set(vecDelta, inflictor->origin[VX] - mo->origin[VX],
                inflictor->origin[VY] - mo->origin[VY],
                (inflictor->origin[VZ] - inflictor->height / 2) - (mo->origin[VZ] + mo->height / 2));

        vec3f_t vector;
        V3f_SetFixed(vector, gen->vector[VX], gen->vector[VY], gen->vector[VZ]);
        V3f_Sum(vector, vector, vecDelta);
        V3f_Normalize(vector);

        gen->vector[VX] = FLT2FIX(vector[VX]);
        gen->vector[VY] = FLT2FIX(vector[VY]);
        gen->vector[VZ] = FLT2FIX(vector[VZ]);

        // Is there a need to pre-simulate?
        P_PresimParticleGen(gen, def->preSim);
    }
}

static int findDefForGenerator(ptcgen_t *gen, void *parameters)
{
    DENG_UNUSED(parameters);

    // Search for a suitable definition.
    ded_ptcgen_t *def = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, def++)
    {
        // A type generator?
        if(def->typeNum == DED_PTCGEN_ANY_MOBJ_TYPE && gen->type == DED_PTCGEN_ANY_MOBJ_TYPE)
        {
            return i+1; // Stop iteration.
        }
        if(def->typeNum >= 0 &&
           (gen->type == def->typeNum || gen->type2 == def->type2Num))
        {
            return i+1; // Stop iteration.
        }

        // A damage generator?
        if(gen->source && gen->source->type == def->damageNum)
        {
            return i+1; // Stop iteration.
        }

        // A flat generator?
        if(gen->plane && def->material)
        {
            try
            {
                Material *defMat = &App_Materials().find(*reinterpret_cast<de::Uri const *>(def->material)).material();

                Material *mat = gen->plane->surface().materialPtr();
                if(def->flags & PGF_FLOOR_SPAWN)
                    mat = gen->plane->sector().floorSurface().materialPtr();
                if(def->flags & PGF_CEILING_SPAWN)
                    mat = gen->plane->sector().ceilingSurface().materialPtr();

                // Is this suitable?
                if(mat == defMat)
                {
                    return i + 1; // 1-based index.
                }

#if 0 /// @todo $revise-texture-animation
                if(def->flags & PGF_GROUP)
                {
                    // Generator triggered by all materials in the animation.
                    if(Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat) &&
                       &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
                    {
                        // Both are in this animation! This def will do.
                        return i + 1; // 1-based index.
                    }
                }
#endif
            }
            catch(MaterialManifest::MissingMaterialError const &)
            {} // Ignore this error.
            catch(Materials::NotFoundError const &)
            {} // Ignore this error.
        }

        // A state generator?
        if(gen->source && def->state[0] &&
           gen->source->state - states == Def_GetStateNum(def->state))
        {
            return i + 1; // 1-based index.
        }
    }

    return 0; // Not found.
}

static int updateGenerator(ptcgen_t *gen, void *parameters)
{
    DENG_ASSERT(gen);
    DENG_UNUSED(parameters);

    // Map generators cannot be updated (we have no means to reliably
    // identify them), so destroy them.
    if(gen->flags & PGF_UNTRIGGERED)
    {
        destroyGenerator(gen, 0);
        return false; // Continue iteration.
    }

    int defIndex = findDefForGenerator(gen, NULL/*no parameters*/);
    if(defIndex)
    {
        // Update the generator using the new definition.
        ded_ptcgen_t* def = defs.ptcGens + (defIndex-1);
        gen->def = def;
    }
    else
    {
        // Nothing else we can do, destroy it.
        destroyGenerator(gen, 0);
    }

    return false; // Continue iteration.
}

void P_UpdateParticleGens()
{
    if(!theMap) return;

    Generators *gens = theMap->generators();
    if(!gens) return;

    // Update existing generators.
    Generators_Iterate(gens, updateGenerator, NULL/*no parameters*/);

    // Re-spawn map generators.
    P_SpawnMapParticleGens();
}
