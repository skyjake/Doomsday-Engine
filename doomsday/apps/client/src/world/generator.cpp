/** @file generator.cpp  World map (particle) generator.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "world/generator.h"
#include "world/subsector.h"
#include "client/cl_mobj.h"
#include "world/convexsubspace.h"
#include "world/surface.h"
#include "render/rend_model.h"
#include "render/rend_particle.h"
#include "network/net_main.h"
#include "api_sound.h"
#include "dd_def.h"
#include "clientapp.h"

#include <doomsday/console/var.h>
#include <doomsday/mesh/face.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/thinkers.h>
#include <doomsday/tab_tables.h>
#include <de/string.h>
#include <de/time.h>
#include <de/legacy/fixedpoint.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/timer.h>
#include <de/legacy/vector1.h>
#include <cmath>

using namespace de;
using world::World;

#define DOT2F(a,b)          ( FIX2FLT(a[0]) * FIX2FLT(b[0]) + FIX2FLT(a[1]) * FIX2FLT(b[1]) )
#define VECMUL(a,scalar)    ( a[0] = FixedMul(a[0], scalar), a[1] = FixedMul(a[1], scalar) )
#define VECADD(a,b)         ( a[0] += b[0], a[1] += b[1] )
#define VECMULADD(a,scal,b) ( a[0] += FixedMul(scal, b[0]), a[1] += FixedMul(scal, b[1]) )
#define VECSUB(a,b)         ( a[0] -= b[0], a[1] -= b[1] )
#define VECCPY(a,b)         ( a[0] = b[0], a[1] = b[1] )

static float particleSpawnRate = 1; // Unmodified (cvar).

/**
 * The offset is spherical and random.
 * Low and High should be positive.
 */
static void uncertainPosition(fixed_t *pos, fixed_t low, fixed_t high)
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
        vec[0] = FixedMul(finecosine[theta], finesine[phi]);
        vec[1] = FixedMul(finesine[theta], finesine[phi]);
        vec[2] = FixedMul(finecosine[phi], FLT2FIX(0.8333f));

        for(int i = 0; i < 3; ++i)
        {
            pos[i] += FixedMul(vec[i], off);
        }
    }
}

Map &Generator::map() const
{
    return Thinker_Map(thinker).as<Map>();
}

Generator::Id Generator::id() const
{
    return _id;
}

void Generator::setId(Id newId)
{
    DE_ASSERT(newId >= 1 && newId <= Map::MAX_GENERATORS); // 1-based
    _id = newId;
}

int Generator::age() const
{
    return _age;
}

Vec3d Generator::origin() const
{
    if(source)
    {
        Vec3d origin(source->origin);
        origin.z += -source->floorClip + FIX2FLT(originAtSpawn[2]);
        return origin;
    }

    return Vec3d(FIX2FLT(originAtSpawn[0]), FIX2FLT(originAtSpawn[1]), FIX2FLT(originAtSpawn[2]));
}

void Generator::clearParticles()
{
    Z_Free(_pinfo);
    _pinfo = nullptr;
}

void Generator::configureFromDef(const ded_ptcgen_t *newDef)
{
    DE_ASSERT(newDef);

    if(count <= 0)
        count = 1;

    // Make sure no generator is type-triggered by default.
    type   = type2 = -1;

    def    = newDef;
    _flags = Flags(def->flags);
    _pinfo = (ParticleInfo *) Z_Calloc(sizeof(ParticleInfo) * count, PU_MAP, 0);
    stages = (ParticleStage *) Z_Calloc(sizeof(ParticleStage) * def->stages.size(), PU_MAP, 0);

    for(int i = 0; i < def->stages.size(); ++i)
    {
        const ded_ptcstage_t *sdef = &def->stages[i];
        ParticleStage *s = &stages[i];

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
        originAtSpawn[i] = FLT2FIX(def->center[i]);
        vector[i] = FLT2FIX(def->vector[i]);
    }

    // Apply a random component to the spawn vector.
    if(def->initVectorVariance > 0)
    {
        uncertainPosition(vector, 0, FLT2FIX(def->initVectorVariance));
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
        runTick();
    }

    // Reset age so presim doesn't affect it.
    _age = 0;
}

