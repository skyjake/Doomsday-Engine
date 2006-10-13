/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * def_data.c: Doomsday Engine Definition Files
 *
 * FIXME: Needs to be redesigned.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "de_misc.h"

#include "def_data.h"
#include "gl_main.h"

// Helper Routines -------------------------------------------------------

// Returns a pointer to the new block of memory.
void *DED_NewEntries(void **ptr, ded_count_t * cnt, int elem_size, int count)
{
    void   *np;

    cnt->num += count;
    if(cnt->num > cnt->max)
    {
        cnt->max *= 2;          // Double the size of the array.
        if(cnt->num > cnt->max)
            cnt->max = cnt->num;
        *ptr = M_Realloc(*ptr, elem_size * cnt->max);
    }
    np = (char *) *ptr + (cnt->num - count) * elem_size;
    memset(np, 0, elem_size * count);   // Clear the new entries.
    return np;
}

// Returns a pointer to the new block of memory.
void *DED_NewEntry(void **ptr, ded_count_t * cnt, int elem_size)
{
    return DED_NewEntries(ptr, cnt, elem_size, 1);
}

void DED_DelEntry(int index, void **ptr, ded_count_t * cnt, int elem_size)
{
    if(index < 0 || index >= cnt->num)
        return;                 // Can't do that!
    memmove((char *) *ptr + elem_size * index,
            (char *) *ptr + elem_size * (index + 1),
            elem_size * (cnt->num - index - 1));
    if(--cnt->num < cnt->max / 2)
    {
        cnt->max /= 2;
        *ptr = M_Realloc(*ptr, elem_size * cnt->max);
    }
}

void DED_DelArray(void **ptr, ded_count_t * cnt)
{
    M_Free(*ptr);
    *ptr = 0;
    cnt->num = cnt->max = 0;
}

void DED_ZCount(ded_count_t * c)
{
    c->num = c->max = 0;
}

// DED Code --------------------------------------------------------------

void DED_Init(ded_t * ded)
{
    memset(ded, 0, sizeof(*ded));
    ded->version = DED_VERSION;
}

void DED_Destroy(ded_t * ded)
{
    int     i;

    M_Free(ded->flags);
    M_Free(ded->mobjs);
    M_Free(ded->states);
    M_Free(ded->sprites);
    M_Free(ded->lights);
    M_Free(ded->models);
    M_Free(ded->sounds);
    M_Free(ded->music);
    M_Free(ded->mapinfo);
    for(i = 0; i < ded->count.text.num; i++)
    {
        M_Free(ded->text[i].text);
    }
    M_Free(ded->text);
    for(i = 0; i < ded->count.tenviron.num; i++)
    {
        M_Free(ded->tenviron[i].textures);
    }
    M_Free(ded->tenviron);
    for(i = 0; i < ded->count.values.num; i++)
    {
        M_Free(ded->values[i].id);
        M_Free(ded->values[i].text);
    }
    M_Free(ded->values);
    M_Free(ded->decorations);
    for(i = 0; i < ded->count.groups.num; i++)
    {
        M_Free(ded->groups[i].members);
    }
    M_Free(ded->groups);
    M_Free(ded->sectors);
    for(i = 0; i < ded->count.ptcgens.num; i++)
    {
        M_Free(ded->ptcgens[i].stages);
    }
    M_Free(ded->ptcgens);
    M_Free(ded->finales);

    for(i = 0; i < ded->count.xgclasses.num; i++)
    {
        M_Free(ded->xgclasses[i].properties);
    }
    M_Free(ded->xgclasses);

    for(i = 0; i < ded->count.lumpformats.num; i++)
    {
        M_Free(ded->lumpformats[i].members);
    }
    M_Free(ded->lumpformats);
}

int DED_AddMobj(ded_t * ded, char *idstr)
{
    ded_mobj_t *mo = DED_NewEntry((void **) &ded->mobjs,
                                  &ded->count.mobjs, sizeof(ded_mobj_t));

    strcpy(mo->id, idstr);
    return mo - ded->mobjs;
}

