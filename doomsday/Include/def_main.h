#ifndef __DOOMSDAY_DEFINITIONS_MAIN_H__
#define __DOOMSDAY_DEFINITIONS_MAIN_H__

#include "dedfile.h"
#include "dd_dfdat.h"

typedef struct
{
	int wall_texture;
	int flat_lump;
	int detail_lump;
	unsigned int gltex;
} detailtex_t;

typedef struct sfxinfo_s
{
	void *data;				// Pointer to sound data.
	int lumpnum;
	char lumpname[9];		// Actual lump name of the sound (full name).
	char id[32];			// Identifier name (from the def).
	char name[32];			// Long name.
	struct sfxinfo_s *link;	// Link to another sound.
	int link_pitch;
	int link_volume;
	int priority;
	int channels;			// Max. channels for the sound to occupy.
	int usefulness;			// Used to determine when to cache out.
	int flags;
	int group;
	char external[256];		// Path to external file.
} sfxinfo_t;

extern ded_t		defs;		// The main definitions database.
extern sprname_t	*sprnames;	// Sprite name list.
extern state_t		*states;	// State list.
extern mobjinfo_t	*mobjinfo;	// Map object info database.
extern sfxinfo_t	*sounds;	// Sound effect list.
extern ddtext_t		*texts;		// Text list.
extern detailtex_t	*details;	// Detail texture mappings.
extern mobjinfo_t	**stateowners;
extern ded_count_t	count_sprnames;
extern ded_count_t	count_states;

// From DD_Setup.c.
extern ded_mapinfo_t *mapinfo;	// Current mapinfo, can be NULL.

void Def_Init(void);
void Def_PostInit(void);

// Destroy databases.
void Def_Destroy(void);

// Reads the specified definition file, and creates the sprite name, 
// state, mobjinfo, sound, music and text databases accordingly.
void Def_Read(void);

void Def_ReadProcessDED(const char *fileName);

int Def_GetStateNum(char *id);
int Def_GetSpriteNum(char *name);
int Def_GetModelNum(const char *id);
int Def_GetMusicNum(char *id);
int Def_EvalFlags(char *ptr);
ded_mapinfo_t *Def_GetMapInfo(char *map_id);
ded_light_t *Def_GetLightDef(int spr, int frame);
ded_decor_t *Def_GetDecoration(int number, boolean is_texture, boolean has_ext);
int Def_Get(int type, char *id, void *out);
boolean Def_SameStateSequence(state_t *snew, state_t *sold);
void Def_LightMapLoaded(const char *id, unsigned int texture);

#endif