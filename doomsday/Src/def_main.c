/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * def_main.c: Definitions Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <ctype.h>

/*
#ifdef WIN32
#	include <io.h>
#endif
*/

#include "de_base.h"
#include "de_defs.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_console.h"
#include "de_audio.h"
#include "de_misc.h"

// XGClass.h is actually a part of the engine.
// It just defines the various LTC_* constants.
#include "Common/xgclass.h"

// MACROS ------------------------------------------------------------------

#define LOOPi(n)	for(i = 0; i < (n); i++)
#define LOOPk(n)	for(k = 0; k < (n); k++)

// TYPES -------------------------------------------------------------------

typedef struct {
	char *name;					// Name of the routine.
	void (C_DECL *func)();		// Pointer to the function.
} actionlink_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Def_ReadProcessDED(const char *filename);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ded_t		defs;		// The main definitions database.
sprname_t	*sprnames;	// Sprite name list.
state_t		*states;	// State list.
mobjinfo_t	*mobjinfo;	// Map object info database.
sfxinfo_t	*sounds;	// Sound effect list.
//musicinfo_t	*music;		// Music list.
ddtext_t	*texts;		// Text list.
detailtex_t	*details;	// Detail texture assignments.
mobjinfo_t	**stateowners; // A pointer for each state.
ded_count_t	count_sprnames;
ded_count_t	count_states;
ded_count_t	count_mobjinfo;
ded_count_t	count_sounds;
//int			num_music;
ded_count_t	count_texts;
ded_count_t	count_details;
ded_count_t	count_stateowners;

boolean		first_ded;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean defsInited = false;
static char *dedFiles[MAX_READ];
static mobjinfo_t *gettingFor;

// CODE --------------------------------------------------------------------

//===========================================================================
// Def_Init
//	Initializes the databases.
//===========================================================================
void Def_Init(void)
{
	extern filename_t defsFileName;
	extern filename_t topDefsFileName;
	int c;

	// Sprite name list.
	sprnames = 0;
	mobjinfo = 0;
	states = 0;
	sounds = 0;
	texts = 0;
	details = 0;
	stateowners = 0;
	DED_ZCount(&count_sprnames);
	DED_ZCount(&count_mobjinfo);
	DED_ZCount(&count_states);
	DED_ZCount(&count_sounds);
	DED_ZCount(&count_texts); 
	DED_ZCount(&count_details);
	DED_ZCount(&count_stateowners);

	DED_Init(&defs);

	// The engine defs.
	dedFiles[0] = defsFileName;

	// Add the default ded. It will be overwritten by -defs.
	dedFiles[c = 1] = topDefsFileName;

	// See which .ded files are specified on the command line.
	if(ArgCheck("-defs"))
	{
		while(c < MAX_READ)
		{
			char *arg = ArgNext();
			if(!arg || arg[0] == '-') break;
			// Add it to the list.
			dedFiles[c++] = arg;
		}
	}

	// How about additional .ded files?
	if(ArgCheckWith("-def", 1))
	{
		// Find the next empty place.
		for(c = 0; dedFiles[c] && c < MAX_READ; c++);
		while(c < MAX_READ)
		{
			char *arg = ArgNext();
			if(!arg || arg[0] == '-') break;
			// Add it to the list.
			dedFiles[c++] = arg;
		}
	}
}

//===========================================================================
// Def_Destroy
//	Destroy databases.
//===========================================================================
void Def_Destroy(void)
{
	// To make sure...
	DED_Destroy(&defs);
	DED_Init(&defs);

	// Destroy the databases.
	DED_DelArray((void**)&sprnames, &count_sprnames);
	DED_DelArray((void**)&states, &count_states);
	DED_DelArray((void**)&mobjinfo, &count_mobjinfo);
	DED_DelArray((void**)&sounds, &count_sounds);
	DED_DelArray((void**)&texts, &count_texts);
	DED_DelArray((void**)&details, &count_details);
	DED_DelArray((void**)&stateowners, &count_stateowners);

	defsInited = false;

	// Clear global variables pointing to definitions.
	mapinfo = NULL;
}

//===========================================================================
// Def_GetSpriteNum
//	Returns the number of the given sprite, or -1 if it doesn't exist.
//===========================================================================
int Def_GetSpriteNum(char *name)
{
	int i;

	for(i = 0; i < count_sprnames.num; i++)
		if(!stricmp(sprnames[i].name, name))
			return i;
	return -1;
}

int Def_GetMobjNum(char *id)
{
	int i;

	if(!id || !id[0]) return -1;

	for(i = 0; i < defs.count.mobjs.num; i++)
		if(!strcmp(defs.mobjs[i].id, id))
			return i;
	return -1;

}

int Def_GetStateNum(char *id)
{
	int i;

	for(i = 0; i < defs.count.states.num; i++)
		if(!strcmp(defs.states[i].id, id))
			return i;
	return -1;
}

