#ifndef __DOOMSDAY_DEFINITIONS_H__
#define __DOOMSDAY_DEFINITIONS_H__

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
	char name[32];			// Long name.
	struct sfxinfo_s *link;	// Link to another sound.
	int link_pitch;
	int link_volume;
	int priority;
	int channels;			// Max. channels for the sound to occupy.
	int usefulness;			// Used to determine when to cache out.
	int flags;
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

// Destroy databases.
void Def_Destroy(void);

// Reads the specified definition file, and creates the sprite name, 
// state, mobjinfo, sound, music and text databases accordingly.
void Def_Read(void);

void Def_PostInit(void);

int Def_GetStateNum(char *id);
int Def_GetSpriteNum(char *name);
int Def_GetMusicNum(char *id);
int Def_EvalFlags(char *ptr);
ded_mapinfo_t *Def_GetMapInfo(char *map_id);
ded_light_t *Def_GetLightDef(int spr, int frame);
int Def_Get(int type, char *id, void *out);
boolean Def_SameStateSequence(state_t *snew, state_t *sold);

#endif