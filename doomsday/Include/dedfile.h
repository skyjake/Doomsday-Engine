// Doomsday Engine Definition Files
// (not a shining example of brilliant design, but works
//  after a fashion)

#ifndef __DOOMSDAY_DED_FILES_H__
#define __DOOMSDAY_DED_FILES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_dfdat.h"

#define DED_VERSION			5

#define DED_SPRITEID_LEN	4
#define DED_STRINGID_LEN	31
#define DED_PATH_LEN		128
#define DED_FLAGS_LEN		400
#define DED_FUNC_LEN		255

#define DED_PTC_STAGES		16

typedef char			ded_stringid_t[DED_STRINGID_LEN+1];
typedef ded_stringid_t	ded_string_t;
typedef ded_stringid_t	ded_mobjid_t;
typedef ded_stringid_t	ded_stateid_t;
typedef ded_stringid_t	ded_soundid_t;
typedef ded_stringid_t	ded_musicid_t;
typedef ded_stringid_t	ded_funcid_t;
typedef char			ded_func_t[DED_FUNC_LEN+1];
typedef char			ded_flags_t[DED_FLAGS_LEN+1];

typedef struct ded_count_s { int num, max; } ded_count_t;

typedef struct
{
	char			path[DED_PATH_LEN + 1];
} ded_path_t;

typedef struct
{
	char			id[DED_SPRITEID_LEN + 1];
} ded_sprid_t;

typedef struct
{
	char			str[DED_STRINGID_LEN + 1];
} ded_str_t;

typedef struct
{
	ded_stringid_t	id;
	int				value;
} ded_flag_t;

typedef struct
{
	ded_mobjid_t	id;	
	int				doomednum;
	ded_string_t	name;
	
	ded_stateid_t	spawnstate;
	ded_stateid_t	seestate;
	ded_stateid_t	painstate;
	ded_stateid_t	meleestate;
	ded_stateid_t	missilestate;
	ded_stateid_t	crashstate;
	ded_stateid_t	deathstate;
	ded_stateid_t	xdeathstate;
	ded_stateid_t	raisestate;

	ded_soundid_t	seesound;
	ded_soundid_t	attacksound;
	ded_soundid_t	painsound;
	ded_soundid_t	deathsound;
	ded_soundid_t	activesound;

	int				reactiontime;
	int				painchance;
	int				spawnhealth;
	float			speed;
	float			radius;
	float			height;
	int				mass;
	int				damage;
	char			flags[NUM_MOBJ_FLAGS][DED_FLAGS_LEN+1];
	int				misc[NUM_MOBJ_MISC];
} ded_mobj_t;

typedef struct
{	
	ded_stateid_t	id;				// ID of this state.
	ded_sprid_t		sprite;
	ded_flags_t		flags;
	int				frame;
	int				tics;
	ded_funcid_t	action;
	ded_stateid_t	nextstate;
	int				misc[NUM_STATE_MISC];
} ded_state_t;

typedef struct
{
	ded_stateid_t	state;
	float			xoffset;		// Origin offset in world coords
	float			yoffset;		// Zero means automatic
	float			size;			// Zero: automatic
	float			color[3];		// Red Green Blue (0,1)
	ded_flags_t		flags_string;	
	int				flags;			// Runtime
} ded_light_t;

typedef struct
{
	ded_path_t		filename;
	ded_string_t	frame;
	int				framerange;
	ded_flags_t		flags;			// ASCII string of the flags.
	int				skin;
	int				skinrange;
	float			offset[3];		// Offset XYZ within model.
	float			alpha;
	float			parm;			// Custom parameter.
	unsigned char	selskinbits[2];	// Mask and offset.
	unsigned char	selskins[8];	
	ded_string_t	shinyskin;
	float			shiny;
	float			shinycolor[3];
} ded_submodel_t;

typedef struct
{
	ded_stateid_t	state;
	int				off;
	ded_sprid_t		sprite;			// Only used by autoscale.
	int				spriteframe;	// Only used by autoscale.
	ded_flags_t		group;
	int				selector;
	ded_flags_t		flags;
	float			intermark;
	float			interrange[2];	// 0-1 by default.
	int				skintics;		// Tics per skin in range.
	float			scale[3];		// Scale XYZ
	float			resize;
	float			offset[3];		// Offset XYZ
	float			shadowradius;	// Radius for shadow (0=auto).
	ded_submodel_t	sub[4];
} ded_model_t;

