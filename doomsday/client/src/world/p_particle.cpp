/** @file p_particle.cpp  World map generator management (particles).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_platform.h"
#include "world/p_particle.h"

#include "de_base.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"
#include "clientapp.h"
#include "m_misc.h"

#include "Face"

#include "world/thinkers.h"
#include "BspLeaf"
#include "api_map.h"

#include "render/r_main.h" // validCount
#include "render/rend_model.h"

#include <de/String>
#include <de/Time>
#include <de/fixedpoint.h>
#include <de/memoryzone.h>
#include <de/timer.h>
#include <cmath>

using namespace de;

#define DOT2F(a,b)          ( FIX2FLT(a[VX]) * FIX2FLT(b[VX]) + FIX2FLT(a[VY]) * FIX2FLT(b[VY]) )
#define VECMUL(a,scalar)    ( a[VX] = FixedMul(a[VX], scalar), a[VY] = FixedMul(a[VY], scalar) )
#define VECADD(a,b)         ( a[VX] += b[VX], a[VY] += b[VY] )
#define VECMULADD(a,scal,b) ( a[VX] += FixedMul(scal, b[VX]), a[VY] += FixedMul(scal, b[VY]) )
#define VECSUB(a,b)         ( a[VX] -= b[VX], a[VY] -= b[VY] )
#define VECCPY(a,b)         ( a[VX] = b[VX], a[VY] = b[VY] )

byte useParticles = true;
int maxParticles = 0; // Unlimited.
float particleSpawnRate = 1; // Unmodified.

static AABoxd mbox;
static fixed_t tmpz, tmprad, tmpx1, tmpx2, tmpy1, tmpy2;
static dd_bool tmcross;
static Line *ptcHitLine;

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

void Generator_Delete(Generator *gen)
{
    if(!gen) return;
    gen->map().unlink(*gen);
    gen->map().thinkers().remove(gen->thinker);
    gen->clearParticles();
    // The generator itself is free'd when it's next turn for thinking comes.
}

Map &Generator::map() const
{
    return Thinker_Map(thinker);
}

Generator::Id Generator::id() const
{
    return _id;
}

void Generator::setId(Id newId)
{
    _id = newId;
}

Vector3d Generator::origin() const
{
    if(source)
    {
        Vector3d origin(source->origin);
        origin.z += -source->floorClip + FIX2FLT(center[VZ]);
        return origin;
    }

    return Vector3d(FIX2FLT(center[VX]), FIX2FLT(center[VY]), FIX2FLT(center[VZ]));
}

void Generator::clearParticles()
{
    Z_Free(_pinfo);
    _pinfo = 0;
}

void P_PtcInitForMap(Map &map)
{
    Time begunAt;

    LOG_AS("P_PtcInitForMap");

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    P_SpawnTypeParticleGens(map);
    P_SpawnMapParticleGens(map);

    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

void P_MapSpawnPlaneParticleGens(Map &map)
{
    if(isDedicated || !useParticles) return;

    foreach(Sector *sector, map.sectors())
    {
        Plane &floor = sector->floor();
        P_SpawnPlaneParticleGen(Def_GetGenerator(floor.surface().composeMaterialUri()), &floor);

        Plane &ceiling = sector->ceiling();
        P_SpawnPlaneParticleGen(Def_GetGenerator(ceiling.surface().composeMaterialUri()), &ceiling);
    }
}

void Generator::configureFromDef(ded_ptcgen_t const *newDef)
{
    DENG2_ASSERT(newDef != 0);

    if(count <= 0)
        count = 1;

    // Make sure no generator is type-triggered by default.
    type   = type2 = -1;

    def    = newDef;
    flags  = Flags(def->flags);
    _pinfo = (ParticleInfo *) Z_Calloc(sizeof(ParticleInfo) * count, PU_MAP, 0);
    stages = (ParticleStage *) Z_Calloc(sizeof(ParticleStage) * def->stageCount.num, PU_MAP, 0);

    for(int i = 0; i < def->stageCount.num; ++i)
    {
        ded_ptcstage_t const *sdef = &def->stages[i];
        ParticleStage *s = &stages[i];

        s->bounce     = FLT2FIX(sdef->bounce);
        s->resistance = FLT2FIX(1 - sdef->resistance);
        s->radius     = FLT2FIX(sdef->radius);
        s->gravity    = FLT2FIX(sdef->gravity);
        s->type       = sdef->type;
        s->flags      = ParticleStage::Flags(sdef->flags);
    }

    // Init some data.
    for(int i = 0; i < 3; ++i)
    {
        center[i] = FLT2FIX(def->center[i]);
        vector[i] = FLT2FIX(def->vector[i]);
    }

    // Apply a random component to the spawn vector.
    if(def->initVectorVariance > 0)
    {
        P_Uncertain(vector, 0, FLT2FIX(def->initVectorVariance));
    }

    // Mark unused.
    for(int i = 0; i < count; ++i)
    {
        ParticleInfo* pinfo = &_pinfo[i];
        pinfo->stage = -1;
    }
}

void Generator::presimulate(int tics)
{
    for(; tics > 0; tics--)
    {
        Generator_Thinker(this);
    }

    // Reset age so presim doesn't affect it.
    age = 0;
}

bool Generator::isStatic() const
{
    return flags.testFlag(Static);
}

blendmode_t Generator::blendmode() const
{
    if(flags.testFlag(BlendAdditive))        return BM_ADD;
    if(flags.testFlag(BlendSubtract))        return BM_SUBTRACT;
    if(flags.testFlag(BlendReverseSubtract)) return BM_REVERSE_SUBTRACT;
    if(flags.testFlag(BlendMultiply))        return BM_MUL;
    if(flags.testFlag(BlendInverseMultiply)) return BM_INVERSE_MUL;
    return BM_NORMAL;
}

ParticleInfo const *Generator::particleInfo() const
{
    return _pinfo;
}

void P_SpawnMobjParticleGen(ded_ptcgen_t const *def, mobj_t *source)
{
    DENG2_ASSERT(def != 0 && source != 0);

    if(isDedicated || !useParticles)return;

    Generator *gen = Mobj_Map(*source).newGenerator();
    if(!gen) return;

    /*LOG_INFO("SpawnPtcGen: %s/%i (src:%s typ:%s mo:%p)")
        << def->state << (def - defs.ptcgens) << defs.states[source->state-states].id
        << defs.mobjs[source->type].id << source;*/

    // Initialize the particle generator.
    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & Generator::ScaledRate)
    {
        gen->spawnRateMultiplier = Mobj_BspLeafAtOrigin(*source).sector().roughArea() / (128 * 128);
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    gen->configureFromDef(def);
    gen->source = source;
    gen->srcid = source->thinker.id;

    // Is there a need to pre-simulate?
    gen->presimulate(def->preSim);
}

