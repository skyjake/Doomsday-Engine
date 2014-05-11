/** @file defs/ded.h  Definition namespace.
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

#ifndef LIBDOOMSDAY_DEFINITION_DATABASE_H
#define LIBDOOMSDAY_DEFINITION_DATABASE_H

#include <vector>
#include <de/libcore.h>
#include <de/Vector>
#include "../uri.h"

#include "dedtypes.h"

// Version 6 does not require semicolons.
#define DED_VERSION                 6

/**
 * The ded_t structure encapsulates all the data one definition file can contain. This is
 * only an internal representation of the data. An ASCII version is written and read by
 * DED_Write() and DED_Read() routines.
 *
 * It is VERY important not to sort the data arrays in any way: the index numbers are
 * important. The Game DLL must be recompiled with the new constants if the order of the
 * array items changes.
 */
struct LIBDOOMSDAY_PUBLIC ded_s {
    int             version; // DED version number.
    ded_flags_t     modelFlags; // Default values for models.
    float           modelScale;
    float           modelOffset;

    struct ded_counts_s {
        ded_count_t     flags;
        ded_count_t     mobjs;
        ded_count_t     states;
        ded_count_t     sprites;
        ded_count_t     lights;
        ded_count_t     materials;
        //ded_count_t     models;
        ded_count_t     skies;
        ded_count_t     sounds;
        ded_count_t     music;
        ded_count_t     mapInfo;
        ded_count_t     text;
        ded_count_t     textureEnv;
        ded_count_t     values;
        ded_count_t     details;
        ded_count_t     ptcGens;
        ded_count_t     finales;
        ded_count_t     decorations;
        ded_count_t     reflections;
        ded_count_t     groups;
        ded_count_t     lineTypes;
        ded_count_t     sectorTypes;
        ded_count_t     xgClasses;
        ded_count_t     compositeFonts;
    } count;

    // Flag values (for all types of data).
    ded_flag_t*     flags;

    // Map object information.
    ded_mobj_t*     mobjs;

    // States.
    ded_state_t*    states;

    // Sprites.
    ded_sprid_t*    sprites;

    // Lights.
    ded_light_t*    lights;

    // Materials.
    ded_material_t* materials;

    // Models.
    typedef std::vector<ded_model_t> Models;
    Models models;

    // Skies.
    ded_sky_t*      skies;

    // Sounds.
    ded_sound_t*    sounds;

    // Music.
    ded_music_t*    music;

    // Map information.
    ded_mapinfo_t*  mapInfo;

    // Text.
    ded_text_t*     text;

    // Aural environments for textures.
    ded_tenviron_t* textureEnv;

    // Free-from string values.
    ded_value_t*    values;

    // Detail texture assignments.
    ded_detailtexture_t* details;

    // Particle generators.
    ded_ptcgen_t*   ptcGens;

    // Finales.
    ded_finale_t*   finales;

    // Decorations.
    ded_decor_t*    decorations;

    // Reflections.
    ded_reflection_t* reflections;

    // Animation/Precache groups for textures.
    ded_group_t*    groups;

    // XG line types.
    ded_linetype_t* lineTypes;

    // XG sector types.
    ded_sectortype_t* sectorTypes;

    // Composite fonts.
    ded_compositefont_t* compositeFonts;

	/// Constructor initializes everything to zero.
	ded_s() 
		: version(DED_VERSION),
		  modelFlags(0),
		  modelScale(0),
		  modelOffset(0),
		  flags(0),
		  mobjs(0),
		  states(0),
		  sprites(0),
		  lights(0),
		  materials(0),
		  skies(0),
		  sounds(0),
		  music(0),
		  mapInfo(0),
		  text(0),
		  textureEnv(0),
		  values(0),
		  details(0),
		  ptcGens(0),
		  finales(0),
		  decorations(0),
		  reflections(0),
		  groups(0),
		  lineTypes(0),
		  sectorTypes(0),
		  compositeFonts(0)
	{
		de::zap(count);
	}

    ded_flag_t *getFlag(char const *flag) const;

    int evalFlags2(char const *ptr) const;

    int getTextNumForName(const char* name) const;

    ded_material_t *findMaterialDef(de::Uri const &uri) const;

    ded_material_t *getMaterial(char const *uriCString) const;

    int getMobjNum(char const *id) const;

    int getMobjNumForName(char const *name) const;

    char const *getMobjName(int num) const;

    int getStateNum(char const *id) const;

    int getModelNum(char const *id) const;

    int getSoundNum(char const *id) const;

    /**
     * Looks up a sound using @a name key.
     * @param name  Sound name.
     * @return If the name is not found, returns the NULL sound index (zero).
     */
    int getSoundNumForName(const char* name) const;