int Def_GetModelNum(const char *id)
{
	int i;

	if(!id[0]) return -1;

	for(i = 0; i < defs.count.models.num; i++)
		if(!strcmp(defs.models[i].id, id))
			return i;
	return -1;
}

int Def_GetSoundNum(char *id)
{
	int i;

	if(!id || !id[0]) 
		return -1;

	for(i = 0; i < defs.count.sounds.num; i++)
	{
		if(!strcmp(defs.sounds[i].id, id))
			return i;
	}
	return -1;
}

//===========================================================================
// Def_GetSoundNumForName
//	Looks up a sound using the Name key. If the name is not found, returns
//	the NULL sound index (zero).
//===========================================================================
int Def_GetSoundNumForName(char *name)
{
	int i;

	for(i = 0; i < defs.count.sounds.num; i++)
		if(!stricmp(defs.sounds[i].name, name)) return i;
	return 0;
}

int Def_GetMusicNum(char *id)
{
	int i;

	if(!id || !id[0]) 
		return -1;

	for(i = 0; i < defs.count.music.num; i++)
		if(!strcmp(defs.music[i].id, id))
			return i;
	return -1;
}

acfnptr_t Def_GetActionPtr(char *name)
{
	// Action links are provided by the Game, who owns the actual 
	// action functions.
	actionlink_t *link = (void*) gx.Get(DD_ACTION_LINK);
	
	if(!link) 
		Con_Error("GetActionPtr: Game DLL doesn't have an action function link table.\n");
	for(; link->name; link++)
		if(!strcmp(name, link->name)) 
			return link->func;
	return 0;
}

ded_mapinfo_t *Def_GetMapInfo(char *map_id)
{
	int i;

	for(i = defs.count.mapinfo.num - 1; i >= 0; i--)
		if(!stricmp(defs.mapinfo[i].id, map_id))
			return defs.mapinfo + i;
	return 0;
}

ded_decor_t *Def_GetDecoration(int number, boolean is_texture, boolean has_ext)
{
	ded_decor_t *def;
	int i;

	for(i = defs.count.decorations.num - 1, def = defs.decorations + i; 
		i >= 0; i--, def--)
	{
		if(!def->is_texture == !is_texture && number == def->surface_index)
		{
			// Is this suitable?
			if(R_IsAllowedDecoration(def, number, has_ext))
				return def;
		}
	}
	return 0;
}

int Def_GetFlagValue(char *flag)
{
	int i;

	for(i = defs.count.flags.num - 1; i >= 0; i--)
		if(!strcmp(defs.flags[i].id, flag))
			return defs.flags[i].value;
	Con_Message("Def_GetFlagValue: Undefined flag '%s'.\n", flag);
	return 0;
}

int Def_EvalFlags(char *ptr)
{
	int value = 0, len;
	char buf[64];
	
	while(*ptr)
	{
		ptr = M_SkipWhite(ptr);
		len = M_FindWhite(ptr) - ptr;
		strncpy(buf, ptr, len);
		buf[len] = 0;
		value |= Def_GetFlagValue(buf);
		ptr += len;
	}
	return value;
}

//===========================================================================
// DD_InitTextDef
//	Escape sequences are un-escaped (\n, \r, \t, \s, \_).
//===========================================================================
void Def_InitTextDef(ddtext_t *txt, char *str)
{
	char *out, *in;

	if(!str) str = ""; // Handle null pointers with "".
	txt->text = calloc(strlen(str) + 1, 1);
	for(out = txt->text, in = str; *in; out++, in++)
	{
		if(*in == '\\') 
		{
			in++;
			if(*in == 'n') *out = '\n';			// Newline.
			else if(*in == 'r') *out = '\r';	// Carriage return.
			else if(*in == 't') *out = '\t';	// Tab.
			else if(*in == '_' || *in == 's') *out = ' '; // Space.
			else *out = *in;
			continue;
		}
		*out = *in;
	}	
	// Adjust buffer to fix exactly.
	txt->text = realloc(txt->text, strlen(txt->text) + 1);
}

//===========================================================================
// Def_ReadDEDFile
//	Callback for DD_ReadProcessDED.
//===========================================================================
int Def_ReadDEDFile(const char *fn, filetype_t type, void *parm)
{
	// Skip directories.
	if(type == FT_DIRECTORY) return true;

	if(M_CheckFileID(fn))
	{
		if(!DED_Read(&defs, fn))
		{
			// Damn.
			Con_Error("Def_ReadDEDFile: %s\n", dedReadError);
		}	
		else if(verbose) 
		{
			Con_Message("DED done: %s\n", M_Pretty(fn));
		}
	}
	// Continue processing files.
	return true;
}

//===========================================================================
// Def_ReadProcessDED
//===========================================================================
void Def_ReadProcessDED(const char *fileName)
{
	filename_t fn, fullFn;
	directory_t dir;

	Dir_FileName(fileName, fn);

	// We want an absolute path.
	if(!Dir_IsAbsolute(fileName))
	{
		Dir_FileDir(fileName, &dir);
		sprintf(fullFn, "%s%s", dir.path, fn);
	}
	else
	{
		strcpy(fullFn, fileName);
	}

	if(strchr(fn, '*') || strchr(fn, '?'))
	{
		// Wildcard search.
		F_ForAll(fullFn, 0, Def_ReadDEDFile);
	}
	else
	{
		Def_ReadDEDFile(fullFn, FT_NORMAL, 0);
	}
}

