/** @file generator.h  World map (particle) generator.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include <de/vector.h>
#include <doomsday/defs/dedtypes.h>
#include "map.h"

class Line;
class Plane;
struct mobj_s;

/**
 * POD structure used when querying the current state of a particle.
 */
struct ParticleInfo
{
    int             stage;      // -1 => particle doesn't exist
    int16_t         tics;
    fixed_t         origin[3];  // Coordinates.
    fixed_t         mov[3];     // Momentum.
    world::BspLeaf *bspLeaf;    // Updated when needed.
    Line *          contact;    // Updated when lines hit/avoided.
    uint16_t        yaw, pitch; // Rotation angles (0-65536 => 0-360).
};

/**
 * Particle generator.
 */
struct Generator
{
    /**
     * Particle animation is defined as a sequence of (perhaps interpolated)
     * property value stages.
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

        int16_t type;
        de::Flags flags;
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
        StateChain           = 0x10000  ///< Chain after existing state gen(s).
    };

    /// Unique identifier associated with each generator (1-based).
    typedef int16_t Id;

public:                                   //! @todo make private:
    thinker_t           thinker;          //  Func = P_PtcGenThinker
    Plane *             plane;            //  Flat-triggered.
    const ded_ptcgen_t *def;              //  The definition of this generator.
    struct mobj_s *     source;           //  If mobj-triggered.
    int                 srcid;            //  Source mobj ID.
    int                 type;             //  Type-triggered; mobj type number (-1=none).
    int                 type2;            //  Type-triggered; alternate type.
    fixed_t             originAtSpawn[3]; //  Used by untriggered/damage gens.
    fixed_t             vector[3];        //  Converted from the definition.
    float               spawnRateMultiplier;
    int                 count;            // Number of particles generated thus far.
    ParticleStage *     stages;

public:
    /**
     * Returns the map in which the generator exists.
     *
     * @see Thinker_Map()
     */
    Map &map() const;

    /**
     * Returns the unique identifier of the generator. The identifier is 1-based.
     */
    Id id() const;

    /**
     * Change the unique identifier of the generator. The identifier is 1-based.
     *
     * @param newId  New identifier to apply.
     */
    void setId(Id newId);

    /**
     * Set gen->count prior to calling this function.
     */
    void configureFromDef(const ded_ptcgen_t *def);

    /**
     * Generate and/or move the particles.
     */
    void runTick();

    /**
     * Run the generator's thinker for the given number of @a tics.
     */
    void presimulate(int tics);

    /**
     * Returns the age of the generator (time since spawn), in tics.
     */
    int age() const;

    /**
     * Determine the @em approximate origin of the generator in map space.
     *
     * @note In the case of generator attached to a mobj this is the @em current,
     * unsmoothed origin of the mobj offset by the @em initial origin at generator
     * spawn time. For all other types of generator the initial origin at generator
     * spawn time is returned.
     */
    de::Vec3d origin() const;

    /**
     * Returns @c true iff the generator is @em static, meaning it will not be
     * replaced under any circumstances.
     */
    bool isStatic() const;

    /**
     * Returns @c true iff the generator is @em untriggered.
     */
    bool isUntriggered() const;

    /**
     * Change the @em untriggered state of the generator.
     */
    void setUntriggered(bool yes = true);

    /**
     * Returns the currently configured blending mode for the generator.
     */
    blendmode_t blendmode() const;

    /**
     * Returns the total number of @em active particles for the generator.
     */
    int activeParticleCount() const;

    /**
     * Provides readonly access to the generator particle info data.
     */
    const ParticleInfo *particleInfo() const;

public: /// @todo make private:
    /**
     * Clears all memory used for manipulating the generated particles.
     */
    void clearParticles();

    /**
     * Attempt to spawn a new particle.
     *
     * @return  Index of the newly spawned particle; otherwise @c -1.
     */
    int newParticle();

    /**
     * The movement is done in two steps:
     * Z movement is done first. Skyflat kills the particle.
     * XY movement checks for hits with solid walls (no backsector).
     * This is supposed to be fast and simple (but not too simple).
     */
    void moveParticle(int index);

    void spinParticle(ParticleInfo &pt);

    float particleZ(const ParticleInfo &pt) const;

    de::Vec3f particleOrigin(const ParticleInfo &pt) const;
    de::Vec3f particleMomentum(const ParticleInfo &pt) const;

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

private:
    Id            _id; // Unique in the map.
    de::Flags     _flags;
    int           _age; // Time since spawn, in tics.
    float         _spawnCount;
    bool          _untriggered; // @c true= consider this as not yet triggered.
    int           _spawnCP;     // Particle spawn cursor.
    ParticleInfo *_pinfo;       // Info about each generated particle.
};

typedef Generator::ParticleStage GeneratorParticleStage;

void Generator_Delete(Generator *gen);
void Generator_Thinker(Generator *gen);