void P_SpawnPlaneParticleGen(ded_ptcgen_t const *def, Plane *plane)
{
    if(isDedicated || !useParticles) return;
    if(!def || !plane) return;

    // Only planes in sectors with volume on the world X/Y axis can support generators.
    if(!plane->sector().sideCount()) return;

    // Plane we spawn relative to may not be this one.
    int relPlane = plane->indexInSector();
    if(def->flags & Generator::SpawnCeiling)
        relPlane = Sector::Ceiling;
    if(def->flags & Generator::SpawnFloor)
        relPlane = Sector::Floor;

    plane = &plane->sector().plane(relPlane);

    // Only one generator per plane.
    if(plane->hasGenerator()) return;

    // Are we out of generators?
    Generator *gen = plane->map().newGenerator();
    if(!gen) return;

    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & Generator::Density)
    {
        gen->spawnRateMultiplier = plane->sector().roughArea() / (128 * 128);
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    // Initialize the particle generator.
    gen->configureFromDef(def);
    gen->plane = plane;

    // Is there a need to pre-simulate?
    gen->presimulate(def->preSim);
}

static void P_SetParticleAngles(ParticleInfo *pinfo, int flags)
{
    DENG2_ASSERT(pinfo != 0);

    if(flags & Generator::ParticleStage::ZeroYaw)
        pinfo->yaw = 0;
    if(flags & Generator::ParticleStage::ZeroPitch)
        pinfo->pitch = 0;
    if(flags & Generator::ParticleStage::RandomYaw)
        pinfo->yaw = RNG_RandFloat() * 65536;
    if(flags & Generator::ParticleStage::RandomPitch)
        pinfo->pitch = RNG_RandFloat() * 65536;
}

static void P_ParticleSound(fixed_t pos[3], ded_embsound_t *sound)
{
    DENG2_ASSERT(pos != 0 && sound != 0);

    // Is there any sound to play?
    if(!sound->id || sound->volume <= 0) return;

    coord_t orig[3];
    for(int i = 0; i < 3; ++i)
    {
        orig[i] = FIX2FLT(pos[i]);
    }

    S_LocalSoundAtVolumeFrom(sound->id, NULL, orig, sound->volume);
}