//===========================================================================
// Def_CountMsg
//	Prints a count with a 2-space indentation.
//===========================================================================
void Def_CountMsg(int count, const char *label)
{
	if(!verbose && !count) return; // Don't print zeros if not verbose.
	Con_Message("%5i %s\n", count, label);
}

//===========================================================================
// DD_ReadLumpDefs
//	Reads all DD_DEFNS lumps found in the lumpinfo. 
//===========================================================================
void Def_ReadLumpDefs(void)
{
	int i, c;

	for(i = 0, c = 0; i < numlumps; i++)
		if(!strnicmp(lumpinfo[i].name, "DD_DEFNS", 8))
		{
			c++;
			if(!DED_ReadLump(&defs, i))
			{
				Con_Error("DD_ReadLumpDefs: Parse error when reading "
					"DD_DEFNS from\n  %s.\n", W_LumpSourceFile(i));
			}
		}

	if(c || verbose) 
	{
		Con_Message("ReadLumpDefs: %i definition lump%s read.\n", 
			c, c != 1? "s" : "");
	}
}

//===========================================================================
// Def_StateForMobj
//	Uses gettingFor. Initializes the state-owners information.
//===========================================================================
int Def_StateForMobj(char *state_id)
{
	int num = Def_GetStateNum(state_id);
	int st, count = 16;

	if(num < 0) num = 0;

	// State zero is the NULL state.
	if(num > 0)
	{
		stateowners[num] = gettingFor;
		// Span forward at most 'count' states, or until we hit a
		// state with an owner, or the NULL state.
		for(st = states[num].nextstate; 
			st > 0 && count-- && !stateowners[st]; 
			st = states[st].nextstate)
		{
			stateowners[st] = gettingFor;
		}
	}
	return num;
}

