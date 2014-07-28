/** @file database.cpp  Doomsday Engine Definition database.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/ded.h"
#include "doomsday/defs/mapinfo.h"
#include "doomsday/defs/model.h"
#include "doomsday/defs/sky.h"

#include <de/ArrayValue>
#include <de/NumberValue>
#include <de/RecordValue>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <de/memory.h>
#include <de/strutil.h>

using namespace de;

float ded_ptcstage_t::particleRadius(int ptcIDX) const
{
    if(radiusVariance)
    {
        static float const rnd[16] = { .875f, .125f, .3125f, .75f, .5f, .375f,
            .5625f, .0625f, 1, .6875f, .625f, .4375f, .8125f, .1875f,
            .9375f, .25f
        };
        return (rnd[ptcIDX & 0xf] * radiusVariance +
                (1 - radiusVariance)) * radius;
    }
    return radius;
}

ded_s::ded_s()
    : flags   (names.addRecord("flags"))
    , mapInfos(names.addRecord("mapInfos"))
    , models  (names.addRecord("models"))
    , skies   (names.addRecord("skies"))
{
    flags.addLookupKey("id");
    mapInfos.addLookupKey("id");
    models.addLookupKey("id", DEDRegister::OnlyFirst);
    models.addLookupKey("state");
    skies.addLookupKey("id");

    clear();
}

void ded_s::clear()
{
    release();

    version = DED_VERSION;
    modelFlags = 0;
    modelScale = 0;
    modelOffset = 0;
}

int ded_s::addFlag(char const *id, int value)
{
    Record &def = flags.append();
    def.addText("id", id);
    def.addNumber("value", value);
    return def.geti("__order__");
}

int ded_s::addMapInfo()
{
    Record &def = mapInfos.append();
    defn::MapInfo(def).resetToDefaults();
    return def.geti("__order__");
}

int ded_s::addModel()
{
    Record &def = models.append();
    defn::Model(def).resetToDefaults();
    return def.geti("__order__");
}

int ded_s::addSky()
{
    Record &def = skies.append();
    defn::Sky(def).resetToDefaults();
    return def.geti("__order__");
}

void ded_s::release()
{
    flags.clear();
    mobjs.clear();
    states.clear();
    sprites.clear();
    lights.clear();
    models.clear();
    sounds.clear();
    music.clear();
    mapInfos.clear();
    skies.clear();
    details.clear();
    materials.clear();
    text.clear();
    textureEnv.clear();
    compositeFonts.clear();
    values.clear();
    decorations.clear();
    reflections.clear();
    groups.clear();
    sectorTypes.clear();
    lineTypes.clear();
    ptcGens.clear();
    finales.clear();
}

int DED_AddMobj(ded_t* ded, char const* idstr)
{
    ded_mobj_t *mo = ded->mobjs.append();
    strcpy(mo->id, idstr);
    return ded->mobjs.indexOf(mo);
}

#if 0
int DED_AddSky(ded_t* ded, char const* id)
{
    ded_sky_t *sky = ded->skies.append();
    int i;

    strcpy(sky->id, id);
    sky->height = DEFAULT_SKY_HEIGHT;
    for(i = 0; i < NUM_SKY_LAYERS; ++i)
    {
        sky->layers[i].offset = DEFAULT_SKY_SPHERE_XOFFSET;
        sky->layers[i].colorLimit = DEFAULT_SKY_SPHERE_FADEOUT_LIMIT;
    }
    for(i = 0; i < NUM_SKY_MODELS; ++i)
    {
        sky->models[i].frameInterval = 1;
        sky->models[i].color[0] = 1;
        sky->models[i].color[1] = 1;
        sky->models[i].color[2] = 1;
        sky->models[i].color[3] = 1;
    }

    return ded->skies.indexOf(sky);
}
#endif

int DED_AddState(ded_t* ded, char const* id)
{
    ded_state_t *st = ded->states.append();
    strcpy(st->id, id);
    return ded->states.indexOf(st);
}

int DED_AddSprite(ded_t* ded, char const* name)
{
    ded_sprid_t *sp = ded->sprites.append();
    strcpy(sp->id, name);
    return ded->sprites.indexOf(sp);
}

int DED_AddLight(ded_t* ded, char const* stateid)
{
    ded_light_t* light = ded->lights.append();
    strcpy(light->state, stateid);
    return ded->lights.indexOf(light);
}

int DED_AddMaterial(ded_t* ded, char const* uri)
{
    ded_material_t* mat = ded->materials.append();
    if(uri) mat->uri = new de::Uri(uri, RC_NULL);
    return ded->materials.indexOf(mat);
}

int DED_AddMaterialLayerStage(ded_material_layer_t *ml)
{
    ded_material_layer_stage_t *stage = ml->stages.append();
    return ml->stages.indexOf(stage);
}

int DED_AddMaterialDecorationStage(ded_material_decoration_t *li)
{
    ded_decorlight_stage_t *stage = li->stages.append();

    // The color (0,0,0) means the light is not visible during this stage.
    stage->elevation = 1;
    stage->radius    = 1;

    return li->stages.indexOf(stage);
}

int DED_AddSound(ded_t* ded, char const* id)
{
    ded_sound_t* snd = ded->sounds.append();
    strcpy(snd->id, id);
    return ded->sounds.indexOf(snd);
}

int DED_AddMusic(ded_t* ded, char const* id)
{
    ded_music_t* mus = ded->music.append();
    strcpy(mus->id, id);
    return ded->music.indexOf(mus);
}

#if 0
int DED_AddMapInfo(ded_t* ded, char const* uri)
{
    ded_mapinfo_t* inf = ded->mapInfo.append();
    int i;

    if(uri)
    {
        inf->uri = new de::Uri(uri, RC_NULL);
        if(inf->uri->scheme().isEmpty())
            inf->uri->setScheme("Maps");
    }
    inf->gravity = 1;
    inf->parTime = -1; // unknown

    inf->fogColor[0] = DEFAULT_FOG_COLOR_RED;
    inf->fogColor[1] = DEFAULT_FOG_COLOR_GREEN;
    inf->fogColor[2] = DEFAULT_FOG_COLOR_BLUE;
    inf->fogDensity = DEFAULT_FOG_DENSITY;
    inf->fogStart = DEFAULT_FOG_START;
    inf->fogEnd = DEFAULT_FOG_END;

    inf->sky.height = DEFAULT_SKY_HEIGHT;
    for(i = 0; i < NUM_SKY_LAYERS; ++i)
    {
        inf->sky.layers[i].offset = DEFAULT_SKY_SPHERE_XOFFSET;
        inf->sky.layers[i].colorLimit = DEFAULT_SKY_SPHERE_FADEOUT_LIMIT;
    }
    for(i = 0; i < NUM_SKY_MODELS; ++i)
    {
        inf->sky.models[i].frameInterval = 1;
        inf->sky.models[i].color[0] = 1;
        inf->sky.models[i].color[1] = 1;
        inf->sky.models[i].color[2] = 1;
        inf->sky.models[i].color[3] = 1;
    }

    return ded->mapInfo.indexOf(inf);
}
#endif

int DED_AddText(ded_t* ded, char const* id)
{
    ded_text_t* txt = ded->text.append();
    strcpy(txt->id, id);
    return ded->text.indexOf(txt);
}

int DED_AddTextureEnv(ded_t* ded, char const* id)
{
    ded_tenviron_t* env = ded->textureEnv.append();
    strcpy(env->id, id);
    return ded->textureEnv.indexOf(env);
}

int DED_AddCompositeFont(ded_t* ded, char const* uri)
{
    ded_compositefont_t* cfont = ded->compositeFonts.append();
    if(uri) cfont->uri = new de::Uri(uri, RC_NULL);
    return ded->compositeFonts.indexOf(cfont);
}

int DED_AddValue(ded_t* ded, char const* id)
{
    ded_value_t* val = ded->values.append();
    if(id)
    {
        val->id = (char *) M_Malloc(strlen(id) + 1);
        strcpy(val->id, id);
    }
    return ded->values.indexOf(val);
}

int DED_AddDetail(ded_t *ded, char const *lumpname)
{
    ded_detailtexture_t *dtl = ded->details.append();

    // Default usage is allowed with custom textures and external replacements.
    dtl->flags = DTLF_PWAD|DTLF_EXTERNAL;

    if(lumpname && lumpname[0])
    {
        dtl->stage.texture = new de::Uri(lumpname, RC_NULL);
    }
    dtl->stage.scale = 1;
    dtl->stage.strength = 1;

    return ded->details.indexOf(dtl);
}

int DED_AddPtcGen(ded_t* ded, char const* state)
{
    ded_ptcgen_t* gen = ded->ptcGens.append();

    strcpy(gen->state, state);

    // Default choice (use either submodel zero or one).
    gen->subModel = -1;

    return ded->ptcGens.indexOf(gen);
}

int DED_AddPtcGenStage(ded_ptcgen_t* gen)
{
    ded_ptcstage_t* stage = gen->stages.append();

    stage->model = -1;
    stage->sound.volume = 1;
    stage->hitSound.volume = 1;

    return gen->stages.indexOf(stage);
}

int DED_AddFinale(ded_t* ded)
{
    ded_finale_t* fin = ded->finales.append();
    return ded->finales.indexOf(fin);
}

int DED_AddDecoration(ded_t* ded)
{
    ded_decor_t* decor = ded->decorations.append();
    for(int i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
    {
        // The color (0,0,0) means the light is not active.
        decor->lights[i].stage.elevation = 1;
        decor->lights[i].stage.radius    = 1;
    }
    return ded->decorations.indexOf(decor);
}

int DED_AddReflection(ded_t *ded)
{
    ded_reflection_t *ref = ded->reflections.append();

    // Default usage is allowed with custom textures and external replacements.
    ref->flags = REFF_PWAD|REFF_EXTERNAL;

    // Init to defaults.
    ref->stage.shininess  = 1.0f;
    ref->stage.blendMode  = BM_ADD;
    ref->stage.maskWidth  = 1.0f;
    ref->stage.maskHeight = 1.0f;

    return ded->reflections.indexOf(ref);
}

int DED_AddGroup(ded_t* ded)
{
    ded_group_t* group = ded->groups.append();
    return ded->groups.indexOf(group);
}

int DED_AddGroupMember(ded_group_t* grp)
{
    ded_group_member_t* memb = grp->members.append();
    return grp->members.indexOf(memb);
}

int DED_AddSectorType(ded_t* ded, int id)
{
    ded_sectortype_t* sec = ded->sectorTypes.append();
    sec->id = id;
    return ded->sectorTypes.indexOf(sec);
}

int DED_AddLineType(ded_t* ded, int id)
{
    ded_linetype_t* li = ded->lineTypes.append();
    li->id = id;
    //li->actCount = -1;
    return ded->lineTypes.indexOf(li);
}

ded_material_t *ded_s::findMaterialDef(de::Uri const &uri) const
{
    for(int i = materials.size() - 1; i >= 0; i--)
    {
        ded_material_t *def = &materials[i];
        if(def->uri && uri == *def->uri)
        {
            return def;
        }
    }
    return 0; // Not found.
}

ded_material_t* ded_s::getMaterial(char const *uriCString) const
{
    ded_material_t* def = NULL;

    if(uriCString && uriCString[0])
    {
        de::Uri uri(uriCString, RC_NULL);

        if(uri.scheme().isEmpty())
        {
            // Caller doesn't care which scheme - use a priority search order.
            de::Uri temp(uri);

            temp.setScheme("Sprites");
            def = findMaterialDef(temp);
            if(!def)
            {
                temp.setScheme("Textures");
                def = findMaterialDef(temp);
            }
            if(!def)
            {
                temp.setScheme("Flats");
                def = findMaterialDef(temp);
            }
        }

        if(!def)
        {
            def = findMaterialDef(uri);
        }
    }

    return def;
}

int ded_s::getMobjNum(char const *id) const
{
    int i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < mobjs.size(); ++i)
        if(!qstricmp(mobjs[i].id, id))
            return i;

    return -1;
}

int ded_s::getMobjNumForName(const char *name) const
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = mobjs.size() - 1; i >= 0; --i)
        if(!qstricmp(mobjs[i].name, name))
            return i;

    return -1;
}

char const *ded_s::getMobjName(int num) const
{
    if(num < 0) return "(<0)";
    if(num >= mobjs.size()) return "(>mobjtypes)";
    return mobjs[num].id;
}

int ded_s::getStateNum(String const &id) const
{
    return getStateNum(id.toLatin1().constData());
}

int ded_s::getStateNum(char const *id) const
{
    int idx = -1;
    if(id && id[0] && states.size())
    {
        int i = 0;
        do {
            if(!qstricmp(states[i].id, id))
                idx = i;
        } while(idx == -1 && ++i < states.size());
    }
    return idx;
}

/*
ded_flag_t *ded_s::getFlag(char const *flag) const
{
    if(!flag || !flag[0]) return 0;

    for(int i = flags.size() - 1; i >= 0; i--)
    {
        if(!qstricmp(flags[i].id, flag))
            return &flags[i];
    }

    return 0;
}*/

