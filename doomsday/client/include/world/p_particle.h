/** @file p_particle.h  World map generator management (particles).
 *
 * @ingroup world
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_WORLD_P_PARTICLE_H
#define DENG_CLIENT_WORLD_P_PARTICLE_H

#include <de/Vector>
#include "def_data.h"
#include "map.h"

class BspLeaf;
class Line;
class Plane;
struct mobj_s;

// Maximum number of particle textures (not instances).
#define MAX_PTC_TEXTURES        300

// Maximum number of particle models (not instances).
#define MAX_PTC_MODELS          100

enum ParticleType {
    PTC_NONE,
    PTC_POINT,
    PTC_LINE,
    // New types can be added here.
    PTC_TEXTURE = 100,
    // ...followed by MAX_PTC_TEXTURES types.
    PTC_MODEL = 1000
};

/**
 * POD structure used when querying the current state of a particle.
 */
struct ParticleInfo
{
    int stage;         ///< -1 => particle doesn't exist
    short tics;
    fixed_t origin[3]; ///< Coordinates.
    fixed_t mov[3];    ///< Momentum.
    BspLeaf *bspLeaf;  ///< Updated when needed.
    Line *contact;     ///< Updated when lines hit/avoided.
    ushort yaw, pitch; ///< Rotation angles (0-65536 => 0-360).
};

/**
 * Particle generator.
 *
 * @ingroup world
 */
struct Generator
{
    /**
     * Particle animation is defined as a sequence of (perhaps interpolated)
     * property value stages->
     */
    struct ParticleStage
    {
        enum Flag
        {
            StageTouch     = 0x1,   ///< Touching ends current stage.
            DieTouch       = 0x2,   ///< Dies from first touch.
            Bright         = 0x4,   ///< Fullbright.
            Shading        = 0x8,   ///< Pseudo-3D.
            PlaneFlat      = 0x10,  ///< Touches a plane => render as flat.
            StageWallTouch = 0x20,  ///< Touch a wall => end stage.
            StageFlatTouch = 0x40,  ///< Touch a flat => end stage.
            WallFlat       = 0x80,  ///< Touches a wall => render as flat.
            SphereForce    = 0x100,
            ZeroYaw        = 0x200, ///< Set particle yaw to zero.
            ZeroPitch      = 0x400, ///< Set particle pitch to zero.
            RandomYaw      = 0x800,
            RandomPitch    = 0x1000
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        short type;
        Flags flags;
        fixed_t resistance;
        fixed_t bounce;
        fixed_t radius;
        fixed_t gravity;
    };

    enum Flag
    {
        Static               = 0x1,     ///< Can't be replaced by anything.
        RelativeVelocity     = 0x2,     ///< Particles inherit source's velocity.
        SpawnOnly            = 0x4,     ///< Generator is spawned only when source is being spawned.
        RelativeVector       = 0x8,     ///< Rotate spawn vector w/mobj angle.
        BlendAdditive        = 0x10,    ///< Render using additive blending.
        SpawnFloor           = 0x20,    ///< Flat-trig: spawn on floor.
        SpawnCeiling         = 0x40,    ///< Flat-trig: spawn on ceiling.
        SpawnSpace           = 0x80,    ///< Flat-trig: spawn in air.
        Density              = 0x100,   ///< Definition specifies a density.
        ModelOnly            = 0x200,   ///< Only spawn if source is a 3D model.
        ScaledRate           = 0x400,   ///< Spawn rate affected by a factor.
        Group                = 0x800,   ///< Triggered by all in anim group.
        BlendSubtract        = 0x1000,  ///< Subtractive blending.
        BlendReverseSubtract = 0x2000,  ///< Reverse subtractive blending.
        BlendMultiply        = 0x4000,  ///< Multiplicative blending.
        BlendInverseMultiply = 0x8000,  ///< Inverse multiplicative blending.
        StateChain           = 0x10000, ///< Chain after existing state gen(s).

        // Runtime generator flags:
        Untriggered          = 0x8000000
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /// Unique identifier associated with each generator.
    typedef short Id;