//===========================================================================
// Def_Read
//	Reads the specified definition files, and creates the sprite name, 
//	state, mobjinfo, sound, music, text and mapinfo databases accordingly.
//===========================================================================
void Def_Read(void)
{
	int i, k;

	if(defsInited)
	{
		// We've already initialized the definitions once.
		// Get rid of everything.
		R_ClearModelPath();
		Def_Destroy();
	}	

	first_ded = true;

	// Clear all existing definitions.
	DED_Destroy(&defs);
	DED_Init(&defs);

	for(read_count = 0, i = 0; dedFiles[i]; i++)
	{
		Con_Message("Reading definition file: %s\n", M_Pretty(dedFiles[i]));
		Def_ReadProcessDED(dedFiles[i]);
	}

	// Read definitions from WAD files.
	Def_ReadLumpDefs();

	// Any definition hooks?
	Plug_DoHook(HOOK_DEFS);

	// Check that enough defs were found.
	if(!defs.count.states.num || !defs.count.mobjs.num)
		Con_Error("DD_ReadDefs: No state or mobj definitions found!\n");
	
	// Sprite names.
	DED_NewEntries((void**)&sprnames, &count_sprnames, sizeof(*sprnames), 
		defs.count.sprites.num);
	for(i = 0; i < count_sprnames.num; i++) 
		strcpy(sprnames[i].name, defs.sprites[i].id);
	Def_CountMsg(count_sprnames.num, "sprite names");

	// States.
	DED_NewEntries((void**)&states, &count_states, sizeof(*states), 
		defs.count.states.num);
	for(i = 0; i < count_states.num; i++)
	{
		ded_state_t *dst = defs.states + i;
		// Make sure duplicate IDs overwrite the earliest.
		state_t *st = states + Def_GetStateNum(dst->id);
		st->sprite = Def_GetSpriteNum(dst->sprite.id);
		st->flags = dst->flags;
		st->frame = dst->frame;
		st->tics = dst->tics;
		st->action = Def_GetActionPtr(dst->action);
		st->nextstate = Def_GetStateNum(dst->nextstate);
		for(k = 0; k < NUM_STATE_MISC; k++) st->misc[k] = dst->misc[k];
	}
	Def_CountMsg(count_states.num, "states");

	DED_NewEntries( (void**) &stateowners, &count_stateowners, 
		sizeof(mobjinfo_t*), defs.count.states.num);

	// Mobj info.
	DED_NewEntries((void**)&mobjinfo, &count_mobjinfo, sizeof(*mobjinfo),
				   defs.count.mobjs.num);
	for(i = 0; i < count_mobjinfo.num; i++)
	{
		ded_mobj_t *dmo = defs.mobjs + i;
		// Make sure duplicate defs overwrite the earliest.
		mobjinfo_t *mo = mobjinfo + Def_GetMobjNum(dmo->id);
		gettingFor = mo;
		mo->doomednum = dmo->doomednum;
		mo->spawnstate = Def_StateForMobj(dmo->spawnstate);
		mo->seestate = Def_StateForMobj(dmo->seestate);
		mo->painstate = Def_StateForMobj(dmo->painstate);
		mo->meleestate = Def_StateForMobj(dmo->meleestate);
		mo->missilestate = Def_StateForMobj(dmo->missilestate);
		mo->crashstate = Def_StateForMobj(dmo->crashstate);
		mo->deathstate = Def_StateForMobj(dmo->deathstate);
		mo->xdeathstate = Def_StateForMobj(dmo->xdeathstate);
		mo->raisestate = Def_StateForMobj(dmo->raisestate);
		mo->spawnhealth = dmo->spawnhealth;
		mo->seesound = Def_GetSoundNum(dmo->seesound);
		mo->reactiontime = dmo->reactiontime;
		mo->attacksound = Def_GetSoundNum(dmo->attacksound);
		mo->painchance = dmo->painchance;
		mo->painsound = Def_GetSoundNum(dmo->painsound);
		mo->deathsound = Def_GetSoundNum(dmo->deathsound);
		mo->speed = dmo->speed * FRACUNIT;
		mo->radius = dmo->radius * FRACUNIT;
		mo->height = dmo->height * FRACUNIT;
		mo->mass = dmo->mass;
		mo->damage = dmo->damage;
		mo->activesound = Def_GetSoundNum(dmo->activesound);
		mo->flags = dmo->flags[0];
		mo->flags2 = dmo->flags[1];
		mo->flags3 = dmo->flags[2];
		for(k=0; k<NUM_MOBJ_MISC; k++)
			mo->misc[k] = dmo->misc[k];
	}
	Def_CountMsg(count_mobjinfo.num, "things");
	Def_CountMsg(defs.count.models.num, "models");

	// Dynamic lights. Update the sprite numbers.
	for(i = 0; i < defs.count.lights.num; i++)
	{
		k = Def_GetStateNum(defs.lights[i].state);
		if(k < 0)
		{
			Con_Message("DD_ReadDefs(Lights): Undefined state '%s'.\n", 
				defs.lights[i].state);
			continue;
		}
		states[k].light = defs.lights + i;
		//defs.lights[i].flags = Def_EvalFlags(defs.lights[i].flags_string);
	}
	Def_CountMsg(defs.count.lights.num, "lights");

	// Sound effects.
	DED_NewEntries((void**)&sounds, &count_sounds, sizeof(*sounds), 
		defs.count.sounds.num);
	for(i = 0; i < count_sounds.num; i++)
	{
		ded_sound_t *snd = defs.sounds + i;
		// Make sure duplicate defs overwrite the earliest.
		sfxinfo_t *si = sounds + Def_GetSoundNum(snd->id);
		strcpy(si->id, snd->id);
		strcpy(si->lumpname, snd->lumpname);
		si->lumpnum = W_CheckNumForName(snd->lumpname);
		strcpy(si->name, snd->name);
		k = Def_GetSoundNum(snd->link);
		si->link = (k >= 0? sounds + k : NULL);
		si->link_pitch = snd->link_pitch;
		si->link_volume = snd->link_volume;
		si->priority = snd->priority;
		si->channels = snd->channels;
		si->flags = snd->flags;
		si->group = snd->group;
		strcpy(si->external, snd->ext.path);
	}
	Def_CountMsg(count_sounds.num, "sound effects");

	// Music.
	for(i = 0; i < defs.count.music.num; i++)
	{
		ded_music_t *mus = defs.music + i;
		// Make sure duplicate defs overwrite the earliest.
		ded_music_t *earliest = defs.music + Def_GetMusicNum(mus->id);
		if(earliest == mus) continue;
		strcpy(earliest->lumpname, mus->lumpname);
		strcpy(earliest->path.path, mus->path.path); 
		earliest->cdtrack = mus->cdtrack;
	}
	Def_CountMsg(defs.count.music.num, "songs");

	// Text.
	DED_NewEntries((void**)&texts, &count_texts, sizeof(*texts),
				   defs.count.text.num);
	for(i = 0; i < count_texts.num; i++) 
		Def_InitTextDef(texts + i, defs.text[i].text);
	// Handle duplicate strings.
	for(i = 0; i < count_texts.num; i++)
	{
		if(!texts[i].text) continue;
		for(k = i + 1; k < count_texts.num; k++)
			if(!strcmp(defs.text[i].id, defs.text[k].id) && texts[k].text)
			{
				// Update the earlier string.
				texts[i].text = realloc(texts[i].text, strlen(texts[k].text) + 1);
				strcpy(texts[i].text, texts[k].text);
				// Free the later string, it isn't used (>NUMTEXT).
				free(texts[k].text);
				texts[k].text = 0;
			}
	}
	Def_CountMsg(count_texts.num, "text strings");

	// Particle generators.
	for(i = 0; i < defs.count.ptcgens.num; i++)
	{
		ded_ptcgen_t *pg = defs.ptcgens + i;
		int st = Def_GetStateNum(pg->state);
		if(pg->flat[0])
			pg->flat_num = W_CheckNumForName(pg->flat);
		else
			pg->flat_num = -1;
		pg->type_num = Def_GetMobjNum(pg->type);
		pg->type2_num = Def_GetMobjNum(pg->type2);
		pg->damage_num = Def_GetMobjNum(pg->damage);
		
		// Figure out embedded sound ID numbers.
		for(k = 0; k < DED_PTC_STAGES; k++)
		{
			if(pg->stages[k].sound.name[0])
			{
				pg->stages[k].sound.id 
					= Def_GetSoundNum(pg->stages[k].sound.name);
			}
			if(pg->stages[k].hit_sound.name[0])
			{
				pg->stages[k].hit_sound.id
					= Def_GetSoundNum(pg->stages[k].hit_sound.name);
			}
		}

		if(st <= 0) continue; // Not state triggered, then...

		// Link the definition to the state.
		if(pg->flags & PGF_STATE_CHAIN)
		{
			// Add to the chain.
			pg->state_next = states[st].ptrigger;
			states[st].ptrigger = pg;
		}
		else
		{
			// Make sure the previously built list is unlinked.
			while(states[st].ptrigger)
			{
				ded_ptcgen_t *temp = 
					((ded_ptcgen_t*)states[st].ptrigger)->state_next;
				((ded_ptcgen_t*)states[st].ptrigger)->state_next = NULL;
				states[st].ptrigger = temp;
			}
			states[st].ptrigger = pg;
			pg->state_next = NULL;
		}
	}
	Def_CountMsg(defs.count.ptcgens.num, "particle generators");

	// Detail textures. Initialize later...
	Def_CountMsg(defs.count.details.num, "detail textures");

	// Texture animation groups.
	Def_CountMsg(defs.count.groups.num, "animation groups");

	// Surface decorations.
	/*for(i = 0; i < defs.count.decorations.num; i++)
	{
		ded_decor_t *decor = defs.decorations + i;
		decor->flags = Def_EvalFlags(decor->flags_str);
	}*/
	Def_CountMsg(defs.count.decorations.num, "surface decorations");

	// Other data:
	Def_CountMsg(defs.count.mapinfo.num, "map infos");
	Def_CountMsg(defs.count.finales.num, "finales");
	Def_CountMsg(defs.count.lines.num, "line types");
	Def_CountMsg(defs.count.sectors.num, "sector types");

	// Init the base model search path (prepend).
	R_AddModelPath(defs.model_path, false);
	// Model search path specified on the command line?
	if(ArgCheckWith("-modeldir", 1))
	{
		// Prepend to the search list; takes precedence.
		R_AddModelPath(ArgNext(), false);
	}

	defsInited = true;
}