ParticleInfo *Generator::newParticle()
{
#ifdef __CLIENT__
    // Check for model-only generators.
    float inter = -1;
    ModelDef *mf = 0, *nextmf = 0;
    if(source)
    {
        mf = Mobj_ModelDef(*source, &nextmf, &inter);
        if(((!mf || !useModels) && def->flags & ModelOnly) ||
           (mf && useModels && mf->flags & MFF_NO_PARTICLES))
            return 0;
    }

    // Keep the spawn cursor in the valid range.
    if(++spawnCP >= count)
    {
        spawnCP -= count;
    }

    // Set the particle's data.
    ParticleInfo *pinfo = _pinfo + spawnCP;
    pinfo->stage = 0;
    if(RNG_RandFloat() < def->altStartVariance)
    {
        pinfo->stage = def->altStart;
    }

    pinfo->tics = def->stages[pinfo->stage].tics *
        (1 - def->stages[pinfo->stage].variance * RNG_RandFloat());

    // Launch vector.
    pinfo->mov[VX] = vector[VX];
    pinfo->mov[VY] = vector[VY];
    pinfo->mov[VZ] = vector[VZ];

    // Apply some random variance.
    pinfo->mov[VX] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pinfo->mov[VY] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pinfo->mov[VZ] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));

    // Apply some aspect ratio scaling to the momentum vector.
    // This counters the 200/240 difference nearly completely.
    pinfo->mov[VX] = FixedMul(pinfo->mov[VX], FLT2FIX(1.1f));
    pinfo->mov[VY] = FixedMul(pinfo->mov[VY], FLT2FIX(0.95f));
    pinfo->mov[VZ] = FixedMul(pinfo->mov[VZ], FLT2FIX(1.1f));

    // Set proper speed.
    fixed_t uncertain = FLT2FIX(def->speed * (1 - def->speedVariance * RNG_RandFloat()));

    fixed_t len = FLT2FIX(M_ApproxDistancef(
        M_ApproxDistancef(FIX2FLT(pinfo->mov[VX]), FIX2FLT(pinfo->mov[VY])), FIX2FLT(pinfo->mov[VZ])));
    if(!len) len = FRACUNIT;
    len = FixedDiv(uncertain, len);

    pinfo->mov[VX] = FixedMul(pinfo->mov[VX], len);
    pinfo->mov[VY] = FixedMul(pinfo->mov[VY], len);
    pinfo->mov[VZ] = FixedMul(pinfo->mov[VZ], len);

    // The source is a mobj?
    if(source)
    {
        if(flags & RelativeVector)
        {
            // Rotate the vector using the source angle.
            float temp[3];

            temp[VX] = FIX2FLT(pinfo->mov[VX]);
            temp[VY] = FIX2FLT(pinfo->mov[VY]);
            temp[VZ] = 0;

            // Player visangles have some problems, let's not use them.
            M_RotateVector(temp, source->angle / (float) ANG180 * -180 + 90, 0);

            pinfo->mov[VX] = FLT2FIX(temp[VX]);
            pinfo->mov[VY] = FLT2FIX(temp[VY]);
        }

        if(flags & RelativeVelocity)
        {
            pinfo->mov[VX] += FLT2FIX(source->mom[MX]);
            pinfo->mov[VY] += FLT2FIX(source->mom[MY]);
            pinfo->mov[VZ] += FLT2FIX(source->mom[MZ]);
        }

        // Origin.
        pinfo->origin[VX] = FLT2FIX(source->origin[VX]);
        pinfo->origin[VY] = FLT2FIX(source->origin[VY]);
        pinfo->origin[VZ] = FLT2FIX(source->origin[VZ] - source->floorClip);

        P_Uncertain(pinfo->origin, FLT2FIX(def->spawnRadiusMin), FLT2FIX(def->spawnRadius));

        // Offset to the real center.
        pinfo->origin[VZ] += center[VZ];

        // Include bobbing in the spawn height.
        pinfo->origin[VZ] -= FLT2FIX(Mobj_BobOffset(source));

        // Calculate XY center with mobj angle.
        angle_t const angle = Mobj_AngleSmoothed(source) + (fixed_t) (FIX2FLT(center[VY]) / 180.0f * ANG180);
        uint const an       = angle >> ANGLETOFINESHIFT;
        uint const an2      = (angle + ANG90) >> ANGLETOFINESHIFT;

        pinfo->origin[VX] += FixedMul(fineCosine[an], center[VX]);
        pinfo->origin[VY] += FixedMul(finesine[an], center[VX]);

        // There might be an offset from the model of the mobj.
        if(mf && (mf->testSubFlag(0, MFF_PARTICLE_SUB1) || def->subModel >= 0))
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
                off[VX] = nextmf->particleOffset(subidx)[VX] - mf->particleOffset(subidx)[VX];
                off[VY] = nextmf->particleOffset(subidx)[VY] - mf->particleOffset(subidx)[VY];
                off[VZ] = nextmf->particleOffset(subidx)[VZ] - mf->particleOffset(subidx)[VZ];

                off[VX] *= inter;
                off[VY] *= inter;
                off[VZ] *= inter;
            }

            off[VX] += mf->particleOffset(subidx)[VX];
            off[VY] += mf->particleOffset(subidx)[VY];
            off[VZ] += mf->particleOffset(subidx)[VZ];

            // Apply it to the particle coords.
            pinfo->origin[VX] += FixedMul(fineCosine[an],  FLT2FIX(off[VX]));
            pinfo->origin[VX] += FixedMul(fineCosine[an2], FLT2FIX(off[VZ]));
            pinfo->origin[VY] += FixedMul(finesine[an],    FLT2FIX(off[VX]));
            pinfo->origin[VY] += FixedMul(finesine[an2],   FLT2FIX(off[VZ]));
            pinfo->origin[VZ] += FLT2FIX(off[VY]);
        }
    }
    else if(plane)
    {
        /// @todo fixme: ignorant of mapped sector planes.
        fixed_t radius = stages[pinfo->stage].radius;
        Sector const *sector = &plane->sector();

        // Choose a random spot inside the sector, on the spawn plane.
        if(flags & SpawnSpace)
        {
            pinfo->origin[VZ] =
                FLT2FIX(sector->floor().height()) + radius +
                FixedMul(RNG_RandByte() << 8,
                         FLT2FIX(sector->ceiling().height() -
                                 sector->floor().height()) - 2 * radius);
        }
        else if(flags & SpawnFloor ||
                (!(flags & (SpawnFloor | SpawnCeiling)) &&
                 plane->isSectorFloor()))
        {
            // Spawn on the floor.
            pinfo->origin[VZ] = FLT2FIX(plane->height()) + radius;
        }
        else
        {
            // Spawn on the ceiling.
            pinfo->origin[VZ] = FLT2FIX(plane->height()) - radius;
        }

        /**
         * Choosing the XY spot is a bit more difficult.
         * But we must be fast and only sufficiently accurate.
         *
         * @todo Nothing prevents spawning on the wrong side (or inside)
         * of one-sided walls (large diagonal BSP leafs!).
         */
        BspLeaf *bspLeaf;
        for(int i = 0; i < 5; ++i) // Try a couple of times (max).
        {
            float x = sector->aaBox().minX +
                RNG_RandFloat() * (sector->aaBox().maxX - sector->aaBox().minX);
            float y = sector->aaBox().minY +
                RNG_RandFloat() * (sector->aaBox().maxY - sector->aaBox().minY);

            bspLeaf = &map().bspLeafAt(Vector2d(x, y));
            if(bspLeaf->sectorPtr() == sector) break;

            bspLeaf = 0;
        }

        if(!bspLeaf || !bspLeaf->hasPoly())
        {
            pinfo->stage = -1;
            return 0;
        }

        AABoxd const &leafAABox = bspLeaf->poly().aaBox();

        // Try a couple of times to get a good random spot.
        int tries;
        for(tries = 0; tries < 10; ++tries) // Max this many tries before giving up.
        {
            float x = leafAABox.minX +
                RNG_RandFloat() * (leafAABox.maxX - leafAABox.minX);
            float y = leafAABox.minY +
                RNG_RandFloat() * (leafAABox.maxY - leafAABox.minY);

            pinfo->origin[VX] = FLT2FIX(x);
            pinfo->origin[VY] = FLT2FIX(y);

            if(&map().bspLeafAt(Vector2d(x, y)) == bspLeaf)
                break; // This is a good place.
        }

        if(tries == 10) // No good place found?
        {
            pinfo->stage = -1; // Damn.
            return 0;
        }
    }
    else if(flags & Untriggered)
    {
        // The center position is the spawn origin.
        pinfo->origin[VX] = center[VX];
        pinfo->origin[VY] = center[VY];
        pinfo->origin[VZ] = center[VZ];
        P_Uncertain(pinfo->origin, FLT2FIX(def->spawnRadiusMin),
                    FLT2FIX(def->spawnRadius));
    }

    // Initial angles for the particle.
    P_SetParticleAngles(pinfo, def->stages[pinfo->stage].flags);

    // The other place where this gets updated is after moving over
    // a two-sided line.
    /*if(plane)
    {
        pinfo->sector = &plane->sector();
    }
    else*/
    {
        Vector2d ptOrigin(FIX2FLT(pinfo->origin[VX]), FIX2FLT(pinfo->origin[VY]));
        pinfo->bspLeaf = &map().bspLeafAt(ptOrigin);

        // A BSP leaf with no geometry is not a suitable place for a particle.
        if(!pinfo->bspLeaf->hasPoly())
        {
            pinfo->stage = -1;
            return 0;
        }
    }

    // Play a stage sound?
    P_ParticleSound(pinfo->origin, &def->stages[pinfo->stage].sound);

    return pinfo;