int ded_s::evalFlags2(char const *ptr) const
{
    LOG_AS("Def_EvalFlags");

    int value = 0;

    while(*ptr)
    {
        ptr = M_SkipWhite(const_cast<char *>(ptr));

        int flagNameLength = M_FindWhite(const_cast<char *>(ptr)) - ptr;
        String flagName(ptr, flagNameLength);
        ptr += flagNameLength;

        if(Record const *flag = flags.tryFind("id", flagName.toLower()))
        {
            value |= flag->geti("value");
        }
        else
        {
            LOG_RES_WARNING("Flag '%s' is not defined (or used out of context)") << flagName;
        }
    }
    return value;
}

int ded_s::getMapInfoNum(de::Uri const &uri) const
{
    if(Record const *def = mapInfos.tryFind("id", uri.compose()))
    {
        return def->geti("__order__");
    }
    return -1;

    /*for(int i = mapInfo.size() - 1; i >= 0; i--)
    {
        if(mapInfo[i].uri && *uri == *mapInfo[i].uri)
            return i;
    }
    return -1;*/
}

int ded_s::getModelNum(const char *id) const
{
    if(Record const *def = models.tryFind("id", id))
    {
        return def->geti("__order__");
    }
    return -1;

/*    int idx = -1;
    if(id && id[0] && !models.empty())
    {
        int i = 0;
        do {
            if(!qstricmp(models[i].id, id)) idx = i;
        } while(idx == -1 && ++i < (int)models.size());
    }
    return idx;*/
}