//===========================================================================
// Def_PostInit
//	Initialize definitions that must be initialized when engine init is
//	complete (called from R_Init).
//===========================================================================
void Def_PostInit(void)
{
	int i, k;
	ded_ptcgen_t *gen;
	char name[40];
	modeldef_t *modef;
	ded_ptcstage_t *st;

	// Particle generators: model setup.
	for(i = 0, gen = defs.ptcgens; i < defs.count.ptcgens.num; i++, gen++)
	{
		for(k = 0, st = gen->stages; k < DED_PTC_STAGES; k++, st++)
		{
			if(st->type < PTC_MODEL
				|| st->type >= PTC_MODEL + MAX_PTC_MODELS) 
				continue;
			sprintf(name, "Particle%02i", st->type - PTC_MODEL);
			if(!(modef = R_CheckIDModelFor(name))
				|| modef->sub[0].model <= 0)
			{
				st->model = -1;
				continue;
			}			
			st->model = modef - models;
			st->frame = R_ModelFrameNumForName(modef->sub[0].model, 
				st->frame_name);
			if(st->end_frame_name[0])
			{
				st->end_frame = R_ModelFrameNumForName(modef->sub[0].model, 
					st->end_frame_name);
			}
			else
			{
				st->end_frame = -1;
			}
		}
	}

	DED_DelArray((void**)&details, &count_details);
	DED_NewEntries((void**)&details, &count_details, sizeof(*details), 
		defs.count.details.num);	
	for(i = 0; i < defs.count.details.num; i++)
	{
		details[i].wall_texture = R_CheckTextureNumForName
			(defs.details[i].wall);
		details[i].flat_lump = W_CheckNumForName(defs.details[i].flat);
		details[i].detail_lump = W_CheckNumForName
			(defs.details[i].detail_lump);
		details[i].gltex = 0; // Not loaded.
	}
	
	// Surface decorations.
	for(i = 0; i < defs.count.decorations.num; i++)
	{
		ded_decor_t *decor = defs.decorations + i;
		
		decor->surface_index = -1;
		if(decor->is_texture)
			decor->surface_index = R_CheckTextureNumForName(decor->surface);
		else
			decor->surface_index = W_CheckNumForName(decor->surface);

		decor->pregen_lightmap = 0;
	}

	// Animation groups.
	for(i = 0; i < defs.count.groups.num; i++)
	{
		R_InitAnimGroup(defs.groups + i);
	}
}