#else  // !__CLIENT__
    return 0;
#endif
}

#ifdef __CLIENT__

/**
 * Callback for the client mobj iterator, called from P_PtcGenThinker.
 */
static int newGeneratorParticlesWorker(mobj_t *cmo, void *context)
{
    Generator *gen = (Generator *) context;
    ClMobjInfo *info = ClMobj_GetInfo(cmo);

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
    gen->newParticle();
    return false;
}

#endif

/**
 * Spawn multiple new particles using all applicable sources.
 */
static int manyNewParticlesWorker(thinker_t *th, void *context)
{
    Generator *gen = (Generator *) context;
    mobj_t *mo = (mobj_t *) th;

    // Type match?
    if(gen->type == DED_PTCGEN_ANY_MOBJ_TYPE || mo->type == gen->type || mo->type == gen->type2)
    {
        // Someone might think this is a slight hack...
        gen->source = mo;
        gen->newParticle();
    }

    return false; // Continue iteration.
}

static int checkLineWorker(Line *ld, void * /*context*/)
{
    DENG2_ASSERT(ld != 0);

    // Does the bounding box miss the line completely?
    if(mbox.maxX <= ld->aaBox().minX || mbox.minX >= ld->aaBox().maxX ||
       mbox.maxY <= ld->aaBox().minY || mbox.minY >= ld->aaBox().maxY)
        return false;

    // Movement must cross the line.
    if((ld->pointOnSide(Vector2d(FIX2FLT(tmpx1), FIX2FLT(tmpy1))) < 0) ==
       (ld->pointOnSide(Vector2d(FIX2FLT(tmpx2), FIX2FLT(tmpy2))) < 0))
        return false;

    /*
     * We are possibly hitting something here.
     */

    // Bounce if we hit a solid wall.
    /// @todo fixme: What about "one-way" window lines?
    ptcHitLine = ld;
    if(!ld->hasBackSector()) return true; // Boing!

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
static int P_TouchParticle(ParticleInfo *pinfo, Generator::ParticleStage *stage,
    ded_ptcstage_t *stageDef, bool touchWall)
{
    // Play a hit sound.
    P_ParticleSound(pinfo->origin, &stageDef->hitSound);

    if(stage->flags.testFlag(Generator::ParticleStage::DieTouch))
    {
        // Particle dies from touch.
        pinfo->stage = -1;
        return false;
    }

    if(stage->flags.testFlag(Generator::ParticleStage::StageTouch) ||
       (touchWall && stage->flags.testFlag(Generator::ParticleStage::StageWallTouch)) ||
       (!touchWall && stage->flags.testFlag(Generator::ParticleStage::StageFlatTouch)))
    {
        // Particle advances to the next stage.
        pinfo->tics = 0;
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

float Generator::particleZ(ParticleInfo const *pinfo) const
{
    DENG2_ASSERT(pinfo != 0);

    SectorCluster &cluster = pinfo->bspLeaf->cluster();
    if(pinfo->origin[VZ] == DDMAXINT)
        return cluster.visCeiling().heightSmoothed() - 2;
    else if(pinfo->origin[VZ] == DDMININT)
        return (cluster.visFloor().heightSmoothed() + 2);

    return FIX2FLT(pinfo->origin[VZ]);
}

void Generator::spinParticle(ParticleInfo *pinfo)
{
    static int const yawSigns[4]   = { 1,  1, -1, -1 };
    static int const pitchSigns[4] = { 1, -1,  1, -1 };

    ded_ptcstage_t const *stDef = &def->stages[pinfo->stage];
    uint const index = pinfo - &_pinfo[id() / 8];

    int yawSign   =   yawSigns[index % 4];
    int pitchSign = pitchSigns[index % 4];

    if(stDef->spin[0] != 0)
    {
        pinfo->yaw   += 65536 * yawSign   * stDef->spin[0] / (360 * TICSPERSEC);
    }
    if(stDef->spin[1] != 0)
    {
        pinfo->pitch += 65536 * pitchSign * stDef->spin[1] / (360 * TICSPERSEC);
    }

    pinfo->yaw   *= 1 - stDef->spinResistance[0];
    pinfo->pitch *= 1 - stDef->spinResistance[1];
}

void Generator::moveParticle(ParticleInfo *pinfo)
{
    DENG2_ASSERT(pinfo != 0);

    ParticleStage *st = &stages[pinfo->stage];
    ded_ptcstage_t *stDef = &def->stages[pinfo->stage];
    bool zBounce = false, hitFloor = false;
    vec2d_t point;
    fixed_t x, y, z, hardRadius = st->radius / 2;

    // Particle rotates according to spin speed.
    spinParticle(pinfo);

    // Changes to momentum.
    /// @todo Do not assume generator is from the CURRENT map.
    pinfo->mov[VZ] -= FixedMul(FLT2FIX(map().gravity()), st->gravity);

    // Vector force.
    if(stDef->vectorForce[VX] != 0 || stDef->vectorForce[VY] != 0 ||
       stDef->vectorForce[VZ] != 0)
    {
        for(int i = 0; i < 3; ++i)
        {
            pinfo->mov[i] += FLT2FIX(stDef->vectorForce[i]);
        }
    }

    // Sphere force pull and turn.
    // Only applicable to sourced or untriggered generators. For other
    // types it's difficult to define the center coordinates.
    if(st->flags.testFlag(ParticleStage::SphereForce) &&
       (source || flags & Untriggered))
    {
        float delta[3];

        if(source)
        {
            delta[VX] = FIX2FLT(pinfo->origin[VX]) - source->origin[VX];
            delta[VY] = FIX2FLT(pinfo->origin[VY]) - source->origin[VY];
            delta[VZ] = particleZ(pinfo) - (source->origin[VZ] + FIX2FLT(center[VZ]));
        }
        else
        {
            for(int i = 0; i < 3; ++i)
            {
                delta[i] = FIX2FLT(pinfo->origin[i] - center[i]);
            }
        }

        // Apply the offset (to source coords).
        for(int i = 0; i < 3; ++i)
        {
            delta[i] -= def->forceOrigin[i];
        }

        // Counter the aspect ratio of old times.
        delta[VZ] *= 1.2f;

        float dist = M_ApproxDistancef(M_ApproxDistancef(delta[VX], delta[VY]), delta[VZ]);

        if(dist != 0)
        {
            // Radial force pushes the particles on the surface of a sphere.
            if(def->force)
            {
                // Normalize delta vector, multiply with (dist - forceRadius),
                // multiply with radial force strength.
                for(int i = 0; i < 3; ++i)
                {
                    pinfo->mov[i] -= FLT2FIX(
                        ((delta[i] / dist) * (dist - def->forceRadius)) * def->force);
                }
            }

            // Rotate!
            if(def->forceAxis[VX] || def->forceAxis[VY] || def->forceAxis[VZ])
            {
                float cross[3];
                V3f_CrossProduct(cross, def->forceAxis, delta);

                for(int i = 0; i < 3; ++i)
                {
                    pinfo->mov[i] += FLT2FIX(cross[i]) >> 8;
                }
            }
        }
    }

    if(st->resistance != FRACUNIT)
    {
        for(int i = 0; i < 3; ++i)
        {
            pinfo->mov[i] = FixedMul(pinfo->mov[i], st->resistance);
        }
    }

    // The particle is 'soft': half of radius is ignored.
    // The exception is plane flat particles, which are rendered flat
    // against planes. They are almost entirely soft when it comes to plane
    // collisions.
    if((st->type == PTC_POINT || (st->type >= PTC_TEXTURE && st->type < PTC_TEXTURE + MAX_PTC_TEXTURES)) &&
       st->flags.testFlag(ParticleStage::PlaneFlat))
    {
        hardRadius = FRACUNIT;
    }

    // Check the new Z position only if not stuck to a plane.
    z = pinfo->origin[VZ] + pinfo->mov[VZ];
    if(pinfo->origin[VZ] != DDMININT && pinfo->origin[VZ] != DDMAXINT && pinfo->bspLeaf)
    {
        SectorCluster &cluster = pinfo->bspLeaf->cluster();
        if(z > FLT2FIX(cluster.visCeiling().heightSmoothed()) - hardRadius)
        {
            // The Z is through the roof!
            if(cluster.visCeiling().surface().hasSkyMaskedMaterial())
            {
                // Special case: particle gets lost in the sky.
                pinfo->stage = -1;
                return;
            }

            if(!P_TouchParticle(pinfo, st, stDef, false))
                return;

            z = FLT2FIX(cluster.visCeiling().heightSmoothed()) - hardRadius;
            zBounce = true;
            hitFloor = false;
        }

        // Also check the floor.
        if(z < FLT2FIX(cluster.visFloor().heightSmoothed()) + hardRadius)
        {
            if(cluster.visFloor().surface().hasSkyMaskedMaterial())
            {
                pinfo->stage = -1;
                return;
            }

            if(!P_TouchParticle(pinfo, st, stDef, false))
                return;

            z = FLT2FIX(cluster.visFloor().heightSmoothed()) + hardRadius;
            zBounce = true;
            hitFloor = true;
        }

        if(zBounce)
        {
            pinfo->mov[VZ] = FixedMul(-pinfo->mov[VZ], st->bounce);
            if(!pinfo->mov[VZ])
            {
                // The particle has stopped moving. This means its Z-movement
                // has ceased because of the collision with a plane. Plane-flat
                // particles will stick to the plane.
                if((st->type == PTC_POINT || (st->type >= PTC_TEXTURE && st->type < PTC_TEXTURE + MAX_PTC_TEXTURES)) &&
                   st->flags.testFlag(ParticleStage::PlaneFlat))
                {
                    z = hitFloor ? DDMININT : DDMAXINT;
                }
            }
        }

        // Move to the new Z coordinate.
        pinfo->origin[VZ] = z;
    }

    // Now check the XY direction.
    // - Check if the movement crosses any solid lines.
    // - If it does, quit when first one contacted and apply appropriate
    //   bounce (result depends on the angle of the contacted wall).
    x = pinfo->origin[VX] + pinfo->mov[VX];
    y = pinfo->origin[VY] + pinfo->mov[VY];

    tmcross = false; // Has crossed potential sector boundary?

    // XY movement can be skipped if the particle is not moving on the
    // XY plane.
    if(!pinfo->mov[VX] && !pinfo->mov[VY])
    {
        // If the particle is contacting a line, there is a chance that the
        // particle should be killed (if it's moving slowly at max).
        if(pinfo->contact)
        {
            Sector *front = pinfo->contact->frontSectorPtr();
            Sector *back  = pinfo->contact->backSectorPtr();

            if(front && back && abs(pinfo->mov[VZ]) < FRACUNIT / 2)
            {
                coord_t pz = particleZ(pinfo);
                coord_t fz, cz;

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
                    pinfo->stage = -1;
                    return;
                }
            }
        }

        // Still not moving on the XY plane...
        goto quit_iteration;
    }

    // We're moving in XY, so if we don't hit anything there can't be any
    // line contact.
    pinfo->contact = NULL;

    // Bounding box of the movement line.
    tmpz = z;
    tmprad = hardRadius;
    tmpx1 = pinfo->origin[VX];
    tmpx2 = x;
    tmpy1 = pinfo->origin[VY];
    tmpy2 = y;
    V2d_Set(point, FIX2FLT(MIN_OF(x, pinfo->origin[VX]) - st->radius),
                   FIX2FLT(MIN_OF(y, pinfo->origin[VY]) - st->radius));
    V2d_InitBox(mbox.arvec2, point);
    V2d_Set(point, FIX2FLT(MAX_OF(x, pinfo->origin[VX]) + st->radius),
                   FIX2FLT(MAX_OF(y, pinfo->origin[VY]) + st->radius));
    V2d_AddToBox(mbox.arvec2, point);

    // Iterate the lines in the contacted blocks.

    validCount++;
    if(map().lineBoxIterator(mbox, checkLineWorker))
    {
        fixed_t normal[2], dotp;

        // Must survive the touch.
        if(!P_TouchParticle(pinfo, st, stDef, true))
            return;

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
        dotp = FRACUNIT * (DOT2F(pinfo->mov, normal) / DOT2F(normal, normal));
        VECMUL(normal, dotp);
        VECSUB(normal, pinfo->mov);
        VECMULADD(pinfo->mov, 2 * FRACUNIT, normal);
        VECMUL(pinfo->mov, st->bounce);

        // Continue from the old position.
        x = pinfo->origin[VX];
        y = pinfo->origin[VY];
        tmcross = false; // Sector can't change if XY doesn't.

        // This line is the latest contacted line.
        pinfo->contact = ptcHitLine;
        goto quit_iteration;
    }

  quit_iteration:
    // The move is now OK.
    pinfo->origin[VX] = x;
    pinfo->origin[VY] = y;

    // Should we update the sector pointer?
    if(tmcross)
    {
        pinfo->bspLeaf = &map().bspLeafAt(Vector2d(FIX2FLT(x), FIX2FLT(y)));

        // A BSP leaf with no geometry is not a suitable place for a particle.
        if(!pinfo->bspLeaf->hasPoly())
        {
            // Kill the particle.
            pinfo->stage = -1;
        }
    }
}

void Generator::runTick()
{
    // Source has been destroyed?
    if(!(flags & Generator::Untriggered) && !map().thinkers().isUsedMobjId(srcid))
    {
        // Blasted... Spawning new particles becomes impossible.
        source = 0;
    }

    // Time to die?
    DENG2_ASSERT(def != 0);
    if(++age > def->maxAge && def->maxAge >= 0)
    {
        Generator_Delete(this);
        return;
    }

    // Spawn new particles?
    float newParts = 0;
    if((age <= def->spawnAge || def->spawnAge < 0) &&
       (source || plane || type >= 0 || type == DED_PTCGEN_ANY_MOBJ_TYPE ||
        (flags & Generator::Untriggered)))
    {
        newParts = def->spawnRate * spawnRateMultiplier;

        newParts *= particleSpawnRate *
            (1 - def->spawnRateVariance * RNG_RandFloat());

        spawnCount += newParts;
        while(spawnCount >= 1)
        {
            // Spawn a new particle.
            if(type == DED_PTCGEN_ANY_MOBJ_TYPE || type >= 0) // Type-triggered?
            {
#ifdef __CLIENT__
                // Client's should also check the client mobjs.
                if(isClient)
                {
                    map().clMobjIterator(newGeneratorParticlesWorker, this);
                }
#endif
                map().thinkers()
                    .iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1 /*mobjs are public*/,
                             manyNewParticlesWorker, this);

                // The generator has no real source.
                source = 0;
            }
            else
            {
                newParticle();
            }

            spawnCount--;
        }
    }

    // Move particles.
    ParticleInfo *pinfo = _pinfo;
    for(int i = 0; i < count; ++i, pinfo++)
    {
        if(pinfo->stage < 0) continue; // Not in use.

        if(pinfo->tics-- <= 0)
        {
            // Advance to next stage.
            if(++pinfo->stage == def->stageCount.num ||
               stages[pinfo->stage].type == PTC_NONE)
            {
                // Kill the particle.
                pinfo->stage = -1;
                continue;
            }

            pinfo->tics = def->stages[pinfo->stage].tics * (1 - def->stages[pinfo->stage].variance * RNG_RandFloat());

            // Change in particle angles?
            P_SetParticleAngles(pinfo, def->stages[pinfo->stage].flags);

            // Play a sound?
            P_ParticleSound(pinfo->origin, &def->stages[pinfo->stage].sound);
        }

        // Try to move.
        moveParticle(pinfo);
    }
}

void Generator_Thinker(Generator *gen)
{
    DENG2_ASSERT(gen != 0);
    gen->runTick();
}

void P_SpawnTypeParticleGens(Map &map)
{
    if(isDedicated || !useParticles) return;

    ded_ptcgen_t *def = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, def++)
    {
        if(def->typeNum != DED_PTCGEN_ANY_MOBJ_TYPE && def->typeNum < 0)
            continue;

        Generator *gen = map.newGenerator();
        if(!gen) return; // No more generators.

        // Initialize the particle generator.
        gen->count = def->particles;
        gen->spawnRateMultiplier = 1;

        gen->configureFromDef(def);
        gen->type = def->typeNum;
        gen->type2 = def->type2Num;

        // Is there a need to pre-simulate?
        gen->presimulate(def->preSim);
    }
}