typedef struct
{
	ded_soundid_t	id;			// ID of this sound, refered to by others.
	ded_string_t	lumpname;	// Actual lump name of the sound (DS not included).
	ded_string_t	name;		// A tag name for the sound.
	ded_soundid_t	link;		// Link to another sound.
	int				link_pitch;
	int				link_volume;
	int				priority;	// Priority classification.
	int				channels;	// Max number of channels to occupy.
	int				group;		// Exclusion group.
	ded_flags_t		flags;		// Flags (like chg_pitch).
	ded_path_t		ext;		// External sound file (WAV).
} ded_sound_t;

typedef struct
{
	ded_musicid_t	id;			// ID of this piece of music.
	ded_string_t	lumpname;	// Lump name.
	ded_path_t		path;		// External file (not a normal MUS file).
	int				cdtrack;	// 0 = no track.
} ded_music_t;

typedef struct
{
	ded_flags_t		flags;
	ded_string_t	texture;
	float			offset;
	float			color_limit;
} ded_skylayer_t;

#define NUM_SKY_LAYERS		2

typedef struct
{
	ded_stringid_t	id;			// ID of the map (e.g. E2M3 or MAP21).
	char			name[64];	// Name of the map.
	ded_string_t	author;		// Author of the map.
	ded_flags_t		flags;		// Flags.
	ded_musicid_t	music;		// Music to play.
	float			partime;	// Partime, in seconds.
	float			fog_color[3]; // Fog color (RGB).
	float			fog_start;
	float			fog_end;
	float			fog_density;
	float			ambient;	// Ambient light level.
	float			gravity;	// 1 = normal.
	float			sky_height;
	float			horizon_offset;
	ded_skylayer_t	sky_layers[NUM_SKY_LAYERS];
} ded_mapinfo_t;

typedef struct
{
	ded_stringid_t	id;
	char			*text;
} ded_text_t;

typedef struct
{
	ded_stringid_t	id;
	ded_count_t		count;
	ded_str_t		*textures;
} ded_tenviron_t;

typedef struct
{
	char			*id;	
	char			*text;
} ded_value_t;

typedef struct
{
	ded_stringid_t	before;
	ded_stringid_t	after;
	int				game;
	char			*script;
} ded_finale_t;

typedef struct
{
	int				id;
	char			comment[64];
	ded_flags_t		flags[3];
	ded_stringid_t	line_class;
	ded_stringid_t	act_type;
	int				act_count;
	float			act_time;
	int				act_tag;
	int				aparm[7];
	ded_stringid_t	aparm_str[3];	// aparms 4, 6, 9
	float			ticker_start;
	float			ticker_end;
	int				ticker_interval;
	ded_soundid_t	act_sound;
	ded_soundid_t	deact_sound;
	int				ev_chain;
	int				act_chain;
	int				deact_chain;
	ded_stringid_t	wallsection;
	ded_stringid_t	act_tex;
	ded_stringid_t	deact_tex;
	char			act_msg[128];
	char			deact_msg[128];
	float			texmove_angle;
	float			texmove_speed;
	int				iparm[20];
	char			iparm_str[20][64];
	float			fparm[20];
	char			sparm[5][128];	
} ded_linetype_t;

typedef struct
{
	int				id;
	char			comment[64];
	ded_flags_t		flags;
	int				act_tag;
	int				chain[5];
	ded_flags_t		chain_flags[5];
	float			start[5];
	float			end[5];
	float			interval[5][2];
	int				count[5];
	ded_soundid_t	ambient_sound;
	float			sound_interval[2];	// min,max
	float			texmove_angle[2];	// floor, ceil
	float			texmove_speed[2];	// floor, ceil
	float			wind_angle;
	float			wind_speed;
	float			vertical_wind;
	float			gravity;
	float			friction;
	ded_func_t		lightfunc;
	int				light_interval[2];
	ded_func_t		colfunc[3];	// RGB
	int				col_interval[3][2];
	ded_func_t		floorfunc;
	float			floormul, flooroff;
	int				floor_interval[2];
	ded_func_t		ceilfunc;
	float			ceilmul, ceiloff;
	int				ceil_interval[2];
} ded_sectortype_t;

typedef struct
{
	ded_string_t	wall;			// Name of a wall texture.
	ded_string_t	flat;			// Name of a flat.
	ded_string_t	detail_lump;	// The lump with the detail texture.
	float			scale;
	float			strength;
	float			maxdist;
} ded_detailtexture_t;