//===========================================================================
// Def_SetLightMap
//===========================================================================
void Def_SetLightMap
	(ded_lightmap_t *map, const char *id, unsigned int texture)
{
	if(stricmp(map->id, id)) return; // Not the same lightmap?
	map->tex = texture;
}

//===========================================================================
// Def_LightMapLoaded
//===========================================================================
void Def_LightMapLoaded(const char *id, unsigned int texture)
{
	int i, k;
	ded_decor_t *decor;

	// Load lightmaps.
	for(i = 0; i < defs.count.lights.num; i++)
	{
		Def_SetLightMap(&defs.lights[i].up, id, texture);
		Def_SetLightMap(&defs.lights[i].down, id, texture);
		Def_SetLightMap(&defs.lights[i].sides, id, texture);
	}
	for(i = 0, decor = defs.decorations; i < defs.count.decorations.num; 
		i++, decor++)
	{
		for(k = 0; k < DED_DECOR_NUM_LIGHTS; k++)
		{
			if(!R_IsValidLightDecoration(&decor->lights[k]))
				break;
			Def_SetLightMap(&decor->lights[k].up, id, texture);
			Def_SetLightMap(&decor->lights[k].down, id, texture);
			Def_SetLightMap(&decor->lights[k].sides, id, texture);
		}
	}
}

//==========================================================================
// Def_SameStateSequence
//	Can we reach 'snew' if we start searching from 'sold'?
//	Take a maximum of 16 steps.
//==========================================================================
boolean Def_SameStateSequence(state_t *snew, state_t *sold)
{
	int it, target = snew - states, start = sold - states;
	int count = 0;

	if(!snew || !sold) return false;
	if(snew == sold) return true; // Trivial.
	for(it = sold->nextstate; it >= 0 && it != start && count < 16; 
		it = states[it].nextstate, count++)
	{
		if(it == target) return true;
		if(it == states[it].nextstate) break;
	}
	return false;
}

#if 0
//===========================================================================
// DD_HasMobjState
//	Returns true if the mobj (in mobjinfo) has the given state.
//===========================================================================
boolean DD_HasMobjState(int mobj_num, int state_num)
{
	mobjinfo_t *mo = mobjinfo + mobj_num;
	state_t *target = states + state_num;
	int i, statelist[9] =
	{
		mo->spawnstate,
		mo->seestate,
		mo->painstate,
		mo->meleestate,
		mo->missilestate,
		mo->crashstate,
		mo->deathstate,
		mo->xdeathstate,
		mo->raisestate
	};

	for(i = 0; i < 8; i++)
		if(statelist[i] > 0
			&& DD_SameStateSequence(target, states + statelist[i]))
			return true;
	// Not here...
	return false;
}
#endif

static int Friendly(int num)
{
	if(num < 0) num = 0;
	return num;
}