bool Generator::isStatic() const
{
    return _flags.testFlag(Static);
}

bool Generator::isUntriggered() const
{
    return _untriggered;
}

void Generator::setUntriggered(bool yes)
{
    _untriggered = yes;
}

blendmode_t Generator::blendmode() const
{
    /// @todo Translate these old flags once, during definition parsing -ds
    if(_flags.testFlag(BlendAdditive))        return BM_ADD;
    if(_flags.testFlag(BlendSubtract))        return BM_SUBTRACT;
    if(_flags.testFlag(BlendReverseSubtract)) return BM_REVERSE_SUBTRACT;
    if(_flags.testFlag(BlendMultiply))        return BM_MUL;
    if(_flags.testFlag(BlendInverseMultiply)) return BM_INVERSE_MUL;
    return BM_NORMAL;
}

int Generator::activeParticleCount() const
{
    int numActive = 0;
    for(int i = 0; i < count; ++i)
    {
        if(_pinfo[i].stage >= 0)
        {
            numActive += 1;
        }
    }
    return numActive;
}

const ParticleInfo *Generator::particleInfo() const
{
    return _pinfo;
}

static void setParticleAngles(ParticleInfo *pinfo, int flags)
{
    DE_ASSERT(pinfo);

    if(flags & Generator::ParticleStage::ZeroYaw)
        pinfo->yaw = 0;
    if(flags & Generator::ParticleStage::ZeroPitch)
        pinfo->pitch = 0;
    if(flags & Generator::ParticleStage::RandomYaw)
        pinfo->yaw = RNG_RandFloat() * 65536;
    if(flags & Generator::ParticleStage::RandomPitch)
        pinfo->pitch = RNG_RandFloat() * 65536;
}

static void particleSound(fixed_t pos[3], ded_embsound_t *sound)
{
    DE_ASSERT(pos && sound);

    // Is there any sound to play?
    if(!sound->id || sound->volume <= 0) return;

    double orig[3];
    for (int i = 0; i < 3; ++i)
    {
        orig[i] = FIX2FLT(pos[i]);
    }

    S_LocalSoundAtVolumeFrom(sound->id, nullptr, orig, sound->volume);
}