int ded_s::getSkyNum(char const *id) const
{
    if(Record const *def = skies.tryFind("id", id))
    {
        return def->geti("__order__");
    }
    return -1;

    /*if(!id || !id[0]) return -1;

    for(int i = skies.size() - 1; i >= 0; i--)
    {
        if(!qstricmp(skies[i].id, id))
            return i;
    }
    return -1;*/
}

int ded_s::getSoundNum(const char *id) const
{
    int idx = -1;
    if(id && id[0] && sounds.size())
    {
        int i = 0;
        do {
            if(!qstricmp(sounds[i].id, id)) idx = i;
        } while(idx == -1 && ++i < sounds.size());
    }
    return idx;
}

int ded_s::getSoundNumForName(const char *name) const
{
    if(!name || !name[0])
        return -1;

    for(int i = 0; i < sounds.size(); ++i)
        if(!qstricmp(sounds[i].name, name))
            return i;

    return 0;
}

ded_music_t *ded_s::getMusic(char const *id) const
{
    if(id && id[0] && music.size())
    {
        for(int i = 0; i < music.size(); ++i)
        {
            if(!qstricmp(music[i].id, id))
                return &music[i];
        }
    }
    return 0;
}

int ded_s::getMusicNum(const char* id) const
{
    int idx = -1;
    if(id && id[0] && music.size())
    {
        int i = 0;
        do {
            if(!qstricmp(music[i].id, id)) idx = i;
        } while(idx == -1 && ++i < music.size());
    }
    return idx;
}

