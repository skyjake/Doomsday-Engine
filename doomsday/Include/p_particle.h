#ifndef __DOOMSDAY_PARTICLES_H__
#define __DOOMSDAY_PARTICLES_H__

#define MAX_ACTIVE_PTCGENS	96
#define MAX_PTC_STAGES		DED_PTC_STAGES

// Generator flags
#define PGF_STATIC				0x1		// Can't be replaced by anything.
#define PGF_RELATIVE_VELOCITY	0x2		// Particles inherit source's velocity.
#define PGF_SPAWN_ONLY			0x4		// Generator is spawned only when source is being spawned.
//#define PGF_ANGLE_SPAWN		0x8		// Rotate spawn vector w/mobj angle.
#define PGF_ADD_BLEND			0x10	// Render using additive blending.
#define PGF_FLOOR_SPAWN			0x20	// Flat-trig: spawn on floor.
#define PGF_CEILING_SPAWN		0x40	// Flat-trig: spawn on ceiling.
#define PGF_SPACE_SPAWN			0x80	// Flat-trig: spawn in air.
#define PGF_PARTS_PER_128		0x100	// Definition specifies a density.
#define PGF_MODEL_ONLY			0x200	// Only spawn if source is a 3D model.
#define PGF_SCALED_RATE			0x400	// Spawn rate affected by a factor.

// Runtime generator flags:
#define PGF_VISIBLE				0x1000000	// Should be rendered.
#define PGF_UNTRIGGERED			0x2000000

// Particle types
enum 
{
	PTC_NONE,
	PTC_POINT,
	PTC_LINE
} ptc_type_e;

// Particle flags
#define PTCF_STAGE_TOUCH	0x1		// Touching ends current stage.
#define PTCF_DIE_TOUCH		0x2		// Dies from first touch.
#define PTCF_BRIGHT			0x4		// Fullbright.
#define PTCF_SHADING		0x8		// Pseudo-3D.
#define PTCF_PLANE_FLAT		0x10	// Touches a plane => render as flat.
#define PTCF_STAGE_WALL_TOUCH 0x20	// Touch a wall => end stage.
#define PTCF_STAGE_FLAT_TOUCH 0x40	// Touch a flat => end stage.
#define PTCF_WALL_FLAT		0x80	// Touches a wall => render as flat.
#define PTCF_SPHERE_FORCE	0x100

typedef struct
{
	char			stage;			// -1 => particle doesn't exist
	short			tics;
	int				pos[3];			// Coordinates.
	int				mov[3];			// Momentum.
	sector_t		*sector;		// Updated when needed.
	line_t			*contact;		// Updated when lines hit/avoided.
} particle_t;

typedef struct
{
	short			type;
	short			flags;
	int				resistance;
	int				bounce;
	int				radius;
	int				gravity;
} ptcstage_t;

// Particle Generator
typedef struct ptcgen_s
{
	thinker_t		thinker;		// Func = P_PtcGenThinker
	sector_t		*sector;		// Flat-triggered.
	int				ceiling;		// Flat-triggered.
	float			area;			// Rough estimate of sector area.
	ded_ptcgen_t	*def;			// The definition of this generator.
	mobj_t			*source;		// If mobj-triggered.
	int				srcid;			// Source mobj ID.
	int				type;			// Type-triggered; mobj type number (-1=none).
	int				type2;			// Type-triggered; alternate type.
	fixed_t			center[3];		// Used by untriggered/damage gens.
	fixed_t			vector[3];		// Converted from the definition.
	int				flags;
	float			spawncount;
	int				spawncp;		// Spawn cursor.
	int				age;
	int				count;			// Number of particles.
	particle_t		*ptcs;			// List of particles.
	ptcstage_t		stages[MAX_PTC_STAGES];
} ptcgen_t;

ptcgen_t *active_ptcgens[MAX_ACTIVE_PTCGENS];

void P_SpawnParticleGen(ded_ptcgen_t *def, mobj_t *source);
void P_SpawnTypeParticleGens(void);
void P_SpawnMapParticleGens(char *map_id);
void P_SpawnDamageParticleGen(mobj_t *mo, mobj_t *inflictor, int amount);
void P_CheckPtcPlanes(void);

float P_GetParticleRadius(ded_ptcstage_t *stage_def, int ptc_index);

#endif