//===========================================================================
// Def_CopyLineType
//	Converts a DED line type to the internal format.
//	Bit of a nuisance really...
//===========================================================================
void Def_CopyLineType(linetype_t *l, ded_linetype_t *def)
{
#define MAP_SND		0x01000000
#define MAP_MUS		0x02000000
#define MAP_TEX		0x04000000
#define MAP_FLAT	0x08000000
#define MAP_MASK	0x00ffffff
	struct 
	{
		int lclass;
		int map[20];
	} 
	mappings[] =
	{
		{ LTC_CHAIN_SEQUENCE, {0,0,-1} },
		{ LTC_PLANE_MOVE, {0,2,3,4|MAP_SND,5|MAP_SND,6|MAP_SND,7,8|MAP_FLAT,9,
			10|MAP_FLAT,11,13,-1} },
		{ LTC_BUILD_STAIRS, {0,4|MAP_SND,5|MAP_SND,6|MAP_SND,7|MAP_SND,-1} },
		{ LTC_SECTOR_TYPE, {0,-1} },
		{ LTC_SECTOR_LIGHT, {0,4,6,-1} },
		{ LTC_LINE_TYPE, {0,-1} },
		{ LTC_ACTIVATE, {0,-1} },
		{ LTC_MUSIC, {0|MAP_MUS,-1} },
		{ LTC_LINE_COUNT, {0,-1} },
		{ LTC_DISABLE_IF_ACTIVE, {0,-1} },
		{ LTC_ENABLE_IF_ACTIVE, {0,-1} },
		{ LTC_PLANE_TEXTURE, {0,2,3|MAP_FLAT,-1} },
		{ LTC_WALL_TEXTURE, {0,3|MAP_TEX,4|MAP_TEX,5|MAP_TEX,-1} },
		{ LTC_SOUND, {0,2|MAP_SND,-1} },
		{ LTC_MIMIC_SECTOR, {0,2,-1} },
		{ -1 }		
	};
	int i, k, a, n;

	l->id = def->id;
	l->flags = def->flags[0];
	l->flags2 = def->flags[1];
	l->flags3 = def->flags[2];
	l->line_class = def->line_class;
	l->act_type = def->act_type;
	l->act_count = def->act_count;
	l->act_time = def->act_time;
	l->act_tag = def->act_tag;
	for(i = 0; i < 10; i++)
	{
		if(i == 9)
			l->aparm[i] = Def_GetMobjNum(def->aparm9);
		else
			l->aparm[i] = def->aparm[i];
	}
	l->ticker_start = def->ticker_start;
	l->ticker_end = def->ticker_end;
	l->ticker_interval = def->ticker_interval;
	l->act_sound = Friendly(Def_GetSoundNum(def->act_sound));
	l->deact_sound = Friendly(Def_GetSoundNum(def->deact_sound));
	l->ev_chain = def->ev_chain;
	l->act_chain = def->act_chain;
	l->deact_chain = def->deact_chain;
	l->wallsection = def->wallsection;
	l->act_tex = Friendly(R_CheckTextureNumForName(def->act_tex));
	l->deact_tex = Friendly(R_CheckTextureNumForName(def->deact_tex));
	l->act_msg = def->act_msg;
	l->deact_msg = def->deact_msg;
	l->texmove_angle = def->texmove_angle;
	l->texmove_speed = def->texmove_speed;
	memcpy(l->iparm, def->iparm, sizeof(int)*20);
	memcpy(l->fparm, def->fparm, sizeof(int)*20);
	LOOPi(5) l->sparm[i] = def->sparm[i];

	// Some of the parameters might be strings depending on the line class.
	// Find the right mapping table.
	for(i=0; mappings[i].lclass >= 0; i++)
	{
		if(mappings[i].lclass != l->line_class) continue;
		for(k=0; k<20; k++)
		{
			a = mappings[i].map[k];
			n = a & MAP_MASK;
			if(a < 0) break;
			if(a & MAP_SND)
			{
				l->iparm[n] = Friendly(Def_GetSoundNum(def->iparm_str[n]));
			}
			else if(a & MAP_TEX)
			{
				l->iparm[n] = Friendly(
					R_CheckTextureNumForName(def->iparm_str[n]));
			}
			else if(a & MAP_FLAT)
			{
				if(def->iparm_str[n][0])
					l->iparm[n] = Friendly(W_CheckNumForName(def->iparm_str[n]));
			}
			else if(a & MAP_MUS)
			{
				l->iparm[n] = Friendly(Def_GetMusicNum(def->iparm_str[n]));
			}
			else
			{
				l->iparm[n] = Def_EvalFlags(def->iparm_str[n]);
			}
		}
		break;
	}
}

//===========================================================================
// Def_CopySectorType
//	Converts a DED sector type to the internal format.
//===========================================================================
void Def_CopySectorType(sectortype_t *s, ded_sectortype_t *def)
{
	int i, k;

	s->id = def->id;
	s->flags = def->flags;
	s->act_tag = def->act_tag;
	LOOPi(5)
	{
		s->chain[i] = def->chain[i];
		s->chain_flags[i] = def->chain_flags[i];
		s->start[i] = def->start[i];
		s->end[i] = def->end[i];
		LOOPk(2) s->interval[i][k] = def->interval[i][k];
		s->count[i] = def->count[i];
	}
	s->ambient_sound = Friendly(Def_GetSoundNum(def->ambient_sound));
	LOOPi(2) 
	{
		s->sound_interval[i] = def->sound_interval[i];
		s->texmove_angle[i] = def->texmove_angle[i];
		s->texmove_speed[i] = def->texmove_speed[i];
	}
	s->wind_angle = def->wind_angle;
	s->wind_speed = def->wind_speed;
	s->vertical_wind = def->vertical_wind;
	s->gravity = def->gravity;
	s->friction = def->friction;
	s->lightfunc = def->lightfunc;
	LOOPi(2) s->light_interval[i] = def->light_interval[i];
	LOOPi(3)
	{
		s->colfunc[i] = def->colfunc[i];
		LOOPk(2) s->col_interval[i][k] = def->col_interval[i][k];
	}
	s->floorfunc = def->floorfunc;
	s->floormul = def->floormul;
	s->flooroff = def->flooroff;
	LOOPi(2) s->floor_interval[i] = def->floor_interval[i];
	s->ceilfunc = def->ceilfunc;
	s->ceilmul = def->ceilmul;
	s->ceiloff = def->ceiloff;
	LOOPi(2) s->ceil_interval[i] = def->ceil_interval[i];
}