typedef struct
{
	ded_string_t	type;
	int				tics;
	float			variance;		// Stage variance (time).
	float			color[4];		// RGBA
	float			radius;
	float			radius_variance;
	ded_flags_t		flags;
	float			bounce;
	float			resistance;		// Air resistance.
	float			gravity;
} ded_ptcstage_t;

typedef struct
{
	ded_stateid_t	state;			// Triggered by this state (if mobj-gen).
	ded_string_t	flat;			// Triggered by this flat.
	int				flat_num;
	ded_mobjid_t	type;			// Triggered by this type of mobjs.
	ded_mobjid_t	type2;			// Also triggered by this type.
	int				type_num;
	int				type2_num;
	ded_mobjid_t	damage;			// Triggered by mobj damage of this type.
	int				damage_num;
	ded_string_t	map;			// Triggered by this map (or empty string).
	ded_flags_t		flags_string;
	int				flags;
	float			speed;			// Particle spawn velocity.
	float			spd_variance;	// Spawn speed variance (0-1).
	float			vector[3];		// Particle launch vector.
	float			vec_variance;	// Launch vector variance (0-1). 1=totally random.
	float			center[3];		// Offset to the mobj (relat. to source).
	float			min_spawn_radius; // Spawn uncertainty box.
	float			spawn_radius;	
	float			maxdist;		// Max visibility for particles.
	int				spawn_age;		// How long until spawning stops?
	int				max_age;		// How long until generator dies?
	int				particles;		// Maximum number of particles.
	float			spawn_rate;		// Particles spawned per tic.
	float			spawn_variance;
	int				presim;			// Tics to pre-simulate when spawned.
	int				alt_start;
	float			alt_variance;	// Probability for alt start.
	float			force;			// Radial strength of the sphere force.
	float			force_radius;	// Radius of the sphere force.
	float			force_axis[3];	// Rotation axis of the sphere force (+ speed).
	float			force_origin[3]; // Offset for the force sphere.
	ded_ptcstage_t	stages[DED_PTC_STAGES];
} ded_ptcgen_t;

typedef struct
{
	float			pos[2];			// Coordinates on the surface.
	float			elevation;		// Distance from the surface.
	float			color[3];		// Light color.
	float			radius;			// Dynamic light radius (-1 = no light).
	float			halo_radius;	// Halo radius (zero = no halo).
	int				pattern_offset[2];
	int				pattern_skip[2]; 
	int				light_levels[2];// Fade by sector lightlevel.
	int				flare_texture;
	//ded_string_t	texture;		// DL map to use (later...).
} ded_decorlight_t;

// There is a fixed number of light decorations in each decoration.
#define DED_DECOR_NUM_LIGHTS	8

typedef struct ded_decor_s
{
	ded_string_t	surface;		// Texture or flat name.
	int				is_texture;		// True, if decoration for a wall.
	ded_flags_t		flags_str;		// Flags as text.
	int				flags;			// Evaluated flags.
	int				surface_index;	// Flat or texture index.
	ded_decorlight_t lights[DED_DECOR_NUM_LIGHTS];
} ded_decor_t;

// The ded_t structure encapsulates all the data one definition file 
// can contain. This is only an internal representation of the data.
// An ASCII version is written and read by DED_Write() and DED_Read() 
// routines.

// It is VERY important not to sort the data arrays in any way: the
// index numbers are important. The Game DLL must be recompiled with the
// new constants if the order of the array items changes.

