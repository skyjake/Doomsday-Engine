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
 * def_data.c: Doomsday Engine Definition Files
 *
 * FIXME: Needs to be redesigned.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def_data.h"

// Helper Routines -------------------------------------------------------

// Returns a pointer to the new block of memory.
void *DED_NewEntries(void **ptr, ded_count_t *cnt, int elem_size, int count)
{
	void *np;

	cnt->num += count;
	if(cnt->num > cnt->max) 
	{
		cnt->max *= 2; // Double the size of the array.
		if(cnt->num > cnt->max) cnt->max = cnt->num;
		*ptr = realloc(*ptr, elem_size * cnt->max);
	}
	np = (char*) *ptr + (cnt->num - count) * elem_size;
	memset(np, 0, elem_size * count); // Clear the new entries.
	return np;
}

// Returns a pointer to the new block of memory.
void *DED_NewEntry(void **ptr, ded_count_t *cnt, int elem_size)
{
	return DED_NewEntries(ptr, cnt, elem_size, 1);
}

void DED_DelEntry(int index, void **ptr, ded_count_t *cnt, int elem_size)
{
	if(index < 0 || index >= cnt->num) return;	// Can't do that!
	memmove((char*) *ptr + elem_size*index, 
		(char*) *ptr + elem_size*(index + 1), 
		elem_size * (cnt->num - index - 1));
	if(--cnt->num < cnt->max/2)
	{
		cnt->max /= 2;
		*ptr = realloc(*ptr, elem_size * cnt->max);
	}
}

void DED_DelArray(void **ptr, ded_count_t *cnt)
{
	free(*ptr);
	*ptr = 0;
	cnt->num = cnt->max = 0;
}

void DED_ZCount(ded_count_t *c)
{
	c->num = c->max = 0;
}

// DED Code --------------------------------------------------------------

void DED_Init(ded_t *ded)
{
	memset(ded, 0, sizeof(*ded));
	ded->version = DED_VERSION;
}

void DED_Destroy(ded_t *ded)
{
	int i;

	free(ded->flags);
	free(ded->mobjs);
	free(ded->states);
	free(ded->sprites);
	free(ded->lights);
	free(ded->models);
	free(ded->sounds);
	free(ded->music);
	free(ded->mapinfo);
	for(i = 0; i < ded->count.text.num; i++) 
	{
		free(ded->text[i].text);
	}
	free(ded->text);
	for(i = 0; i < ded->count.tenviron.num; i++) 
	{
		free(ded->tenviron[i].textures);
	}
	free(ded->tenviron);
	for(i = 0; i < ded->count.values.num; i++)
	{
		free(ded->values[i].id);
		free(ded->values[i].text);
	}
	free(ded->values);
	free(ded->decorations);
	free(ded->groups);
	free(ded->sectors);
}

int DED_AddMobj(ded_t *ded, char *idstr)
{
	ded_mobj_t *mo = DED_NewEntry( (void**) &ded->mobjs,
		&ded->count.mobjs, sizeof(ded_mobj_t));
	strcpy(mo->id, idstr);
	return mo - ded->mobjs;
}

void DED_RemoveMobj(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->mobjs, &ded->count.mobjs, 
		sizeof(ded_mobj_t));
}

int DED_AddFlag(ded_t *ded, char *name, int value)
{
	ded_flag_t *fl = DED_NewEntry( (void**) &ded->flags,
		&ded->count.flags, sizeof(ded_flag_t));
	strcpy(fl->id, name);
	fl->value = value;
	return fl - ded->flags;
}

void DED_RemoveFlag(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->flags, &ded->count.flags,
		sizeof(ded_flag_t));
}

int DED_AddModel(ded_t *ded, char *spr)
{
	int i;
	ded_model_t *md = DED_NewEntry( (void**) &ded->models,
		&ded->count.models, sizeof(ded_model_t));

	strcpy(md->sprite.id, spr);
	md->interrange[1] = 1;
	md->scale[0] = md->scale[1] = md->scale[2] = 1;
	for(i = 0; i < 4; i++) // Init submodels, too.
	{
		md->sub[i].shinycolor[0] 
			= md->sub[i].shinycolor[1] 
			= md->sub[i].shinycolor[2] = 1;
	}
	return md - ded->models;
}