ded_value_t* ded_s::getValueById(char const* id) const
{
    if(!id || !id[0]) return NULL;

    // Read backwards to allow patching.
    for(int i = values.size() - 1; i >= 0; i--)
    {
        if(!qstricmp(values[i].id, id))
            return &values[i];
    }
    return 0;
}

ded_value_t* ded_s::getValueByUri(de::Uri const &uri) const
{
    if(uri.scheme().compareWithoutCase("Values")) return 0;
    return getValueById(uri.pathCStr());
}

ded_compositefont_t* ded_s::findCompositeFontDef(de::Uri const& uri) const
{
    for(int i = compositeFonts.size() - 1; i >= 0; i--)
    {
        ded_compositefont_t* def = &compositeFonts[i];
        if(!def->uri || uri != *def->uri) continue;
        return def;
    }
    return 0;
}

ded_compositefont_t* ded_s::getCompositeFont(char const* uriCString) const
{
    ded_compositefont_t* def = NULL;
    if(uriCString && uriCString[0])
    {
        de::Uri uri(uriCString, RC_NULL);

        if(uri.scheme().isEmpty())
        {
            // Caller doesn't care which scheme - use a priority search order.
            de::Uri temp(uri);

            temp.setScheme("Game");
            def = findCompositeFontDef(temp);
            if(!def)
            {
                temp.setScheme("System");
                def = findCompositeFontDef(temp);
            }
        }

        if(!def)
        {
            def = findCompositeFontDef(uri);
        }
    }
    return def;
}

int ded_s::getTextNumForName(const char* name) const
{
    int idx = -1;
    if(name && name[0] && text.size())
    {
        int i = 0;
        do
        {
            if(!qstricmp(text[i].id, name))
                idx = i;
        } while(idx == -1 && ++i < text.size());
    }
    return idx;
}