typedef struct ded_s
{
	int			version;			// DED version number.
	/*
	char		mobj_prefix[10];	// Prefix for mobj type constants.
	char		state_prefix[10];	// Prefix for state constants.
	char		sprite_prefix[10];	// Prefix for sprite constants.
	*/
	char		model_path[256];	// Directories for searching MD2s.
	ded_flags_t	model_flags;		// Default values for models.
	float		model_scale;		
	float		model_offset;		
	/*
	char		sfx_prefix[10];		// Prefix for sfx constants.
	char		mus_prefix[10];		// Prefix for mus constants.
	char		text_prefix[10];	// Prefix for text constants.
	*/

	struct ded_counts_s {
		//ded_count_t includes;
		ded_count_t	flags;
		ded_count_t	mobjs;
		ded_count_t	states;
		ded_count_t	sprites;
		ded_count_t	lights;
		ded_count_t	models;
		ded_count_t	sounds;
		ded_count_t	music;
		ded_count_t	mapinfo;
		ded_count_t	text;
		ded_count_t	tenviron;
		ded_count_t	values;
		ded_count_t	details;
		ded_count_t	ptcgens;
		ded_count_t	finales;
		ded_count_t decorations;
		ded_count_t	lines;
		ded_count_t	sectors;
	} count;

	// Include other DED files.
	//ded_path_t	*includes;

	// Flag values (for all types of data).
	ded_flag_t	*flags;

	// Map object information.
	ded_mobj_t	*mobjs;

	// States.
	ded_state_t	*states;

	// Sprites.
	ded_sprid_t	*sprites;

	// Lights.
	ded_light_t *lights;

	// Models.
	ded_model_t	*models;

	// Sounds.
	ded_sound_t	*sounds;

	// Music.
	ded_music_t	*music;

	// Map information.
	ded_mapinfo_t *mapinfo;

	// Text.
	ded_text_t	*text;

	// Aural environments for textures.
	ded_tenviron_t *tenviron;

	// Free-from string values.
	ded_value_t	*values;

	// Detail texture assignments.
	ded_detailtexture_t *details;

	// Particle generators.
	ded_ptcgen_t *ptcgens;

	// Finales.
	ded_finale_t *finales;

	// Decorations.
	ded_decor_t *decorations;
	
	// XG line types.
	ded_linetype_t *lines;

	// XG sector types.
	ded_sectortype_t *sectors;
} ded_t;

// Routines for managing DED files.
void DED_Init(ded_t *ded);
void DED_Destroy(ded_t *ded);
int DED_Read(ded_t *ded, const char *sPathName);
int DED_ReadLump(ded_t *ded, int lump);

//int DED_AddInclude(ded_t *ded, char *inc);
int DED_AddFlag(ded_t *ded, char *name, int value);
int DED_AddMobj(ded_t *ded, char *idstr);
int DED_AddState(ded_t *ded, char *id);
int DED_AddSprite(ded_t *ded, const char *name);
int DED_AddLight(ded_t *ded, const char *stateid);
int DED_AddModel(ded_t *ded, char *spr);
int DED_AddSound(ded_t *ded, char *id);
int DED_AddMusic(ded_t *ded, char *id);
int DED_AddMapInfo(ded_t *ded, char *str);
int DED_AddText(ded_t *ded, char *id);
int DED_AddTexEnviron(ded_t *ded, char *id);
int DED_AddValue(ded_t *ded, const char *id);
int DED_AddDetail(ded_t *ded, const char *lumpname);
int DED_AddPtcGen(ded_t *ded, const char *state);
int DED_AddFinale(ded_t *ded);
int DED_AddDecoration(ded_t *ded);
int DED_AddSector(ded_t *ded, int id);
int DED_AddLine(ded_t *ded, int id);

//void DED_RemoveInclude(ded_t *ded, int index);
void DED_RemoveFlag(ded_t *ded, int index);
void DED_RemoveMobj(ded_t *ded, int index);
void DED_RemoveState(ded_t *ded, int index);
void DED_RemoveSprite(ded_t *ded, int index);
void DED_RemoveLight(ded_t *ded, int index);
void DED_RemoveModel(ded_t *ded, int index);
void DED_RemoveSound(ded_t *ded, int index);
void DED_RemoveMusic(ded_t *ded, int index);
void DED_RemoveMapInfo(ded_t *ded, int index);
void DED_RemoveText(ded_t *ded, int index);
void DED_RemoveTexEnviron(ded_t *ded, int index);
void DED_RemoveValue(ded_t *ded, int index);
void DED_RemoveDetail(ded_t *ded, int index);
void DED_RemovePtcGen(ded_t *ded, int index);
void DED_RemoveFinale(ded_t *ded, int index);
void DED_RemoveDecoration(ded_t *ded, int index);
void DED_RemoveSector(ded_t *ded, int index);
void DED_RemoveLine(ded_t *ded, int index);

void *DED_NewEntries(void **ptr, ded_count_t *cnt, int elem_size, int count);
void *DED_NewEntry(void **ptr, ded_count_t *cnt, int elem_size);
void DED_DelEntry(int index, void **ptr, ded_count_t *cnt, int elem_size);
void DED_DelArray(void **ptr, ded_count_t *cnt);
void DED_ZCount(ded_count_t *c);

extern char	dedReadError[];

#ifdef __cplusplus
}
#endif

#endif