//===========================================================================
// Def_Get
//	Returns true if the definition was found.
//===========================================================================
int Def_Get(int type, char *id, void *out)
{
	int				i;
	ded_mapinfo_t	*map;
	ddmapinfo_t		*mout;
	finalescript_t	*fin;

	switch(type)
	{
	case DD_DEF_MOBJ:
		return Def_GetMobjNum(id);

	case DD_DEF_STATE:
		return Def_GetStateNum(id);

	case DD_DEF_SPRITE:
		return Def_GetSpriteNum(id);

	case DD_DEF_SOUND:
		return Def_GetSoundNum(id);

	case DD_DEF_SOUND_BY_NAME:
		return Def_GetSoundNumForName(id);

	case DD_DEF_SOUND_LUMPNAME:
		i = (int) id;
		if(i < 0 || i >= count_sounds.num) return false;
		strcpy(out, sounds[i].lumpname);
		break;

	case DD_DEF_MUSIC:
		return Def_GetMusicNum(id);

	case DD_DEF_MAP_INFO:
		map = Def_GetMapInfo(id);
		if(!map) return false;
		mout = (ddmapinfo_t*) out;	
		mout->name = map->name;
		mout->author = map->author;
		mout->music = Def_GetMusicNum(map->music);
		mout->flags = map->flags;
		mout->ambient = map->ambient;
		mout->gravity = map->gravity;
		mout->partime = map->partime;
		break;

	case DD_DEF_TEXT:
		for(i = 0; i < defs.count.text.num; i++)
			if(!stricmp(defs.text[i].id, id))
			{
				if(out) *(char**)out = defs.text[i].text;
				return i;
			}
		return -1;

	case DD_DEF_VALUE:
		// Read backwards to allow patching.
		for(i = defs.count.values.num - 1; i >= 0; i--)
			if(!stricmp(defs.values[i].id, id))
			{
				if(out) *(char**)out = defs.values[i].text;
				return true;
			}
		return false;

	case DD_DEF_FINALE:
		// Find InFine script by ID.
		for(i = defs.count.finales.num - 1; i >= 0; i--)
			if(!stricmp(defs.finales[i].id, id))
			{
				// This has a matching ID. Return a pointer to the script.
				*(void**) out = defs.finales[i].script;
				return true;
			}		
		return false;

	case DD_DEF_FINALE_BEFORE:
		fin = (finalescript_t*) out;
		for(i = defs.count.finales.num - 1; i >= 0; i--)
			if(!stricmp(defs.finales[i].before, id))
			{
				fin->before = defs.finales[i].before;
				fin->after = defs.finales[i].after;
				fin->script = defs.finales[i].script;
				return true;
			}
		return false;

	case DD_DEF_FINALE_AFTER:
		fin = (finalescript_t*) out;
		for(i = defs.count.finales.num - 1; i >= 0; i--)
			if(!stricmp(defs.finales[i].after, id))
			{
				
				fin->before = defs.finales[i].before;
				fin->after = defs.finales[i].after;
				fin->script = defs.finales[i].script;
				return true;
			}
		return false;

	case DD_DEF_LINE_TYPE:
		for(i = defs.count.lines.num - 1; i >= 0; i--)
			if(defs.lines[i].id == (int) id)
			{
				if(out) Def_CopyLineType(out, &defs.lines[i]);
				return true;
			}
		return false;

	case DD_DEF_SECTOR_TYPE:
		for(i = defs.count.sectors.num - 1; i >= 0; i--)
			if(defs.sectors[i].id == (int) id)
			{
				if(out) Def_CopySectorType(out, &defs.sectors[i]);
				return true;
			}
		return false;

	default:
		return false;
	}
	return true;
}

//===========================================================================
// Def_Set
//	This is supposed to be the main interface for outside parties to 
//	modify definitions (unless they want to do it manually with dedfile.h).
//===========================================================================
int Def_Set(int type, int index, int value, void *ptr)
{
	ded_music_t *musdef;
	int i;

	switch(type)
	{
	case DD_DEF_SOUND:
		if(index < 0 || index >= count_sounds.num)
			Con_Error("Def_Set: Sound index %i is invalid.\n", index);
		switch(value)
		{
		case DD_LUMP:
			S_StopSound(index, 0);
			strcpy(sounds[index].lumpname, ptr);
			sounds[index].lumpnum = W_CheckNumForName(sounds[index].lumpname);
			break;
		}
		break;

	case DD_DEF_MUSIC:
		if(index == DD_NEW)
		{
			// We should create a new music definition.
			i = DED_AddMusic(&defs, ""); // No ID is known at this stage.
			musdef = defs.music + i;
		}
		else if(index >= 0 && index < defs.count.music.num)
		{
			musdef = defs.music + index;
		}
		else Con_Error("Def_Set: Music index %i is invalid.\n", index);
		// Which key to set?
		switch(value)
		{
		case DD_ID:
			if(ptr) strcpy(musdef->id, ptr);
			break;

		case DD_LUMP:
			if(ptr) strcpy(musdef->lumpname, ptr);
			break;

		case DD_CD_TRACK:
			musdef->cdtrack = (int) ptr;
			break;
		}
		// If the def was just created, return its index.
		if(index == DD_NEW) return musdef - defs.music;
		break;

	default:
		return false;
	}
	return true;
}