void DED_RemoveModel(ded_t *ded, int index)
{
	//free(ded->models[index].frames);
	DED_DelEntry(index, (void**) &ded->models, &ded->count.models, 
		sizeof(ded_model_t));
}

int DED_AddState(ded_t *ded, char *id)
{
	ded_state_t *st = DED_NewEntry( (void**) &ded->states,
		&ded->count.states, sizeof(ded_state_t));
	strcpy(st->id, id);
	return st - ded->states;
}

void DED_RemoveState(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->states, &ded->count.states,
		sizeof(ded_state_t));
}

int DED_AddSprite(ded_t *ded, const char *name)
{
	ded_sprid_t *sp = DED_NewEntry( (void**) &ded->sprites,
		&ded->count.sprites, sizeof(ded_sprid_t));
	strcpy(sp->id, name);
	return sp - ded->sprites;
}

void DED_RemoveSprite(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->sprites, &ded->count.sprites,
		sizeof(ded_sprid_t));
}

int DED_AddLight(ded_t *ded, const char *stateid)
{
	ded_light_t *light = DED_NewEntry( (void**) &ded->lights,
		&ded->count.lights, sizeof(ded_light_t));
	strcpy(light->state, stateid);
	return light - ded->lights;
}

void DED_RemoveLight(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->lights, &ded->count.lights,
		sizeof(ded_light_t));
}

int DED_AddSound(ded_t *ded, char *id)
{
	ded_sound_t *snd = DED_NewEntry( (void**) &ded->sounds,
		&ded->count.sounds, sizeof(ded_sound_t));
	strcpy(snd->id, id);
	return snd - ded->sounds;
}

void DED_RemoveSound(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->sounds, &ded->count.sounds,
		sizeof(ded_sound_t));
}

int DED_AddMusic(ded_t *ded, char *id)
{
	ded_music_t *mus = DED_NewEntry( (void**) &ded->music,
		&ded->count.music, sizeof(ded_music_t));
	strcpy(mus->id, id);
	return mus - ded->music;
}

void DED_RemoveMusic(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->music, &ded->count.music,
		sizeof(ded_music_t));
}

int DED_AddMapInfo(ded_t *ded, char *str)
{
	ded_mapinfo_t *inf = DED_NewEntry( (void**) &ded->mapinfo,
		&ded->count.mapinfo, sizeof(ded_mapinfo_t));
	int i;

	strcpy(inf->id, str);
	inf->gravity = 1;
	inf->sky_height = .666667f;

	for(i = 0; i < NUM_SKY_MODELS; i++)
	{
		inf->sky_models[i].frame_interval = 1;
		inf->sky_models[i].color[0] = 1;
		inf->sky_models[i].color[1] = 1;
		inf->sky_models[i].color[2] = 1;
		inf->sky_models[i].color[3] = 1;
	}

	return inf - ded->mapinfo;
}

void DED_RemoveMapInfo(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->mapinfo, &ded->count.mapinfo,
		sizeof(ded_mapinfo_t));
}

int DED_AddText(ded_t *ded, char *id)
{
	ded_text_t *txt = DED_NewEntry( (void**) &ded->text,
		&ded->count.text, sizeof(ded_text_t));
	strcpy(txt->id, id);
	return txt - ded->text;
}

void DED_RemoveText(ded_t *ded, int index)
{
	free(ded->text[index].text);
	DED_DelEntry(index, (void**) &ded->text, &ded->count.text, 
		sizeof(ded_text_t));
}

int DED_AddTexEnviron(ded_t *ded, char *id)
{
	ded_tenviron_t *env = DED_NewEntry( (void**) &ded->tenviron,
		&ded->count.tenviron, sizeof(ded_tenviron_t));
	strcpy(env->id, id);
	return env - ded->tenviron;
}

void DED_RemoveTexEnviron(ded_t *ded, int index)
{
	free(ded->tenviron[index].textures);
	DED_DelEntry(index, (void**) &ded->tenviron, &ded->count.tenviron,
		sizeof(ded_tenviron_t));
}