void DED_RemoveMobj(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->mobjs, &ded->count.mobjs,
                 sizeof(ded_mobj_t));
}

int DED_AddXGClass(ded_t * ded)
{
    ded_xgclass_t *xgc = DED_NewEntry((void **) &ded->xgclasses,
                                        &ded->count.xgclasses,
                                        sizeof(ded_xgclass_t));
    return xgc - ded->xgclasses;
}

int DED_AddXGClassProperty(ded_xgclass_t *xgc)
{
    ded_xgclass_property_t *xgcp = DED_NewEntry((void **) &xgc->properties,
                                         &xgc->properties_count,
                                         sizeof(ded_xgclass_property_t));

    return xgcp - xgc->properties;
}

void DED_RemoveXGClass(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->xgclasses, &ded->count.xgclasses,
                 sizeof(ded_xgclass_t));
}

int DED_AddLumpFormat(ded_t * ded)
{
    ded_lumpformat_t *lf = DED_NewEntry((void **) &ded->lumpformats,
                                        &ded->count.lumpformats,
                                        sizeof(ded_lumpformat_t));

    return lf - ded->lumpformats;
}

int DED_AddLumpFormatMember(ded_lumpformat_t *lmpf)
{
    ded_lumpformat_member_t *lmpfm = DED_NewEntry((void **) &lmpf->members,
                                         &lmpf->members_count,
                                         sizeof(ded_lumpformat_member_t));

    return lmpfm - lmpf->members;
}

void DED_RemoveLumpFormat(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->lumpformats, &ded->count.lumpformats,
                 sizeof(ded_lumpformat_t));
}

int DED_AddFlag(ded_t * ded, char *name, char *text, int value)
{
    ded_flag_t *fl = DED_NewEntry((void **) &ded->flags,
                                  &ded->count.flags, sizeof(ded_flag_t));

    strcpy(fl->id, name);
    strcpy(fl->text, text);
    fl->value = value;
    return fl - ded->flags;
}

void DED_RemoveFlag(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->flags, &ded->count.flags,
                 sizeof(ded_flag_t));
}

int DED_AddModel(ded_t * ded, char *spr)
{
    int     i;
    ded_model_t *md = DED_NewEntry((void **) &ded->models,
                                   &ded->count.models, sizeof(ded_model_t));

    strcpy(md->sprite.id, spr);
    md->interrange[1] = 1;
    md->scale[0] = md->scale[1] = md->scale[2] = 1;
    for(i = 0; i < 4; i++)      // Init submodels, too.
    {
        md->sub[i].shinycolor[0] = md->sub[i].shinycolor[1] =
            md->sub[i].shinycolor[2] = 1;
        md->sub[i].shinyreact = 1.0f;
    }
    return md - ded->models;
}

void DED_RemoveModel(ded_t * ded, int index)
{
    //M_Free(ded->models[index].frames);
    DED_DelEntry(index, (void **) &ded->models, &ded->count.models,
                 sizeof(ded_model_t));
}

int DED_AddState(ded_t * ded, char *id)
{
    ded_state_t *st = DED_NewEntry((void **) &ded->states,
                                   &ded->count.states, sizeof(ded_state_t));

    strcpy(st->id, id);
    return st - ded->states;
}

void DED_RemoveState(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->states, &ded->count.states,
                 sizeof(ded_state_t));
}

int DED_AddSprite(ded_t * ded, const char *name)
{
    ded_sprid_t *sp = DED_NewEntry((void **) &ded->sprites,
                                   &ded->count.sprites, sizeof(ded_sprid_t));

    strcpy(sp->id, name);
    return sp - ded->sprites;
}

void DED_RemoveSprite(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->sprites, &ded->count.sprites,
                 sizeof(ded_sprid_t));
}

