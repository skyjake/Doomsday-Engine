/** @file ded.h  Definition namespace.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include <de/Record>
#include <de/String>
#include <de/Vector>
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
 * important. The game plugins must be recompiled with the new constants if the order of the
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
    DEDRegister flags;

    // Episodes.
    DEDRegister episodes;

    // Map object information.
    DEDRegister things; // DEDArray<ded_mobj_t> mobjs;

    // States.
    DEDRegister states;

    // Sprites.
    DEDArray<ded_sprid_t> sprites;

    // Lights.
    DEDArray<ded_light_t> lights;

    // Materials.
    DEDRegister materials;

    // Models.
    DEDRegister models;

    // Skies.
    DEDRegister skies;

    // Sounds.
    DEDArray<ded_sound_t> sounds;

    // Music.
    DEDRegister musics;

    // Map information.
    DEDRegister mapInfos;

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
    DEDRegister finales;

    // Decorations.
    DEDRegister decorations;

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

    int addFlag(de::String const &id, int value);

    int addEpisode();

    int addThing(de::String const &id);

    int addState(de::String const &id);

    int addDecoration();

    int addFinale();

    int addMapInfo();

    int addMaterial();

    int addModel();

    int addMusic();

    int addSky();

    int evalFlags(char const *ptr) const;

    int getEpisodeNum(de::String const &id) const;

    int getMapInfoNum(de::Uri const &uri) const;

    int getMaterialNum(de::Uri const &uri) const;

    int getMobjNum(de::String const &id) const;

    int getMobjNumForName(char const *name) const;

    de::String getMobjName(int num) const;

    int getModelNum(char const *id) const;

    int getMusicNum(char const *id) const;

    int getSkyNum(char const *id) const;

    int getSoundNum(char const *id) const;
    int getSoundNum(de::String const &id) const;

    /**
     * Looks up a sound using @a name key.
     * @param name  Sound name.
     * @return If the name is not found, returns the NULL sound index (zero).
     */
    int getSoundNumForName(char const *name) const;

    int getSpriteNum(char const *id) const;
    int getSpriteNum(de::String const &id) const;

    int getStateNum(char const *id) const;
    int getStateNum(de::String const &id) const;

    int getTextNum(char const *id) const;

    int getValueNum(char const *id) const;
    int getValueNum(de::String const &id) const;

    ded_value_t *getValueById(char const *id) const;
    ded_value_t *getValueById(de::String const &id) const;
    ded_value_t *getValueByUri(de::Uri const &uri) const;

    ded_compositefont_t *findCompositeFontDef(de::Uri const &uri) const;
    ded_compositefont_t *getCompositeFont(char const *uriCString) const;

    /**
     * Finds the episode that has a specific map in it.
     * @param mapId  Map identifier.
     * @return Episode ID.
     */
    de::String findEpisode(de::String const &mapId) const;

protected:
    void release();

    DENG2_NO_ASSIGN(ded_s)
    DENG2_NO_COPY  (ded_s)
};

typedef ded_s ded_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the main definitions database.
 */
LIBDOOMSDAY_PUBLIC ded_t *DED_Definitions();

LIBDOOMSDAY_PUBLIC void DED_DestroyDefinitions();

// Routines for managing DED files:

int             DED_AddSprite(ded_t* ded, char const* name);
int             DED_AddLight(ded_t* ded, char const* stateID);
int             DED_AddSound(ded_t* ded, char const* id);
int             DED_AddText(ded_t* ded, char const* id);
int             DED_AddTextureEnv(ded_t* ded, char const* id);
int             DED_AddValue(ded_t *ded, char const* id);
int             DED_AddDetail(ded_t* ded, char const* lumpname);
int             DED_AddPtcGen(ded_t* ded, char const* state);
int             DED_AddPtcGenStage(ded_ptcgen_t* gen);
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