    thinker_t thinker;         ///< Func = P_PtcGenThinker
    Plane *plane;              ///< Flat-triggered.
    ded_ptcgen_t const *def;   ///< The definition of this generator.
    struct mobj_s *source;     ///< If mobj-triggered.
    int srcid;                 ///< Source mobj ID.
    int type;                  ///< Type-triggered; mobj type number (-1=none).
    int type2;                 ///< Type-triggered; alternate type.
    fixed_t center[3];         ///< Used by untriggered/damage gens.
    fixed_t vector[3];         ///< Converted from the definition.
    Flags flags;
    float spawnCount;
    float spawnRateMultiplier;
    int spawnCP;               ///< Spawn cursor.
    int age;
    int count;                 ///< Number of particles generated thus far.
    ParticleStage *stages;

    /**
     * Returns the map in which the generator exists.
     *
     * @see Thinker_Map()
     */
    de::Map &map() const;

    /**
     * Returns the unique identifier of the generator.
     */
    Id id() const;

    /**
     * Change the unique identifier of the generator.
     *
     * @param newId  New identifier to apply.
     */
    void setId(Id newId);

    /**
     * Set gen->count prior to calling this function.
     */
    void configureFromDef(ded_ptcgen_t const *def);

    /**
     * Spawn and move the generated particles.
     */
    void runTick();

    /**
     * Run the generator's thinker for the given number of @a tics.
     */
    void presimulate(int tics);

    /**
     * Determine the @em approximate origin of the generator in map space.
     */
    de::Vector3d origin() const;

    /**
     * Returns @c true iff the generator is @em static, meaning it will not be
     * replaced under any circumstances.
     */
    bool isStatic() const;

    /**
     * Returns the currently configured blending mode for the generator.
     */
    blendmode_t blendmode() const;

    /**
     * Provides readonly access to the generator particle info data.
     */
    ParticleInfo const *particleInfo() const;

public: /// @todo make private:
    /**
     * Clears all memory used for manipulating the generated particles.
     */
    void clearParticles();

    /**
     * Attempt to spawn a new particle.
     */
    ParticleInfo *newParticle();

    /**
     * The movement is done in two steps:
     * Z movement is done first. Skyflat kills the particle.
     * XY movement checks for hits with solid walls (no backsector).
     * This is supposed to be fast and simple (but not too simple).
     */
    void moveParticle(ParticleInfo *pt);

    void spinParticle(ParticleInfo *pt);

    /**
     * A particle may be "projected" onto the floor or ceiling of a sector.
     */
    float particleZ(ParticleInfo const *pt) const;

private:
    Id _id;               ///< Unique in the map.
    ParticleInfo *_pinfo; ///< Info about each generated particle.
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Generator::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Generator::ParticleStage::Flags)

typedef Generator::ParticleStage GeneratorParticleStage;

void Generator_Delete(Generator *gen);
void Generator_Thinker(Generator *gen);

DENG_EXTERN_C byte useParticles;
DENG_EXTERN_C int maxParticles;
DENG_EXTERN_C float particleSpawnRate;

void P_PtcInitForMap(de::Map &map);

/**
 * Attempt to spawn all flat-triggered particle generators for the @a map.
 * To be called after map setup is completed.
 *
 * @note Cannot presently be done in P_PtcInitForMap as this is called during
 *       initial Map load and before any saved game has been loaded.
 */
void P_MapSpawnPlaneParticleGens(de::Map &map);

/**
 * Spawns all type-triggered particle generators, regardless of whether
 * the type of mobj exists in the map or not (mobjs might be dynamically
 * created).
 */
void P_SpawnTypeParticleGens(de::Map &map);

void P_SpawnMapParticleGens(de::Map &map);

/**
 * Update existing generators in the map following an engine reset.
 */
void P_UpdateParticleGens(de::Map &map);

/**
 * Creates a new mobj-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
void P_SpawnMobjParticleGen(ded_ptcgen_t const *def, struct mobj_s *source);

void P_SpawnMapDamageParticleGen(struct mobj_s *mo, struct mobj_s *inflictor, int amount);

/**
 * Creates a new flat-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
void P_SpawnPlaneParticleGen(ded_ptcgen_t const *def, Plane *plane);

/**
 * Takes care of consistent variance.
 * Currently only used visually, collisions use the constant radius.
 * The variance can be negative (results will be larger).
 */
float P_GetParticleRadius(ded_ptcstage_t const *stageDef, int ptcIndex);

#endif // DENG_CLIENT_WORLD_P_PARTICLE_H