void P_SpawnMapParticleGens(Map &map)
{
    if(isDedicated || !useParticles) return;

    ded_ptcgen_t *def = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, def++)
    {
        if(!def->map) continue;

        de::Uri mapUri = map.uri();
        if(!Uri_Equality(def->map, reinterpret_cast<uri_s *>(&mapUri)))
            continue;

        // Are we still spawning using this generator?
        if(def->spawnAge > 0 && App_WorldSystem().time() > def->spawnAge)
            continue;

        Generator *gen = map.newGenerator();
        if(!gen) return; // No more generators.

        // Initialize the particle generator.
        gen->count = def->particles;
        gen->spawnRateMultiplier = 1;

        gen->configureFromDef(def);
        gen->flags |= Generator::Untriggered;

        // Is there a need to pre-simulate?
        gen->presimulate(def->preSim);
    }
}

void P_SpawnMapDamageParticleGen(mobj_t *mo, mobj_t *inflictor, int amount)
{
    // Are particles allowed?
    if(isDedicated || !useParticles) return;

    if(!mo || !inflictor || amount <= 0) return;

    ded_ptcgen_t const *def = Def_GetDamageGenerator(mo->type);
    if(def)
    {
        Generator *gen = Mobj_Map(*mo).newGenerator();
        if(!gen) return; // No more generators.

        gen->count = def->particles;
        gen->configureFromDef(def);
        gen->flags |= Generator::Untriggered;

        gen->spawnRateMultiplier = de::max(amount, 1);

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
        gen->presimulate(def->preSim);
    }
}