int Generator::newParticle()
{
#ifdef __CLIENT__
    // Check for model-only generators.
    float inter = -1;
    FrameModelDef *mf = 0, *nextmf = 0;
    if(source)
    {
        mf = Mobj_ModelDef(*source, &nextmf, &inter);
        if(((!mf || !useModels) && def->flags & ModelOnly) ||
           (mf && useModels && mf->flags & MFF_NO_PARTICLES))
            return -1;
    }

    // Keep the spawn cursor in the valid range.
    if(++_spawnCP >= count)
    {
        _spawnCP -= count;
    }

    const int newParticleIdx = _spawnCP;

    // Set the particle's data.
    ParticleInfo *pinfo = &_pinfo[_spawnCP];
    pinfo->stage = 0;
    if(RNG_RandFloat() < def->altStartVariance)
    {
        pinfo->stage = def->altStart;
    }

    pinfo->tics = def->stages[pinfo->stage].tics *
        (1 - def->stages[pinfo->stage].variance * RNG_RandFloat());

    // Launch vector.
    pinfo->mov[0] = vector[0];
    pinfo->mov[1] = vector[1];
    pinfo->mov[2] = vector[2];

    // Apply some random variance.
    pinfo->mov[0] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pinfo->mov[1] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));
    pinfo->mov[2] += FLT2FIX(def->vectorVariance * (RNG_RandFloat() - RNG_RandFloat()));

    // Apply some aspect ratio scaling to the momentum vector.
    // This counters the 200/240 difference nearly completely.
    pinfo->mov[0] = FixedMul(pinfo->mov[0], FLT2FIX(1.1f));
    pinfo->mov[1] = FixedMul(pinfo->mov[1], FLT2FIX(0.95f));
    pinfo->mov[2] = FixedMul(pinfo->mov[2], FLT2FIX(1.1f));

    // Set proper speed.
    fixed_t uncertain = FLT2FIX(def->speed * (1 - def->speedVariance * RNG_RandFloat()));

    fixed_t len = FLT2FIX(M_ApproxDistancef(
        M_ApproxDistancef(FIX2FLT(pinfo->mov[0]), FIX2FLT(pinfo->mov[1])), FIX2FLT(pinfo->mov[2])));
    if(!len) len = FRACUNIT;
    len = FixedDiv(uncertain, len);

    pinfo->mov[0] = FixedMul(pinfo->mov[0], len);
    pinfo->mov[1] = FixedMul(pinfo->mov[1], len);
    pinfo->mov[2] = FixedMul(pinfo->mov[2], len);

    // The source is a mobj?
    if(source)
    {
        if(_flags & RelativeVector)
        {
            // Rotate the vector using the source angle.
            float temp[3];

            temp[0] = FIX2FLT(pinfo->mov[0]);
            temp[1] = FIX2FLT(pinfo->mov[1]);
            temp[2] = 0;

            // Player visangles have some problems, let's not use them.
            M_RotateVector(temp, source->angle / (float) ANG180 * -180 + 90, 0);

            pinfo->mov[0] = FLT2FIX(temp[0]);
            pinfo->mov[1] = FLT2FIX(temp[1]);
        }

        if(_flags & RelativeVelocity)
        {
            pinfo->mov[0] += FLT2FIX(source->mom[MX]);
            pinfo->mov[1] += FLT2FIX(source->mom[MY]);
            pinfo->mov[2] += FLT2FIX(source->mom[MZ]);
        }

        // Origin.
        pinfo->origin[0] = FLT2FIX(source->origin[0]);
        pinfo->origin[1] = FLT2FIX(source->origin[1]);
        pinfo->origin[2] = FLT2FIX(source->origin[2] - source->floorClip);

        uncertainPosition(pinfo->origin, FLT2FIX(def->spawnRadiusMin), FLT2FIX(def->spawnRadius));

        // Offset to the real center.
        pinfo->origin[2] += originAtSpawn[2];

        // Include bobbing in the spawn height.
        pinfo->origin[2] -= FLT2FIX(Mobj_BobOffset(*source));

        // Calculate XY center with mobj angle.
        const angle_t angle = Mobj_AngleSmoothed(source) + (fixed_t) (FIX2FLT(originAtSpawn[1]) / 180.0f * ANG180);
        const duint an      = angle >> ANGLETOFINESHIFT;
        const duint an2     = (angle + ANG90) >> ANGLETOFINESHIFT;

        pinfo->origin[0] += FixedMul(finecosine[an], originAtSpawn[0]);
        pinfo->origin[1] += FixedMul(finesine[an], originAtSpawn[0]);

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
                off[0] = nextmf->particleOffset(subidx)[0] - mf->particleOffset(subidx)[0];
                off[1] = nextmf->particleOffset(subidx)[1] - mf->particleOffset(subidx)[1];
                off[2] = nextmf->particleOffset(subidx)[2] - mf->particleOffset(subidx)[2];

                off[0] *= inter;
                off[1] *= inter;
                off[2] *= inter;
            }

            off[0] += mf->particleOffset(subidx)[0];
            off[1] += mf->particleOffset(subidx)[1];
            off[2] += mf->particleOffset(subidx)[2];

            // Apply it to the particle coords.
            pinfo->origin[0] += FixedMul(finecosine[an],  FLT2FIX(off[0]));
            pinfo->origin[0] += FixedMul(finecosine[an2], FLT2FIX(off[2]));
            pinfo->origin[1] += FixedMul(finesine[an],    FLT2FIX(off[0]));
            pinfo->origin[1] += FixedMul(finesine[an2],   FLT2FIX(off[2]));
            pinfo->origin[2] += FLT2FIX(off[1]);
        }
    }
    else if(plane)
    {
        /// @todo fixme: ignorant of mapped sector planes.
        fixed_t radius = stages[pinfo->stage].radius;
        const auto *sector = &plane->sector();

        // Choose a random spot inside the sector, on the spawn plane.
        if(_flags & SpawnSpace)
        {
            pinfo->origin[2] =
                FLT2FIX(sector->floor().height()) + radius +
                FixedMul(RNG_RandByte() << 8,
                         FLT2FIX(sector->ceiling().height() -
                                 sector->floor().height()) - 2 * radius);
        }
        else if((_flags & SpawnFloor) ||
                (!(_flags & (SpawnFloor | SpawnCeiling)) &&
                 plane->isSectorFloor()))
        {
            // Spawn on the floor.
            pinfo->origin[2] = FLT2FIX(plane->height()) + radius;
        }
        else
        {
            // Spawn on the ceiling.
            pinfo->origin[2] = FLT2FIX(plane->height()) - radius;
        }

        /**
         * Choosing the XY spot is a bit more difficult.
         * But we must be fast and only sufficiently accurate.
         *
         * @todo Nothing prevents spawning on the wrong side (or inside) of one-sided
         * walls (large diagonal subspaces!).
         */
        ConvexSubspace *subspace = 0;
        for (int i = 0; i < 5; ++i) // Try a couple of times (max).
        {
            float x = sector->bounds().minX +
                RNG_RandFloat() * (sector->bounds().maxX - sector->bounds().minX);
            float y = sector->bounds().minY +
                RNG_RandFloat() * (sector->bounds().maxY - sector->bounds().minY);

            subspace = maybeAs<ConvexSubspace>(map().bspLeafAt(Vec2d(x, y)).subspacePtr());
            if(subspace && sector == &subspace->sector())
                break;

            subspace = 0;
        }

        if(!subspace)
        {
            pinfo->stage = -1;
            return -1;
        }

        const AABoxd &subBounds = subspace->poly().bounds();

        // Try a couple of times to get a good random spot.
        int tries;
        for(tries = 0; tries < 10; ++tries) // Max this many tries before giving up.
        {
            float x = subBounds.minX +
                RNG_RandFloat() * (subBounds.maxX - subBounds.minX);
            float y = subBounds.minY +
                RNG_RandFloat() * (subBounds.maxY - subBounds.minY);

            pinfo->origin[0] = FLT2FIX(x);
            pinfo->origin[1] = FLT2FIX(y);

            if(subspace == map().bspLeafAt(Vec2d(x, y)).subspacePtr())
                break; // This is a good place.
        }

        if(tries == 10) // No good place found?
        {
            pinfo->stage = -1; // Damn.
            return -1;
        }
    }
    else if(isUntriggered())
    {
        // The center position is the spawn origin.
        pinfo->origin[0] = originAtSpawn[0];
        pinfo->origin[1] = originAtSpawn[1];
        pinfo->origin[2] = originAtSpawn[2];
        uncertainPosition(pinfo->origin, FLT2FIX(def->spawnRadiusMin),
                          FLT2FIX(def->spawnRadius));
    }

    // Initial angles for the particle.
    setParticleAngles(pinfo, def->stages[pinfo->stage].flags);

    // The other place where this gets updated is after moving over
    // a two-sided line.
    /*if(plane)
    {
        pinfo->sector = &plane->sector();
    }
    else*/
    {
        Vec2d ptOrigin(FIX2FLT(pinfo->origin[0]), FIX2FLT(pinfo->origin[1]));
        pinfo->bspLeaf = &map().bspLeafAt(ptOrigin);

        // A BSP leaf with no geometry is not a suitable place for a particle.
        if(!pinfo->bspLeaf->hasSubspace())
        {
            pinfo->stage = -1;
            return -1;
        }
    }

    // Play a stage sound?
    particleSound(pinfo->origin, &def->stages[pinfo->stage].sound);

    return newParticleIdx;