    ded_music_t *getMusic(char const *id) const;

    int getMusicNum(const char* id) const;

    ded_value_t* getValueById(char const* id) const;

    ded_value_t* getValueByUri(de::Uri const &uri) const;

    ded_mapinfo_t *getMapInfo(de::Uri const *uri) const;

    ded_sky_t* getSky(char const* id) const;

    ded_compositefont_t* findCompositeFontDef(de::Uri const& uri) const;

    ded_compositefont_t* getCompositeFont(char const* uriCString) const;
};

typedef ded_s ded_t;

#ifdef __cplusplus
extern "C" {
#endif

// Routines for managing DED files.
void            DED_Init(ded_t* ded);
void            DED_Clear(ded_t* ded);

int             DED_AddFlag(ded_t* ded, char const* name, int value);
int             DED_AddMobj(ded_t* ded, char const* idStr);
int             DED_AddState(ded_t* ded, char const* id);
int             DED_AddSprite(ded_t* ded, char const* name);
int             DED_AddLight(ded_t* ded, char const* stateID);
int             DED_AddMaterial(ded_t* ded, char const* uri);
int             DED_AddMaterialLayerStage(ded_material_layer_t *ml);
int             DED_AddMaterialDecorationStage(ded_material_decoration_t *li);
int             DED_AddModel(ded_t* ded, char const* spr);
int             DED_AddSky(ded_t* ded, char const* id);
int             DED_AddSound(ded_t* ded, char const* id);
int             DED_AddMusic(ded_t* ded, char const* id);
int             DED_AddMapInfo(ded_t* ded, char const* uri);
int             DED_AddText(ded_t* ded, char const* id);
int             DED_AddTextureEnv(ded_t* ded, char const* id);
int             DED_AddValue(ded_t *ded, char const* id);
int             DED_AddDetail(ded_t* ded, char const* lumpname);
int             DED_AddPtcGen(ded_t* ded, char const* state);
int             DED_AddPtcGenStage(ded_ptcgen_t* gen);
int             DED_AddFinale(ded_t* ded);
int             DED_AddDecoration(ded_t* ded);
int             DED_AddReflection(ded_t* ded);
int             DED_AddGroup(ded_t* ded);
int             DED_AddGroupMember(ded_group_t* grp);
int             DED_AddSectorType(ded_t* ded, int id);
int             DED_AddLineType(ded_t* ded, int id);
int             DED_AddCompositeFont(ded_t* ded, char const* uri);
int             DED_AddCompositeFontMapCharacter(ded_compositefont_t* font);

void            DED_RemoveFlag(ded_t* ded, int index);
void            DED_RemoveMobj(ded_t* ded, int index);
void            DED_RemoveState(ded_t* ded, int index);
void            DED_RemoveSprite(ded_t* ded, int index);
void            DED_RemoveLight(ded_t* ded, int index);
void            DED_RemoveMaterial(ded_t* ded, int index);
void            DED_RemoveModel(ded_t* ded, int index);
void            DED_RemoveSky(ded_t* ded, int index);
void            DED_RemoveSound(ded_t* ded, int index);
void            DED_RemoveMusic(ded_t* ded, int index);
void            DED_RemoveMapInfo(ded_t* ded, int index);
void            DED_RemoveText(ded_t* ded, int index);
void            DED_RemoveTextureEnv(ded_t* ded, int index);
void            DED_RemoveValue(ded_t* ded, int index);
void            DED_RemoveDetail(ded_t* ded, int index);
void            DED_RemovePtcGen(ded_t* ded, int index);
void            DED_RemoveFinale(ded_t* ded, int index);
void            DED_RemoveDecoration(ded_t* ded, int index);
void            DED_RemoveReflection(ded_t* ded, int index);
void            DED_RemoveGroup(ded_t* ded, int index);
void            DED_RemoveSectorType(ded_t* ded, int index);
void            DED_RemoveLineType(ded_t* ded, int index);
void            DED_RemoveCompositeFont(ded_t* ded, int index);

/**
 * @return  Pointer to the new block of memory.
 */
void*           DED_NewEntries(void** ptr, ded_count_t* cnt, size_t elemSize, int count);

/**
 * @return  Pointer to the new block of memory.
 */
void*           DED_NewEntry(void** ptr, ded_count_t* cnt, size_t elemSize);

void            DED_DelEntry(int index, void** ptr, ded_count_t* cnt, size_t elemSize);
void            DED_DelArray(void** ptr, ded_count_t* cnt);
void            DED_ZCount(ded_count_t* c);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_DEFINITION_DATABASE_H