int DED_AddValue(ded_t *ded, const char *id)
{
	ded_value_t *val = DED_NewEntry( (void**) &ded->values,
		&ded->count.values, sizeof(ded_value_t));
	if(id)
	{
		val->id = malloc(strlen(id)+1);
		strcpy(val->id, id);
	}
	return val - ded->values;
}

void DED_RemoveValue(ded_t *ded, int index)
{
	free(ded->values[index].id);
	free(ded->values[index].text);
	DED_DelEntry(index, (void**) &ded->values, &ded->count.values,
		sizeof(ded_value_t));
}

int DED_AddDetail(ded_t *ded, const char *lumpname)
{
	ded_detailtexture_t *dtl = DED_NewEntry( (void**) &ded->details,
		&ded->count.details, sizeof(ded_detailtexture_t));
	strcpy(dtl->detail_lump, lumpname);
	dtl->scale = 1;
	dtl->strength = 1;
	return dtl - ded->details;
}

void DED_RemoveDetail(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->details, &ded->count.details,
		sizeof(ded_detailtexture_t));
}

int DED_AddPtcGen(ded_t *ded, const char *state)
{
	ded_ptcgen_t *gen = DED_NewEntry( (void**) &ded->ptcgens,
		&ded->count.ptcgens, sizeof(ded_ptcgen_t));
	int i;

	strcpy(gen->state, state);

	// Default choice (use either submodel zero or one).
	gen->submodel = -1;
	
	for(i = 0; i < DED_PTC_STAGES; i++)
	{
		gen->stages[i].model = -1;
		gen->stages[i].sound.volume = 1;
		gen->stages[i].hit_sound.volume = 1;
	}

	return gen - ded->ptcgens;
}

void DED_RemovePtcGen(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->ptcgens, &ded->count.ptcgens,
		sizeof(ded_ptcgen_t));
}

int DED_AddFinale(ded_t *ded)
{
	ded_finale_t *fin = DED_NewEntry( (void**) &ded->finales,
		&ded->count.finales, sizeof(ded_finale_t));
	return fin - ded->finales;
}

void DED_RemoveFinale(ded_t *ded, int index)
{
	free(ded->finales[index].script);
	DED_DelEntry(index, (void**) &ded->finales, &ded->count.finales,
		sizeof(ded_finale_t));
}

int DED_AddDecoration(ded_t *ded)
{
	ded_decor_t *decor = DED_NewEntry( (void**) &ded->decorations,
		&ded->count.decorations, sizeof(ded_decor_t));
	int i;

	// Init some default values.
	for(i = 0; i < DED_DECOR_NUM_LIGHTS; i++)
	{
		// The color (0,0,0) means the light is not active.
		decor->lights[i].elevation = 1;
		decor->lights[i].radius = 1;
	}

	return decor - ded->decorations;
}

void DED_RemoveDecoration(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->decorations, &ded->count.decorations,
		sizeof(ded_decor_t));
}

int DED_AddGroup(ded_t *ded)
{
	ded_group_t *group = DED_NewEntry( (void**) &ded->groups,
		&ded->count.groups, sizeof(ded_group_t));
	return group - ded->groups;	
}

void DED_RemoveGroup(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->groups, &ded->count.groups,
		sizeof(ded_group_t));
}

int DED_AddSector(ded_t *ded, int id)
{
	ded_sectortype_t *sec = DED_NewEntry( (void**) &ded->sectors,
		&ded->count.sectors, sizeof(ded_sectortype_t));
	sec->id = id;
	return sec - ded->sectors;
}

void DED_RemoveSector(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->sectors, &ded->count.sectors,
		sizeof(ded_sectortype_t));
}

int DED_AddLine(ded_t *ded, int id)
{
	ded_linetype_t *li = DED_NewEntry( (void**) &ded->lines,
		&ded->count.lines, sizeof(ded_linetype_t));
	li->id = id;
	//li->act_count = -1;
	return li - ded->lines;
}

void DED_RemoveLine(ded_t *ded, int index)
{
	DED_DelEntry(index, (void**) &ded->lines, &ded->count.lines,
		sizeof(ded_linetype_t));
}