#else  // !__CLIENT__
    return -1;
#endif
}

#ifdef __CLIENT__

/**
 * Callback for the client mobj iterator, called from P_PtcGenThinker.
 */
static int newGeneratorParticlesWorker(mobj_t *cmo, void *context)
{
    Generator *gen = (Generator *) context;
    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(cmo);

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
 * Particle touches something solid. Returns false iff the particle dies.
 */
static int touchParticle(ParticleInfo *pinfo, Generator::ParticleStage *stage,
    ded_ptcstage_t *stageDef, bool touchWall)
{
    // Play a hit sound.
    particleSound(pinfo->origin, &stageDef->hitSound);

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

float Generator::particleZ(const ParticleInfo &pinfo) const
{
    const auto &subsec = pinfo.bspLeaf->subspace().subsector().as<Subsector>();
    if(pinfo.origin[2] == DDMAXINT)
    {
        return subsec.visCeiling().heightSmoothed() - 2;
    }
    if(pinfo.origin[2] == DDMININT)
    {
        return (subsec.visFloor().heightSmoothed() + 2);
    }
    return FIX2FLT(pinfo.origin[2]);
}

Vec3f Generator::particleOrigin(const ParticleInfo &pt) const
{
    return Vec3f(FIX2FLT(pt.origin[0]), FIX2FLT(pt.origin[1]), particleZ(pt));
}

Vec3f Generator::particleMomentum(const ParticleInfo &pt) const
{
    return Vec3f(FIX2FLT(pt.mov[0]), FIX2FLT(pt.mov[1]), FIX2FLT(pt.mov[2]));
}

void Generator::spinParticle(ParticleInfo &pinfo)
{
    static int const yawSigns[4]   = { 1,  1, -1, -1 };
    static int const pitchSigns[4] = { 1, -1,  1, -1 };

    const ded_ptcstage_t *stDef = &def->stages[pinfo.stage];
    const duint spinIndex        = uint(&pinfo - &_pinfo[id() / 8]) % 4;

    DE_ASSERT(spinIndex < 4);

    const int yawSign   =   yawSigns[spinIndex];
    const int pitchSign = pitchSigns[spinIndex];

    if(stDef->spin[0] != 0)
    {
        pinfo.yaw   += 65536 * yawSign   * stDef->spin[0] / (360 * TICSPERSEC);
    }
    if(stDef->spin[1] != 0)
    {
        pinfo.pitch += 65536 * pitchSign * stDef->spin[1] / (360 * TICSPERSEC);
    }

    pinfo.yaw   *= 1 - stDef->spinResistance[0];
    pinfo.pitch *= 1 - stDef->spinResistance[1];
}

void Generator::moveParticle(int index)
{
    DE_ASSERT(index >= 0 && index < count);

    ParticleInfo *pinfo   = &_pinfo[index];
    ParticleStage *st     = &stages[pinfo->stage];
    ded_ptcstage_t *stDef = &def->stages[pinfo->stage];

    // Particle rotates according to spin speed.
    spinParticle(*pinfo);

    // Changes to momentum.
    /// @todo Do not assume generator is from the CURRENT map.
    pinfo->mov[2] -= FixedMul(FLT2FIX(map().gravity()), st->gravity);

    // Vector force.
    if(stDef->vectorForce[0] != 0 || stDef->vectorForce[1] != 0 ||
       stDef->vectorForce[2] != 0)
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
       (source || isUntriggered()))
    {
        float delta[3];

        if(source)
        {
            delta[0] = FIX2FLT(pinfo->origin[0]) - source->origin[0];
            delta[1] = FIX2FLT(pinfo->origin[1]) - source->origin[1];
            delta[2] = particleZ(*pinfo) - (source->origin[2] + FIX2FLT(originAtSpawn[2]));
        }
        else
        {
            for(int i = 0; i < 3; ++i)
            {
                delta[i] = FIX2FLT(pinfo->origin[i] - originAtSpawn[i]);
            }
        }

        // Apply the offset (to source coords).
        for(int i = 0; i < 3; ++i)
        {
            delta[i] -= def->forceOrigin[i];
        }

        // Counter the aspect ratio of old times.
        delta[2] *= 1.2f;

        float dist = M_ApproxDistancef(M_ApproxDistancef(delta[0], delta[1]), delta[2]);
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
            if(def->forceAxis[0] || def->forceAxis[1] || def->forceAxis[2])
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
    fixed_t hardRadius = st->radius / 2;
    if((st->type == PTC_POINT || (st->type >= PTC_TEXTURE && st->type < PTC_TEXTURE + MAX_PTC_TEXTURES)) &&
       st->flags.testFlag(ParticleStage::PlaneFlat))
    {
        hardRadius = FRACUNIT;
    }

    // Check the new Z position only if not stuck to a plane.
    fixed_t z = pinfo->origin[2] + pinfo->mov[2];
    bool zBounce = false, hitFloor = false;
    if(pinfo->origin[2] != DDMININT && pinfo->origin[2] != DDMAXINT && pinfo->bspLeaf)
    {
        auto &subsec = pinfo->bspLeaf->subspace().subsector().as<Subsector>();
        if(z > FLT2FIX(subsec.visCeiling().heightSmoothed()) - hardRadius)
        {
            // The Z is through the roof!
            if(subsec.visCeiling().surface().hasSkyMaskedMaterial())
            {
                // Special case: particle gets lost in the sky.
                pinfo->stage = -1;
                return;
            }

            if(!touchParticle(pinfo, st, stDef, false))
                return;

            z = FLT2FIX(subsec.visCeiling().heightSmoothed()) - hardRadius;
            zBounce = true;
            hitFloor = false;
        }

        // Also check the floor.
        if(z < FLT2FIX(subsec.visFloor().heightSmoothed()) + hardRadius)
        {
            if(subsec.visFloor().surface().hasSkyMaskedMaterial())
            {
                pinfo->stage = -1;
                return;
            }

            if(!touchParticle(pinfo, st, stDef, false))
                return;

            z = FLT2FIX(subsec.visFloor().heightSmoothed()) + hardRadius;
            zBounce = true;
            hitFloor = true;
        }

        if(zBounce)
        {
            pinfo->mov[2] = FixedMul(-pinfo->mov[2], st->bounce);
            if(!pinfo->mov[2])
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
        pinfo->origin[2] = z;
    }

    // Now check the XY direction.
    // - Check if the movement crosses any solid lines.
    // - If it does, quit when first one contacted and apply appropriate
    //   bounce (result depends on the angle of the contacted wall).
    fixed_t x = pinfo->origin[0] + pinfo->mov[0];
    fixed_t y = pinfo->origin[1] + pinfo->mov[1];

    struct checklineworker_params_t
    {
        AABoxd box;
        fixed_t tmpz, tmprad, tmpx1, tmpx2, tmpy1, tmpy2;
        bool tmcross;
        Line *ptcHitLine;
    };
    checklineworker_params_t clParm; zap(clParm);
    clParm.tmcross = false; // Has crossed potential sector boundary?

    // XY movement can be skipped if the particle is not moving on the
    // XY plane.
    if(!pinfo->mov[0] && !pinfo->mov[1])
    {
        // If the particle is contacting a line, there is a chance that the
        // particle should be killed (if it's moving slowly at max).
        if(pinfo->contact)
        {
            auto *front = pinfo->contact->front().sectorPtr();
            auto *back  = pinfo->contact->back().sectorPtr();

            if (front && back && abs(pinfo->mov[2]) < FRACUNIT / 2)
            {
                const coord_t pz = particleZ(*pinfo);

                coord_t fz;
                if (front->floor().height() > back->floor().height())
                {
                    fz = front->floor().height();
                }
                else
                {
                    fz = back->floor().height();
                }

                coord_t cz;
                if (front->ceiling().height() < back->ceiling().height())
                {
                    cz = front->ceiling().height();
                }
                else
                {
                    cz = back->ceiling().height();
                }

                // If the particle is in the opening of a 2-sided line, it's
                // quite likely that it shouldn't be here...
                if (pz > fz && pz < cz)
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

    // We're moving in XY, so if we don't hit anything there can't be any line contact.
    pinfo->contact = 0;

    // Bounding box of the movement line.
    clParm.tmpz = z;
    clParm.tmprad = hardRadius;
    clParm.tmpx1 = pinfo->origin[0];
    clParm.tmpx2 = x;
    clParm.tmpy1 = pinfo->origin[1];
    clParm.tmpy2 = y;

    vec2d_t point;
    V2d_Set(point, FIX2FLT(MIN_OF(x, pinfo->origin[0]) - st->radius),
                   FIX2FLT(MIN_OF(y, pinfo->origin[1]) - st->radius));
    V2d_InitBox(clParm.box.arvec2, point);
    V2d_Set(point, FIX2FLT(MAX_OF(x, pinfo->origin[0]) + st->radius),
                   FIX2FLT(MAX_OF(y, pinfo->origin[1]) + st->radius));
    V2d_AddToBox(clParm.box.arvec2, point);

    // Iterate the lines in the contacted blocks.

    World::validCount++;
    DE_ASSERT(!clParm.ptcHitLine);
    map().forAllLinesInBox(clParm.box, [&clParm] (world::Line &line)
    {
        // Does the bounding box miss the line completely?
        if (clParm.box.maxX <= line.bounds().minX || clParm.box.minX >= line.bounds().maxX ||
            clParm.box.maxY <= line.bounds().minY || clParm.box.minY >= line.bounds().maxY)
        {
            return LoopContinue;
        }

        // Movement must cross the line.
        if ((line.pointOnSide(Vec2d(FIX2FLT(clParm.tmpx1), FIX2FLT(clParm.tmpy1))) < 0) ==
            (line.pointOnSide(Vec2d(FIX2FLT(clParm.tmpx2), FIX2FLT(clParm.tmpy2))) < 0))
        {
            return LoopContinue;
        }

        /*
         * We are possibly hitting something here.
         */

        // Bounce if we hit a solid wall.
        /// @todo fixme: What about "one-way" window lines?
        clParm.ptcHitLine = &line.as<Line>();
        if (!line.back().hasSector())
        {
            return LoopAbort; // Boing!
        }

        auto *front = line.front().sectorPtr();
        auto *back  = line.back().sectorPtr();

        // Determine the opening we have here.
        /// @todo Use R_OpenRange()
        fixed_t ceil;
        if (front->ceiling().height() < back->ceiling().height())
        {
            ceil = FLT2FIX(front->ceiling().height());
        }
        else
        {
            ceil = FLT2FIX(back->ceiling().height());
        }

        fixed_t floor;
        if (front->floor().height() > back->floor().height())
        {
            floor = FLT2FIX(front->floor().height());
        }
        else
        {
            floor = FLT2FIX(back->floor().height());
        }

        // There is a backsector. We possibly might hit something.
        if (clParm.tmpz - clParm.tmprad < floor || clParm.tmpz + clParm.tmprad > ceil)
        {
            return LoopAbort; // Boing!
        }

        // False alarm, continue checking.
        clParm.ptcHitLine = nullptr;
        // There is a possibility that the new position is in a new sector.
        clParm.tmcross    = true; // Afterwards, update the sector pointer.
        return LoopContinue;
    });

    if(clParm.ptcHitLine)
    {
        fixed_t normal[2], dotp;

        // Must survive the touch.
        if(!touchParticle(pinfo, st, stDef, true))
            return;

        // There was a hit! Calculate bounce vector.
        // - Project movement vector on the normal of hitline.
        // - Calculate the difference to the point on the normal.
        // - Add the difference to movement vector, negate movement.
        // - Multiply with bounce.

        // Calculate the normal.
        normal[0] = -FLT2FIX(clParm.ptcHitLine->direction().x);
        normal[1] = -FLT2FIX(clParm.ptcHitLine->direction().y);

        if(!normal[0] && !normal[1])
            goto quit_iteration;

        // Calculate as floating point so we don't overflow.
        dotp = FRACUNIT * (DOT2F(pinfo->mov, normal) / DOT2F(normal, normal));
        VECMUL(normal, dotp);
        VECSUB(normal, pinfo->mov);
        VECMULADD(pinfo->mov, 2 * FRACUNIT, normal);
        VECMUL(pinfo->mov, st->bounce);

        // Continue from the old position.
        x = pinfo->origin[0];
        y = pinfo->origin[1];
        clParm.tmcross = false; // Sector can't change if XY doesn't.

        // This line is the latest contacted line.
        pinfo->contact = clParm.ptcHitLine;
        goto quit_iteration;
    }

  quit_iteration:
    // The move is now OK.
    pinfo->origin[0] = x;
    pinfo->origin[1] = y;

    // Should we update the sector pointer?
    if(clParm.tmcross)
    {
        pinfo->bspLeaf = &map().bspLeafAt(Vec2d(FIX2FLT(x), FIX2FLT(y)));

        // A BSP leaf with no geometry is not a suitable place for a particle.
        if(!pinfo->bspLeaf->hasSubspace())
        {
            // Kill the particle.
            pinfo->stage = -1;
        }
    }
}

void Generator::runTick()
{
    // Source has been destroyed?
    if(!isUntriggered() && !map().thinkers().isUsedMobjId(srcid))
    {
        // Blasted... Spawning new particles becomes impossible.
        source = nullptr;
    }

    // Time to die?
    DE_ASSERT(def);
    if(++_age > def->maxAge && def->maxAge >= 0)
    {
        Generator_Delete(this);
        return;
    }

    // Spawn new particles?
    float newParts = 0;
    if((_age <= def->spawnAge || def->spawnAge < 0) &&
       (source || plane || type >= 0 || type == DED_PTCGEN_ANY_MOBJ_TYPE ||
        isUntriggered()))
    {
        newParts = def->spawnRate * spawnRateMultiplier;

        newParts *= particleSpawnRate *
            (1 - def->spawnRateVariance * RNG_RandFloat());

        newParts = de::min(newParts, float(def->particles)); // don't spawn too many

        _spawnCount += newParts;
        while(_spawnCount >= 1)
        {
            // Spawn a new particle.
            if(type == DED_PTCGEN_ANY_MOBJ_TYPE || type >= 0)  // Type-triggered?
            {
#ifdef __CLIENT__
                // Client's should also check the client mobjs.
                if(netState.isClient)
                {
                    map().clMobjIterator(newGeneratorParticlesWorker, this);
                }
#endif

                // Spawn new particles using all applicable sources.
                map().thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1 /*public*/, [this] (thinker_t *th)
                {
                    // Type match?
                    auto &mob = *reinterpret_cast<mobj_t *>(th);
                    if(type == DED_PTCGEN_ANY_MOBJ_TYPE || mob.type == type || mob.type == type2)
                    {
                        // Someone might think this is a slight hack...
                        source = &mob;
                        newParticle();
                    }
                    return LoopContinue;
                });

                // The generator has no real source.
                source = nullptr;
            }
            else
            {
                newParticle();
            }

            _spawnCount--;
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
            if(++pinfo->stage == def->stages.size() ||
               stages[pinfo->stage].type == PTC_NONE)
            {
                // Kill the particle.
                pinfo->stage = -1;
                continue;
            }

            pinfo->tics = def->stages[pinfo->stage].tics * (1 - def->stages[pinfo->stage].variance * RNG_RandFloat());

            // Change in particle angles?
            setParticleAngles(pinfo, def->stages[pinfo->stage].flags);

            // Play a sound?
            particleSound(pinfo->origin, &def->stages[pinfo->stage].sound);
        }

        // Try to move.
        moveParticle(i);
    }
}

void Generator::consoleRegister() //static
{
    C_VAR_FLOAT("rend-particle-rate", &particleSpawnRate, 0, 0, 5);
}

void Generator_Delete(Generator *gen)
{
    if(!gen) return;
    gen->map().unlinkGenerator(*gen);
    gen->map().thinkers().remove(gen->thinker);
    gen->clearParticles();
    Z_Free(gen->stages); gen->stages = nullptr;
    // The generator itself is free'd when it's next turn for thinking comes.
}

void Generator_Thinker(Generator *gen)
{
    DE_ASSERT(gen != 0);
    gen->runTick();
}
