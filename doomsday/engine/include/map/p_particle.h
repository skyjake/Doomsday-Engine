/** @file p_particle.h Generator Management (Particles).
 * @ingroup map
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_GENERATORS_H
#define LIBDENG_MAP_GENERATORS_H

#include "def_data.h"

// Maximum number of particle textures (not instances).
#define MAX_PTC_TEXTURES        32

// Maximum number of particle models (not instances).
#define MAX_PTC_MODELS          100

/**
 * @defgroup generatorFlags  Generator Flags
 * @ingroup flags
 */
///@{
#define PGF_STATIC              0x1 ///< Can't be replaced by anything.
#define PGF_RELATIVE_VELOCITY   0x2 ///< Particles inherit source's velocity.
#define PGF_SPAWN_ONLY          0x4 ///< Generator is spawned only when source is being spawned.
#define PGF_RELATIVE_VECTOR     0x8 ///< Rotate spawn vector w/mobj angle.
#define PGF_ADD_BLEND           0x10 ///< Render using additive blending.
#define PGF_FLOOR_SPAWN         0x20 ///< Flat-trig: spawn on floor.
#define PGF_CEILING_SPAWN       0x40 ///< Flat-trig: spawn on ceiling.
#define PGF_SPACE_SPAWN         0x80 ///< Flat-trig: spawn in air.
#define PGF_PARTS_PER_128       0x100 ///< Definition specifies a density.
#define PGF_MODEL_ONLY          0x200 ///< Only spawn if source is a 3D model.
#define PGF_SCALED_RATE         0x400 ///< Spawn rate affected by a factor.
#define PGF_GROUP               0x800 ///< Triggered by all in anim group.
#define PGF_SUB_BLEND           0x1000 ///< Subtractive blending.
#define PGF_REVSUB_BLEND        0x2000 ///< Reverse subtractive blending.
#define PGF_MUL_BLEND           0x4000 ///< Multiplicative blending.
#define PGF_INVMUL_BLEND        0x8000 ///< Inverse multiplicative blending.
#define PGF_STATE_CHAIN         0x10000 ///< Chain after existing state gen(s).

// Runtime generator flags:
#define PGF_UNTRIGGERED         0x8000000
///@}

// Particle types
typedef enum {
    PTC_NONE,
    PTC_POINT,
    PTC_LINE,
    // New types can be added here.
    PTC_TEXTURE = 100,
    // ...followed by MAX_PTC_TEXTURES types.
    PTC_MODEL = 1000
} ptc_type_e;

/**
 * @defgroup particleFlags  Particle Flags
 * @ingroup flags
 */
///@{
#define PTCF_STAGE_TOUCH        0x1 ///< Touching ends current stage.
#define PTCF_DIE_TOUCH          0x2 ///< Dies from first touch.
#define PTCF_BRIGHT             0x4 ///< Fullbright.
#define PTCF_SHADING            0x8 ///< Pseudo-3D.
#define PTCF_PLANE_FLAT         0x10 ///< Touches a plane => render as flat.
#define PTCF_STAGE_WALL_TOUCH   0x20 ///< Touch a wall => end stage.
#define PTCF_STAGE_FLAT_TOUCH   0x40 ///< Touch a flat => end stage.
#define PTCF_WALL_FLAT          0x80 ///< Touches a wall => render as flat.
#define PTCF_SPHERE_FORCE       0x100
#define PTCF_ZERO_YAW           0x200 ///< Set particle yaw to zero.
#define PTCF_ZERO_PITCH         0x400 ///< Set particle pitch to zero.
#define PTCF_RANDOM_YAW         0x800
#define PTCF_RANDOM_PITCH       0x1000
///@}

typedef struct {
    int stage; // -1 => particle doesn't exist
    short tics;
    fixed_t origin[3]; // Coordinates.
    fixed_t mov[3]; // Momentum.
    Sector *sector; // Updated when needed.
    LineDef *contact; // Updated when lines hit/avoided.
    ushort yaw, pitch; // Rotation angles (0-65536 => 0-360).
} particle_t;

typedef struct {
    short type;
    short flags;
    fixed_t resistance;
    fixed_t bounce;
    fixed_t radius;
    fixed_t gravity;
} ptcstage_t;

// Particle Generator
typedef struct ptcgen_s {
    thinker_t thinker; // Func = P_PtcGenThinker
    Plane *plane; // Flat-triggered.
    struct ded_ptcgen_s const *def; // The definition of this generator.
    struct mobj_s *source; // If mobj-triggered.
    int srcid; // Source mobj ID.
    int type; // Type-triggered; mobj type number (-1=none).
    int type2; // Type-triggered; alternate type.
    fixed_t center[3]; // Used by untriggered/damage gens.
    fixed_t vector[3]; // Converted from the definition.
    int flags; // PGF_* flags.
    float spawnCount;
    float spawnRateMultiplier;
    int spawnCP; // Spawn cursor.
    int age;
    int count; // Number of particles.
    particle_t *ptcs; // List of particles.
    ptcstage_t *stages;
} ptcgen_t;

DENG_EXTERN_C byte useParticles;
DENG_EXTERN_C int maxParticles;
DENG_EXTERN_C float particleSpawnRate;

#ifdef __cplusplus
extern "C" {
#endif

void P_PtcInitForMap(void);

/**
 * Attempt to spawn all flat-triggered particle generators for the current map.
 * To be called after map setup is completed.
 *
 * @note Cannot presently be done in P_PtcInitForMap as this is called during
 *       initial Map load and before any saved game has been loaded.
 */
void P_MapSpawnPlaneParticleGens(void);

/**
 * Link all active particle generators into the world.
 */
void P_CreatePtcGenLinks(void);

/**
 * Creates a new mobj-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
void P_SpawnMobjParticleGen(struct ded_ptcgen_s const *def, struct mobj_s *source);

/**
 * Spawns all type-triggered particle generators, regardless of whether
 * the type of mobj exists in the map or not (mobjs might be dynamically
 * created).
 */
void P_SpawnTypeParticleGens(void);

void P_SpawnMapParticleGens(void);

/**
 * Creates a new flat-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
void P_SpawnPlaneParticleGen(struct ded_ptcgen_s const *def, Plane *plane);

/**
 * Called after a reset once the definitions have been re-read.
 */
void P_UpdateParticleGens(void);

/**
 * Takes care of consistent variance.
 * Currently only used visually, collisions use the constant radius.
 * The variance can be negative (results will be larger).
 */
float P_GetParticleRadius(ded_ptcstage_t const *stageDef, int ptcIndex);

/**
 * A particle may be attached to the floor or ceiling of the sector.
 */
float P_GetParticleZ(particle_t const *pt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_GENERATORS_H