static int findDefForGeneratorWorker(Generator *gen, void * /*context*/)
{
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
                Material *defMat = &ClientApp::resourceSystem().material(*reinterpret_cast<de::Uri const *>(def->material));

                Material *mat = gen->plane->surface().materialPtr();
                if(def->flags & Generator::SpawnFloor)
                    mat = gen->plane->sector().floorSurface().materialPtr();
                if(def->flags & Generator::SpawnCeiling)
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
            catch(ResourceSystem::MissingManifestError const &)
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

static int updateGeneratorWorker(Generator *gen, void * /*context*/)
{
    DENG2_ASSERT(gen != 0);

    // Map generators cannot be updated (we have no means to reliably
    // identify them), so destroy them.
    if(gen->flags & Generator::Untriggered)
    {
        Generator_Delete(gen);
        return false; // Continue iteration.
    }

    if(int defIndex = findDefForGeneratorWorker(gen, 0/*no params*/))
    {
        // Update the generator using the new definition.
        ded_ptcgen_t *def = defs.ptcGens + (defIndex-1);
        gen->def = def;
    }
    else
    {
        // Nothing else we can do, destroy it.
        Generator_Delete(gen);
    }

    return false; // Continue iteration.
}

void P_UpdateParticleGens(Map &map)
{
    map.generatorIterator(updateGeneratorWorker);

    // Re-spawn map generators.
    P_SpawnMapParticleGens(map);
}
