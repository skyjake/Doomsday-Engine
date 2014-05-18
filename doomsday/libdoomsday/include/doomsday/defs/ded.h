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
#include <de/Record>
#include "../uri.h"

#include "dedtypes.h"
#include "dedregister.h"

// Version 6 does not require semicolons.
#define DED_VERSION 6

/**
 * The ded_t structure encapsulates all the data one definition file can contain. This is
 * only an internal representation of the data. An ASCII version is written and read by
 * DED_Write() and DED_Read() routines.
 *
 * It is VERY important not to sort the data arrays in any way: the index numbers are
 * important. The Game DLL must be recompiled with the new constants if the order of the
 * array items changes.
 */
struct LIBDOOMSDAY_PUBLIC ded_s
{
    de::Record names; ///< Namespace where definition values are stored.

    int version; // DED version number.

    ded_flags_t modelFlags; // Default values for models.
    float       modelScale;
    float       modelOffset;

    // Flag values (for all types of data).
    //DEDArray<ded_flag_t> flags;
    DEDRegister flags;

    // Map object information.
    DEDArray<ded_mobj_t> mobjs;

    // States.
    DEDArray<ded_state_t> states;

    // Sprites.
    DEDArray<ded_sprid_t> sprites;

    // Lights.
    DEDArray<ded_light_t> lights;

    // Materials.
    DEDArray<ded_material_t> materials;

    // Models.
    typedef std::vector<ded_model_t> Models;
    Models models;

    // Skies.
    DEDArray<ded_sky_t> skies;

    // Sounds.
    DEDArray<ded_sound_t> sounds;

    // Music.
    DEDArray<ded_music_t> music;

    // Map information.
    DEDArray<ded_mapinfo_t> mapInfo;

    // Text.
    DEDArray<ded_text_t> text;

    // Aural environments for textures.
    DEDArray<ded_tenviron_t> textureEnv;

    // Free-from string values.
    DEDArray<ded_value_t> values;

    // Detail texture assignments.
    DEDArray<ded_detailtexture_t> details;

    // Particle generators.
    DEDArray<ded_ptcgen_t> ptcGens;

    // Finales.
    DEDArray<ded_finale_t> finales;

    // Decorations.
    DEDArray<ded_decor_t> decorations;

    // Reflections.
    DEDArray<ded_reflection_t> reflections;

    // Animation/Precache groups for textures.
    DEDArray<ded_group_t> groups;

    // XG line types.
    DEDArray<ded_linetype_t> lineTypes;

    // XG sector types.
    DEDArray<ded_sectortype_t> sectorTypes;

    // Composite fonts.
    DEDArray<ded_compositefont_t> compositeFonts;

public:
    /**
     * Constructor initializes everything to zero.
     */
    ded_s();

    void clear();

    int addFlag(char const *id, int value);

    //ded_flag_t *getFlag(char const *flag) const;

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

protected:
    void release();

    DENG2_NO_ASSIGN(ded_s)
    DENG2_NO_COPY  (ded_s)
};

typedef ded_s ded_t;

#ifdef __cplusplus
extern "C" {
#endif

// Routines for managing DED files:

int             DED_AddMobj(ded_t* ded, char const* idStr);
int             DED_AddState(ded_t* ded, char const* id);
int             DED_AddSprite(ded_t* ded, char const* name);
int             DED_AddLight(ded_t* ded, char const* stateID);
LIBDOOMSDAY_PUBLIC int DED_AddMaterial(ded_t* ded, char const* uri);
LIBDOOMSDAY_PUBLIC int DED_AddMaterialLayerStage(ded_material_layer_t *ml);
int             DED_AddMaterialDecorationStage(ded_material_decoration_t *li);
int             DED_AddModel(ded_t* ded, char const* spr);
int             DED_AddSky(ded_t* ded, char const* id);
int             DED_AddSound(ded_t* ded, char const* id);
LIBDOOMSDAY_PUBLIC int DED_AddMusic(ded_t* ded, char const* id);
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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_DEFINITION_DATABASE_H
