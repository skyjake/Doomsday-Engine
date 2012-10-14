/**
 * @file def_main.h
 * Definitions subsystem. @ingroup defs
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_DEFINITIONS_MAIN_H
#define LIBDENG_DEFINITIONS_MAIN_H

#include "def_data.h"
#include "stringarray.h"

#ifdef __cplusplus
extern "C" {
#endif

// the actual classes are game-side
struct xgclass_s;

typedef struct sfxinfo_s {
    void* data; /// Pointer to sound data.
    lumpnum_t lumpNum;
    char lumpName[9]; /// Actual lump name of the sound (full name).
    char id[32]; /// Identifier name (from the def).
    char name[32]; /// Long name.
    struct sfxinfo_s* link; /// Link to another sound.
    int linkPitch;
    int linkVolume;
    int priority;
    int channels; /// Max. channels for the sound to occupy.
    int usefulness; /// Used to determine when to cache out.
    int flags;
    int group;
    ddstring_t external; /// Path to external file.
} sfxinfo_t;

extern ded_t defs; // The main definitions database.
extern sprname_t* sprNames; // Sprite name list.
extern state_t* states; // State list.
extern mobjinfo_t* mobjInfo; // Map object info database.
extern sfxinfo_t* sounds; // Sound effect list.
extern ddtext_t* texts; // Text list.
extern mobjinfo_t** stateOwners;
extern ded_light_t** stateLights;
extern ded_ptcgen_t** statePtcGens;
extern ded_count_t countSprNames;
extern ded_count_t countStates;
extern struct xgclass_s* xgClassLinks;

void            Def_Init(void);
int             Def_GetGameClasses(void);

/**
 * Finish definition database initialization. Initialization is split into two
 * phases either side of the texture manager, this being the post-phase.
 */
void            Def_PostInit(void);

/**
 * Destroy databases.
 */
void            Def_Destroy(void);

/**
 * Reads the specified definition files, and creates the sprite name,
 * state, mobjinfo, sound, music, text and mapinfo databases accordingly.
 */
void            Def_Read(void);
void            Def_ReadProcessDED(const char* fileName);

int             Def_GetMobjNum(const char* id);
int             Def_GetMobjNumForName(const char* name);
const char*     Def_GetMobjName(int num);
int             Def_GetStateNum(const char* id);
const char*     Def_GetStateName(state_t* state);
int             Def_GetActionNum(const char* id);
int             Def_GetSpriteNum(const char* name);
int             Def_GetModelNum(const char* id);
int             Def_GetMusicNum(const char* id);
int             Def_GetSoundNum(const char* id);
ded_flag_t*     Def_GetFlag(const char* id);
ded_value_t*    Def_GetValueById(const char* id);
ded_value_t*    Def_GetValueByUri(const Uri* uri);
ded_mapinfo_t*  Def_GetMapInfo(const Uri* uri);
ded_sky_t*      Def_GetSky(const char* id);
ded_material_t* Def_GetMaterial(const char* uri);
ded_compositefont_t* Def_GetCompositeFont(const char* uri);
ded_light_t*    Def_GetLightDef(int spr, int frame);
ded_decor_t*    Def_GetDecoration(materialid_t matId, boolean hasExternal, boolean isCustom);
ded_reflection_t* Def_GetReflection(materialid_t matId, boolean hasExternal, boolean isCustom);
ded_detailtexture_t* Def_GetDetailTex(materialid_t matId, boolean hasExternal, boolean isCustom);
ded_ptcgen_t*   Def_GetGenerator(materialid_t matId, boolean hasExternal, boolean isCustom);
ded_ptcgen_t*   Def_GetDamageGenerator(int mobjType);

int             Def_EvalFlags(char* string);

/**
 * @return  @c true= the definition was found.
 */
int Def_Get(int type, const char* id, void* out);

/**
 * This is supposed to be the main interface for outside parties to
 * modify definitions (unless they want to do it manually with dedfile.h).
 */
int Def_Set(int type, int index, int value, const void* ptr);

boolean Def_SameStateSequence(state_t* snew, state_t* sold);

/**
 * Compiles a list of all the defined mobj types. Indices in this list
 * match those in the @c mobjInfo array.
 *
 * @return StringArray instance. Caller gets ownership.
 */
StringArray* Def_ListMobjTypeIDs(void);

/**
 * Compiles a list of all the defined mobj states. Indices in this list
 * match those in the @c states array.
 *
 * @return StringArray instance. Caller gets ownership.
 */
StringArray* Def_ListStateIDs(void);

D_CMD(ListMobjs);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DEFINITIONS_MAIN_H */