int DED_AddLight(ded_t * ded, const char *stateid)
{
    ded_light_t *light = DED_NewEntry((void **) &ded->lights,
                                      &ded->count.lights, sizeof(ded_light_t));

    strcpy(light->state, stateid);
    return light - ded->lights;
}

void DED_RemoveLight(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->lights, &ded->count.lights,
                 sizeof(ded_light_t));
}

int DED_AddSound(ded_t * ded, char *id)
{
    ded_sound_t *snd = DED_NewEntry((void **) &ded->sounds,
                                    &ded->count.sounds, sizeof(ded_sound_t));

    strcpy(snd->id, id);
    return snd - ded->sounds;
}

void DED_RemoveSound(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->sounds, &ded->count.sounds,
                 sizeof(ded_sound_t));
}

int DED_AddMusic(ded_t * ded, char *id)
{
    ded_music_t *mus = DED_NewEntry((void **) &ded->music,
                                    &ded->count.music, sizeof(ded_music_t));

    strcpy(mus->id, id);
    return mus - ded->music;
}

void DED_RemoveMusic(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->music, &ded->count.music,
                 sizeof(ded_music_t));
}

int DED_AddMapInfo(ded_t * ded, char *str)
{
    ded_mapinfo_t *inf = DED_NewEntry((void **) &ded->mapinfo,
                                      &ded->count.mapinfo,
                                      sizeof(ded_mapinfo_t));
    int     i;

    strcpy(inf->id, str);
    inf->gravity = 1;
    inf->sky_height = .666667f;
    inf->partime = -1; // unknown

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

void DED_RemoveMapInfo(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->mapinfo, &ded->count.mapinfo,
                 sizeof(ded_mapinfo_t));
}

int DED_AddText(ded_t * ded, char *id)
{
    ded_text_t *txt = DED_NewEntry((void **) &ded->text,
                                   &ded->count.text, sizeof(ded_text_t));

    strcpy(txt->id, id);
    return txt - ded->text;
}

void DED_RemoveText(ded_t * ded, int index)
{
    M_Free(ded->text[index].text);
    DED_DelEntry(index, (void **) &ded->text, &ded->count.text,
                 sizeof(ded_text_t));
}

int DED_AddTexEnviron(ded_t * ded, char *id)
{
    ded_tenviron_t *env = DED_NewEntry((void **) &ded->tenviron,
                                       &ded->count.tenviron,
                                       sizeof(ded_tenviron_t));

    strcpy(env->id, id);
    return env - ded->tenviron;
}

void DED_RemoveTexEnviron(ded_t * ded, int index)
{
    M_Free(ded->tenviron[index].textures);
    DED_DelEntry(index, (void **) &ded->tenviron, &ded->count.tenviron,
                 sizeof(ded_tenviron_t));
}

int DED_AddValue(ded_t * ded, const char *id)
{
    ded_value_t *val = DED_NewEntry((void **) &ded->values,
                                    &ded->count.values, sizeof(ded_value_t));

    if(id)
    {
        val->id = M_Malloc(strlen(id) + 1);
        strcpy(val->id, id);
    }
    return val - ded->values;
}

void DED_RemoveValue(ded_t * ded, int index)
{
    M_Free(ded->values[index].id);
    M_Free(ded->values[index].text);
    DED_DelEntry(index, (void **) &ded->values, &ded->count.values,
                 sizeof(ded_value_t));
}

int DED_AddDetail(ded_t * ded, const char *lumpname)
{
    ded_detailtexture_t *dtl = DED_NewEntry((void **) &ded->details,
                                            &ded->count.details,
                                            sizeof(ded_detailtexture_t));

    strcpy(dtl->detail_lump.path, lumpname);
    dtl->scale = 1;
    dtl->strength = 1;
    return dtl - ded->details;
}

void DED_RemoveDetail(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->details, &ded->count.details,
                 sizeof(ded_detailtexture_t));
}

int DED_AddPtcGen(ded_t * ded, const char *state)
{
    ded_ptcgen_t *gen = DED_NewEntry((void **) &ded->ptcgens,
                                     &ded->count.ptcgens,
                                     sizeof(ded_ptcgen_t));

    strcpy(gen->state, state);

    // Default choice (use either submodel zero or one).
    gen->submodel = -1;

    return gen - ded->ptcgens;
}

int DED_AddPtcGenStage(ded_ptcgen_t *gen)
{
    ded_ptcstage_t *stage = DED_NewEntry((void **) &gen->stages,
                                         &gen->stage_count,
                                         sizeof(ded_ptcstage_t));

    stage->model = -1;
    stage->sound.volume = 1;
    stage->hit_sound.volume = 1;

    return stage - gen->stages;
}

void DED_RemovePtcGen(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->ptcgens, &ded->count.ptcgens,
                 sizeof(ded_ptcgen_t));
}

int DED_AddFinale(ded_t * ded)
{
    ded_finale_t *fin = DED_NewEntry((void **) &ded->finales,
                                     &ded->count.finales,
                                     sizeof(ded_finale_t));

    return fin - ded->finales;
}

void DED_RemoveFinale(ded_t * ded, int index)
{
    M_Free(ded->finales[index].script);
    DED_DelEntry(index, (void **) &ded->finales, &ded->count.finales,
                 sizeof(ded_finale_t));
}

int DED_AddDecoration(ded_t * ded)
{
    ded_decor_t *decor = DED_NewEntry((void **) &ded->decorations,
                                      &ded->count.decorations,
                                      sizeof(ded_decor_t));
    int     i;

    // Init some default values.
    for(i = 0; i < DED_DECOR_NUM_LIGHTS; i++)
    {
        // The color (0,0,0) means the light is not active.
        decor->lights[i].elevation = 1;
        decor->lights[i].radius = 1;
    }

    return decor - ded->decorations;
}

void DED_RemoveDecoration(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->decorations, &ded->count.decorations,
                 sizeof(ded_decor_t));
}

int DED_AddReflection(ded_t * ded)
{
    ded_reflection_t *ref = DED_NewEntry((void **) &ded->reflections,
                                         &ded->count.reflections,
                                         sizeof(ded_reflection_t));
    // Init to defaults.
    ref->shininess = 1.0f;
    ref->mask_width = 1.0f;
    ref->mask_height = 1.0f;
    ref->blend_mode = BM_ADD;

    return ref - ded->reflections;
}

void DED_RemoveReflection(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->reflections, &ded->count.reflections,
                 sizeof(ded_reflection_t));
}

int DED_AddGroup(ded_t * ded)
{
    ded_group_t *group = DED_NewEntry((void **) &ded->groups,
                                      &ded->count.groups, sizeof(ded_group_t));

    return group - ded->groups;
}

void DED_RemoveGroup(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->groups, &ded->count.groups,
                 sizeof(ded_group_t));
}

int DED_AddGroupMember(ded_group_t *grp)
{
    ded_group_member_t *memb = DED_NewEntry((void **) &grp->members,
                                         &grp->count,
                                         sizeof(ded_group_member_t));

    return memb - grp->members;
}

int DED_AddSector(ded_t * ded, int id)
{
    ded_sectortype_t *sec = DED_NewEntry((void **) &ded->sectors,
                                         &ded->count.sectors,
                                         sizeof(ded_sectortype_t));

    sec->id = id;
    return sec - ded->sectors;
}

void DED_RemoveSector(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->sectors, &ded->count.sectors,
                 sizeof(ded_sectortype_t));
}

int DED_AddLine(ded_t * ded, int id)
{
    ded_linetype_t *li = DED_NewEntry((void **) &ded->lines,
                                      &ded->count.lines,
                                      sizeof(ded_linetype_t));

    li->id = id;
    //li->act_count = -1;
    return li - ded->lines;
}

void DED_RemoveLine(ded_t * ded, int index)
{
    DED_DelEntry(index, (void **) &ded->lines, &ded->count.lines,
                 sizeof(ded_linetype_t));
}
